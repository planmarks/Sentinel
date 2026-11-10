/* Polled USB FS device + CDC-ACM for the STM32L4 USB peripheral (RM0394).
 * One control EP (EP0), one bulk pair (EP1 IN/OUT) for CDC data, one interrupt
 * IN (EP2) for CDC notifications. No interrupts — driven by usb_poll(). */
#include "board.h"
#include "usbfs.h"
#include <string.h>

/* ---- EPnR bit layout (same across all STM32 USB FS) ---- */
#define EP_CTR_RX   (1u<<15)
#define EP_DTOG_RX  (1u<<14)
#define EP_STAT_RX  (3u<<12)
#define EP_SETUP    (1u<<11)
#define EP_TYPE     (3u<<9)
#define EP_KIND     (1u<<8)
#define EP_CTR_TX   (1u<<7)
#define EP_DTOG_TX  (1u<<6)
#define EP_STAT_TX  (3u<<4)
#define EP_EA       (0x0fu)
#define EP_TYPE_BULK      (0u<<9)
#define EP_TYPE_CONTROL   (1u<<9)
#define EP_TYPE_INTERRUPT (3u<<9)
#define STAT_DISABLED 0u
#define STAT_STALL    1u
#define STAT_NAK      2u
#define STAT_VALID    3u

#define EPR(ep)   (*(volatile uint16_t *)(USB_BASE + (ep)*4u))
#define EP_INVARIANT (EP_CTR_RX|EP_SETUP|EP_TYPE|EP_KIND|EP_CTR_TX|EP_EA)

static void ep_set_tx(int ep, uint16_t stat) {
  uint16_t v = EPR(ep);
  uint16_t tog = (v & EP_STAT_TX) ^ ((stat<<4) & EP_STAT_TX);
  EPR(ep) = (v & EP_INVARIANT) | EP_CTR_RX | EP_CTR_TX | tog;
}
static void ep_set_rx(int ep, uint16_t stat) {
  uint16_t v = EPR(ep);
  uint16_t tog = (v & EP_STAT_RX) ^ ((stat<<12) & EP_STAT_RX);
  EPR(ep) = (v & EP_INVARIANT) | EP_CTR_RX | EP_CTR_TX | tog;
}
static void ep_clr_ctr_rx(int ep) { EPR(ep) = ((EPR(ep) & EP_INVARIANT) | EP_CTR_TX) & ~EP_CTR_RX; }
static void ep_clr_ctr_tx(int ep) { EPR(ep) = ((EPR(ep) & EP_INVARIANT) | EP_CTR_RX) & ~EP_CTR_TX; }

/* ---- packet memory (1:1 16-bit access on L4) ---- */
#define PMA(off) (*(volatile uint16_t *)(USB_PMAADDR + (off)))
/* BTABLE at PMA 0: per-EP {ADDR_TX, COUNT_TX, ADDR_RX, COUNT_RX} */
#define BT_ADDR_TX(ep)  PMA((ep)*8u + 0u)
#define BT_COUNT_TX(ep) PMA((ep)*8u + 2u)
#define BT_ADDR_RX(ep)  PMA((ep)*8u + 4u)
#define BT_COUNT_RX(ep) PMA((ep)*8u + 6u)

/* PMA buffer offsets */
#define PMA_EP0_TX 0x20u
#define PMA_EP0_RX 0x60u
#define PMA_EP1_TX 0xA0u
#define PMA_EP1_RX 0xE0u
#define PMA_EP2_TX 0x120u
#define EP0_SIZE   64u
#define EP1_SIZE   64u

