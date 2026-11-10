/* Generic STM32L4 flash erase/program (2 KB pages, 64-bit doubleword). */
#include "board.h"
#include "flashraw.h"
#include <string.h>

#define FLASH_KEY1 0x45670123u
#define FLASH_KEY2 0xCDEF89ABu
#define PAGE_SIZE  2048u

static void unlock(void){ if (FLASH->CR & FLASH_CR_LOCK){ FLASH->KEYR=FLASH_KEY1; FLASH->KEYR=FLASH_KEY2; } }
static void lock(void){ FLASH->CR |= FLASH_CR_LOCK; }
static void waitbsy(void){ while (FLASH->SR & FLASH_SR_BSY){} }
static void clrerr(void){
  FLASH->SR = FLASH_SR_OPERR|FLASH_SR_PROGERR|FLASH_SR_WRPERR|FLASH_SR_PGAERR
            | FLASH_SR_SIZERR|FLASH_SR_PGSERR|FLASH_SR_MISERR|FLASH_SR_FASTERR
            | FLASH_SR_RDERR|FLASH_SR_EOP;
}
static bool bad(void){ return (FLASH->SR & (FLASH_SR_PROGERR|FLASH_SR_WRPERR|FLASH_SR_PGAERR|FLASH_SR_PGSERR))!=0; }

bool flashraw_erase(uint32_t addr, uint32_t len){
  if (!len) return true;
  uint32_t first=(addr-0x08000000u)/PAGE_SIZE;
  uint32_t last =(addr+len-1u-0x08000000u)/PAGE_SIZE;
  bool ok=true;
  __disable_irq(); unlock();
  for (uint32_t p=first; p<=last; p++){
    waitbsy(); clrerr();
    FLASH->CR &= ~FLASH_CR_PNB;
    FLASH->CR |= FLASH_CR_PER | (p<<FLASH_CR_PNB_Pos);
    FLASH->CR |= FLASH_CR_STRT;
    waitbsy();
    FLASH->CR &= ~FLASH_CR_PER;
    if (bad()) ok=false;
  }
  lock(); __enable_irq();
  return ok;
}

bool flashraw_program(uint32_t addr, const void *data, uint32_t len){
  const uint8_t *src=(const uint8_t*)data;
  uint32_t nd=(len+7u)/8u; bool ok=true;
  __disable_irq(); unlock();
  for (uint32_t i=0;i<nd && ok;i++){
    uint8_t tmp[8]; memset(tmp,0xFF,8);
    uint32_t chunk=(len-i*8u>=8u)?8u:(len-i*8u);
    memcpy(tmp,src+i*8u,chunk);
    uint32_t lo,hi; memcpy(&lo,tmp,4); memcpy(&hi,tmp+4,4);
    waitbsy(); clrerr();
    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint32_t*)(addr+i*8u)=lo;
    *(volatile uint32_t*)(addr+i*8u+4)=hi;
    waitbsy();
    if (bad()) ok=false;
    FLASH->CR &= ~FLASH_CR_PG;
  }
  lock(); __enable_irq();
  return ok;
}
