/* STM32L4 flash erase/program for the bootloader installer (2 KB pages,
 * 64-bit doubleword programming). Mirrors the app's flash_store internals but
 * works on an arbitrary address range. */
#include "board.h"
#include "bl_flash.h"
#include <string.h>

#define FLASH_KEY1 0x45670123u
#define FLASH_KEY2 0xCDEF89ABu
#define PAGE_SIZE  2048u

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
static bool err(void) {
  return (FLASH->SR & (FLASH_SR_PROGERR|FLASH_SR_WRPERR|FLASH_SR_PGAERR|FLASH_SR_PGSERR)) != 0;
}

static bool erase_page(uint32_t page) {
  flash_wait(); flash_clear_errors();
  FLASH->CR &= ~FLASH_CR_PNB;
  FLASH->CR |= FLASH_CR_PER | (page << FLASH_CR_PNB_Pos);
  FLASH->CR |= FLASH_CR_STRT;
  flash_wait();
  FLASH->CR &= ~FLASH_CR_PER;
  return !err();
}

bool bl_flash_erase(uint32_t addr, uint32_t len) {
  if (len == 0) return true;
  uint32_t first = (addr - 0x08000000u) / PAGE_SIZE;
  uint32_t last  = (addr + len - 1u - 0x08000000u) / PAGE_SIZE;
  bool ok = true;
  __disable_irq();
  flash_unlock();
  for (uint32_t p = first; p <= last; p++) ok = erase_page(p) && ok;
  flash_lock();
  __enable_irq();
  return ok;
}

bool bl_flash_program(uint32_t addr, const void *data, uint32_t len) {
  const uint8_t *src = (const uint8_t *)data;
  uint32_t nd = (len + 7u) / 8u;
  bool ok = true;
  __disable_irq();
  flash_unlock();
  for (uint32_t i = 0; i < nd && ok; i++) {
    uint8_t tmp[8]; memset(tmp, 0xFF, 8);
    uint32_t chunk = (len - i*8u >= 8u) ? 8u : (len - i*8u);
    memcpy(tmp, src + i*8u, chunk);
    uint32_t lo, hi; memcpy(&lo, tmp, 4); memcpy(&hi, tmp+4, 4);
    flash_wait(); flash_clear_errors();
    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint32_t *)(addr + i*8u)     = lo;
    *(volatile uint32_t *)(addr + i*8u + 4) = hi;
    flash_wait();
    ok = !err();
    FLASH->CR &= ~FLASH_CR_PG;
  }
  flash_lock();
  __enable_irq();
  return ok;
}