static void pma_write(uint16_t off, const uint8_t *src, uint16_t n) {
  for (uint16_t i = 0; i < n; i += 2) {
    uint16_t w = src[i];
    if (i + 1 < n) w |= (uint16_t)src[i+1] << 8;
    PMA(off + i) = w;
  }
}
static void pma_read(uint8_t *dst, uint16_t off, uint16_t n) {
  for (uint16_t i = 0; i < n; i += 2) {
    uint16_t w = PMA(off + i);
    dst[i] = (uint8_t)w;
    if (i + 1 < n) dst[i+1] = (uint8_t)(w >> 8);
  }
}
static uint16_t count_rx_bits(uint16_t size) {   /* BL_SIZE/NUM_BLOCK encoding */
  if (size > 62) return 0x8000u | ((((size + 31u)/32u) - 1u) << 10);
  return ((size + 1u)/2u) << 10;
}

/* ---- descriptors ---- */
static const uint8_t desc_device[18] = {
  18,1, 0x00,0x02, 0x02,0,0, EP0_SIZE,
  0x83,0x04, 0x40,0x57, 0x00,0x02, 1,2,3, 1
};
static const uint8_t desc_config[67] = {
  9,2, 67,0, 2, 1, 0, 0xC0, 0x32,
  /* IF0 CDC comm */        9,4, 0,0, 1, 0x02,0x02,0x01, 0,
  /* CDC header */          5,0x24,0x00, 0x10,0x01,
  /* CDC call mgmt */       5,0x24,0x01, 0x00, 1,
  /* CDC ACM */             4,0x24,0x02, 0x02,
  /* CDC union */           5,0x24,0x06, 0, 1,
  /* EP2 IN interrupt */    7,5, 0x82, 0x03, 16,0, 0xFF,
  /* IF1 CDC data */        9,4, 1,0, 2, 0x0A,0,0, 0,
  /* EP1 OUT bulk */        7,5, 0x01, 0x02, EP1_SIZE,0, 0,
  /* EP1 IN bulk */         7,5, 0x81, 0x02, EP1_SIZE,0, 0,
};
static const uint8_t str0[4]  = { 4,3, 0x09,0x04 };
static const uint8_t str1[10] = { 10,3, 'M',0,'u',0,'r',0,'u',0 };
static const uint8_t str2[38] = { 38,3, 'M',0,'u',0,'r',0,'u',0,' ',0,'V',0,'a',0,'u',0,'l',0,'t',0,
                                  ' ',0,'U',0,'p',0,'d',0,'a',0,'t',0,'e',0,'r',0 };
static const uint8_t str3[10] = { 10,3, '0',0,'0',0,'0',0,'1',0 };

/* ---- state ---- */
static uint8_t  g_configured;
static uint8_t  g_pending_addr;
static uint8_t  g_out_data;      /* expecting a control-write data stage (e.g. SET_LINE_CODING) */
static const uint8_t *g_in_ptr; static uint16_t g_in_rem;   /* EP0 IN data stage */
static uint8_t  g_rxbuf[512]; static volatile uint16_t g_rxh, g_rxt;
static uint8_t  cdc_line[7] = { 0x00,0xC2,0x01,0,0,0,8 };   /* 115200 8N1 */

static void ep0_send(const uint8_t *p, uint16_t len, uint16_t wlen) {
  if (len > wlen) len = wlen;
  g_in_ptr = p; g_in_rem = len;
  uint16_t n = (len > EP0_SIZE) ? EP0_SIZE : len;
  pma_write(PMA_EP0_TX, g_in_ptr, n);
  BT_COUNT_TX(0) = n;
  g_in_ptr += n; g_in_rem -= n;
  ep_set_tx(0, STAT_VALID);
}
static void ep0_send_zlp(void) { BT_COUNT_TX(0) = 0; ep_set_tx(0, STAT_VALID); }

