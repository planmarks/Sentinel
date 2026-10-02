/* STM32L4 flash storage: 2 KB page erase + 64-bit doubleword programming. */
#include "board.h"
#include "flash_store.h"
#include <string.h>

#define FLASH_KEY1 0x45670123u
#define FLASH_KEY2 0xCDEF89ABu
#define PAGE_SIZE  2048u

#define STORE_BASE 0x0803E000u
#define AUTH_BASE  0x0803E000u
#define CFG_BASE   0x0803E800u
#define VAULT_BASE 0x0803F000u

static uint32_t region_base(StoreRegion r) {
  switch (r) { case REGION_AUTH: return AUTH_BASE;
               case REGION_CFG:  return CFG_BASE;
               default:          return VAULT_BASE; }
}
static uint32_t region_pages(StoreRegion r) { return (r == REGION_VAULT) ? 2u : 1u; }

const uint8_t *store_ptr(StoreRegion r)  { return (const uint8_t *)region_base(r); }
size_t         store_size(StoreRegion r) { return region_pages(r) * PAGE_SIZE; }

static void flash_unlock(void) {
  if (FLASH->CR & FLASH_CR_LOCK) { FLASH->KEYR = FLASH_KEY1; FLASH->KEYR = FLASH_KEY2; }
}
static void flash_lock(void) { FLASH->CR |= FLASH_CR_LOCK; }

static void flash_wait(void) { while (FLASH->SR & FLASH_SR_BSY) {} }

static void flash_clear_errors(void) {
  FLASH->SR = FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR
            | FLASH_SR_SIZERR | FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR
            | FLASH_SR_RDERR | FLASH_SR_EOP;
}

static bool erase_page(uint32_t page) {
  flash_wait();
  flash_clear_errors();
  FLASH->CR &= ~FLASH_CR_PNB;
  FLASH->CR |= FLASH_CR_PER | (page << FLASH_CR_PNB_Pos);
  FLASH->CR |= FLASH_CR_STRT;
  flash_wait();
  FLASH->CR &= ~FLASH_CR_PER;
  return (FLASH->SR & (FLASH_SR_PROGERR|FLASH_SR_WRPERR|FLASH_SR_PGAERR|FLASH_SR_PGSERR)) == 0;
}

static bool program_dword(uint32_t addr, uint32_t lo, uint32_t hi) {
  flash_wait();
  flash_clear_errors();
  FLASH->CR |= FLASH_CR_PG;
  *(volatile uint32_t *)(addr)     = lo;
  *(volatile uint32_t *)(addr + 4) = hi;
  flash_wait();
  bool ok = (FLASH->SR & (FLASH_SR_PROGERR|FLASH_SR_WRPERR|FLASH_SR_PGAERR|FLASH_SR_PGSERR)) == 0;
  FLASH->CR &= ~FLASH_CR_PG;
  return ok;
}

bool store_erase(StoreRegion r) {
  uint32_t base = region_base(r);
  uint32_t first = (base - 0x08000000u) / PAGE_SIZE;
  bool ok = true;
  /* Mask interrupts: erasing bank 1 stalls fetches, so no ISR may run from
   * flash mid-operation. */
  __disable_irq();
  flash_unlock();
  for (uint32_t i = 0; i < region_pages(r); i++) ok = erase_page(first + i) && ok;
  flash_lock();
  __enable_irq();
  return ok;
}

bool store_write(StoreRegion r, const void *data, size_t len) {
  if (len > store_size(r)) return false;
  uint32_t base = region_base(r);
  uint32_t first = (base - 0x08000000u) / PAGE_SIZE;
  const uint8_t *src = (const uint8_t *)data;

  __disable_irq();
  flash_unlock();
  bool ok = true;
  for (uint32_t i = 0; i < region_pages(r); i++) ok = erase_page(first + i) && ok;

  size_t nd = (len + 7) / 8;   /* number of doublewords */
  for (size_t i = 0; i < nd && ok; i++) {
    uint8_t tmp[8];
    memset(tmp, 0xFF, 8);
    size_t chunk = (len - i*8 >= 8) ? 8 : (len - i*8);
    memcpy(tmp, src + i*8, chunk);
    uint32_t lo, hi;
    memcpy(&lo, tmp, 4); memcpy(&hi, tmp+4, 4);
    ok = program_dword(base + i*8, lo, hi) && ok;
  }
  flash_lock();
  __enable_irq();
  return ok;
}