static void handle_setup(const uint8_t *s) {
  uint8_t  bmReq = s[0], bReq = s[1];
  uint16_t wVal = s[2] | (s[3]<<8), wLen = s[6] | (s[7]<<8);
  uint8_t  type = (bmReq >> 5) & 3;   /* 0 std, 1 class */

  if (type == 0) {   /* standard */
    switch (bReq) {
      case 6: {      /* GET_DESCRIPTOR */
        uint8_t dt = wVal >> 8, di = wVal & 0xFF;
        if (dt == 1) ep0_send(desc_device, sizeof desc_device, wLen);
        else if (dt == 2) ep0_send(desc_config, sizeof desc_config, wLen);
        else if (dt == 3) {
          const uint8_t *p = 0; uint16_t l = 0;
          if (di==0){p=str0;l=sizeof str0;} else if(di==1){p=str1;l=sizeof str1;}
          else if(di==2){p=str2;l=sizeof str2;} else if(di==3){p=str3;l=sizeof str3;}
          if (p) ep0_send(p, l, wLen); else ep_set_tx(0, STAT_STALL);
        } else ep_set_tx(0, STAT_STALL);
        break; }
      case 5: g_pending_addr = wVal & 0x7F; ep0_send_zlp(); break;  /* SET_ADDRESS (apply after status) */
      case 9: g_configured = (wVal != 0); ep0_send_zlp(); break;    /* SET_CONFIGURATION */
      case 8: { uint8_t c = g_configured; ep0_send(&c,1,wLen); break; } /* GET_CONFIGURATION */
      case 0: { uint8_t st[2]={0,0}; ep0_send(st,2,wLen); break; }   /* GET_STATUS */
      default: ep_set_tx(0, STAT_STALL); break;
    }
  } else if (type == 1) {   /* CDC class */
    switch (bReq) {
      case 0x20: /* SET_LINE_CODING: a 7-byte OUT data stage follows; ack it below */ g_out_data = 1; ep_set_rx(0, STAT_VALID); break;
      case 0x21: ep0_send(cdc_line, 7, wLen); break;   /* GET_LINE_CODING */
      case 0x22: ep0_send_zlp(); break;                /* SET_CONTROL_LINE_STATE */
      default:   ep0_send_zlp(); break;
    }
  } else ep_set_tx(0, STAT_STALL);
}

static void ep0_ctr(void) {
  uint16_t epr = EPR(0);
  if (epr & EP_CTR_RX) {
    if (epr & EP_SETUP) {
      uint8_t s[8]; pma_read(s, PMA_EP0_RX, 8);
      ep_clr_ctr_rx(0);
      handle_setup(s);
    } else {
      ep_clr_ctr_rx(0);
      if (g_out_data) {     /* data stage of a control write -> complete with status ZLP */
        uint16_t cnt = BT_COUNT_RX(0) & 0x3FF;
        if (cnt <= sizeof cdc_line) pma_read(cdc_line, PMA_EP0_RX, cnt);
        g_out_data = 0;
        ep0_send_zlp();     /* IN status stage */
      }
      /* else: this is the OUT status stage of a control read -> just ack */
    }
    ep_set_rx(0, STAT_VALID);
  }
  if (epr & EP_CTR_TX) {
    ep_clr_ctr_tx(0);
    if (g_pending_addr) { USB->DADDR = USB_DADDR_EF | g_pending_addr; g_pending_addr = 0; }
    if (g_in_rem) {       /* continue multi-packet IN */
      uint16_t n = (g_in_rem > EP0_SIZE) ? EP0_SIZE : g_in_rem;
      pma_write(PMA_EP0_TX, g_in_ptr, n);
      BT_COUNT_TX(0) = n; g_in_ptr += n; g_in_rem -= n;
      ep_set_tx(0, STAT_VALID);
    }
  }
}

static void ep1_ctr(void) {
  uint16_t epr = EPR(1);
  if (epr & EP_CTR_RX) {           /* data from host */
    uint16_t n = BT_COUNT_RX(1) & 0x3FF;
    uint8_t tmp[EP1_SIZE];
    pma_read(tmp, PMA_EP1_RX, n);
    ep_clr_ctr_rx(1);
    for (uint16_t i = 0; i < n; i++) {
      uint16_t nh = (uint16_t)((g_rxh + 1) % sizeof g_rxbuf);
      if (nh != g_rxt) { g_rxbuf[g_rxh] = tmp[i]; g_rxh = nh; }
    }
    ep_set_rx(1, STAT_VALID);
  }
  if (epr & EP_CTR_TX) ep_clr_ctr_tx(1);
}

static void bus_reset(void) {
  USB->BTABLE = 0;
  /* EP0 control */
  BT_ADDR_TX(0) = PMA_EP0_TX; BT_COUNT_TX(0) = 0;
  BT_ADDR_RX(0) = PMA_EP0_RX; BT_COUNT_RX(0) = count_rx_bits(EP0_SIZE);
  EPR(0) = EP_TYPE_CONTROL | 0;            /* EA=0 */
  ep_set_rx(0, STAT_VALID); ep_set_tx(0, STAT_NAK);
  /* EP1 bulk */
  BT_ADDR_TX(1) = PMA_EP1_TX; BT_COUNT_TX(1) = 0;
  BT_ADDR_RX(1) = PMA_EP1_RX; BT_COUNT_RX(1) = count_rx_bits(EP1_SIZE);
  EPR(1) = EP_TYPE_BULK | 1;
  ep_set_rx(1, STAT_VALID); ep_set_tx(1, STAT_NAK);
  /* EP2 interrupt IN */
  BT_ADDR_TX(2) = PMA_EP2_TX; BT_COUNT_TX(2) = 0;
  EPR(2) = EP_TYPE_INTERRUPT | 2;
  ep_set_tx(2, STAT_NAK);
  g_configured = 0; g_pending_addr = 0; g_rxh = g_rxt = 0;
  USB->DADDR = USB_DADDR_EF | 0;
}

void usb_init(void) {
  USB->CNTR = USB_CNTR_FRES;          /* assert reset, exit power-down */
  for (volatile int i = 0; i < 1000; i++) {}
  USB->CNTR = 0;                      /* release reset */
  USB->ISTR = 0;
  USB->BTABLE = 0;
  USB->DADDR = 0;
  bus_reset();
  USB->DADDR = USB_DADDR_EF;
  /* BCDR DPPU: enable the internal D+ pull-up to signal connect. */
  USB->BCDR |= USB_BCDR_DPPU;
}

void usb_deinit(void) {
  USB->BCDR &= ~USB_BCDR_DPPU;
  USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
}

void usb_poll(void) {
  uint16_t istr = USB->ISTR;
  if (istr & USB_ISTR_RESET) { USB->ISTR = (uint16_t)~USB_ISTR_RESET; bus_reset(); return; }
  if (istr & USB_ISTR_CTR) {
    uint8_t ep = istr & USB_ISTR_EP_ID;
    if (ep == 0) ep0_ctr();
    else if (ep == 1) ep1_ctr();
    else { EPR(ep) = EPR(ep) & EP_INVARIANT; }  /* clear stray */
  }
  /* clear the sticky non-CTR flags we don't use */
  USB->ISTR = (uint16_t)~(USB_ISTR_SOF | USB_ISTR_ESOF | USB_ISTR_SUSP | USB_ISTR_WKUP | USB_ISTR_PMAOVR | USB_ISTR_ERR);
}

bool usb_cdc_configured(void) { return g_configured != 0; }

int usb_cdc_recv(uint8_t *buf, int max) {
  int n = 0;
  while (n < max && g_rxt != g_rxh) {
    buf[n++] = g_rxbuf[g_rxt];
    g_rxt = (uint16_t)((g_rxt + 1) % sizeof g_rxbuf);
  }
  return n;
}

int usb_cdc_send(const uint8_t *buf, int len) {
  if (!g_configured) return 0;
  if (len > (int)EP1_SIZE) len = EP1_SIZE;
  /* wait (bounded) for the IN endpoint to be free */
  for (int t = 0; t < 100000; t++) {
    if ((EPR(1) & EP_STAT_TX) != (STAT_VALID<<4)) break;
    usb_poll();
  }
  pma_write(PMA_EP1_TX, buf, (uint16_t)len);
  BT_COUNT_TX(1) = (uint16_t)len;
  ep_set_tx(1, STAT_VALID);
  return len;
}
