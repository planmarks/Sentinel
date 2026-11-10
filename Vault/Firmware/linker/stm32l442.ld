/* STM32L442KCU6 - Muru Vault APPLICATION (runs under the secure bootloader)
 * 256 KB flash @ 0x08000000, 64 KB SRAM @ 0x20000000.
 * The app is relocated to 0x08008000 (after the 32 KB bootloader + manifest).
 * STORAGE stays at 0x0803E000 so flash_store.c addresses are unchanged.
 * Build with -DVECT_TAB_ADDR=0x08008000 so the vector table is relocated too.
 */
ENTRY(Reset_Handler)

_estack = 0x20010000;    /* top of 64 KB SRAM */
_Min_Heap_Size  = 0x400; /* 1 KB */
_Min_Stack_Size = 0x1000;/* 4 KB */

MEMORY
{
  FLASH  (rx)  : ORIGIN = 0x08008000, LENGTH = 96K    /* app region */
  STORE  (r)   : ORIGIN = 0x0803E000, LENGTH = 8K
  RAM    (xrw) : ORIGIN = 0x20000000, LENGTH = 64K
}

_store_origin = ORIGIN(STORE);

SECTIONS
{
  .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } >FLASH

  .text : {
    . = ALIGN(4);
    *(.text) *(.text*)
    *(.glue_7) *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init)) KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata : { . = ALIGN(4); *(.rodata) *(.rodata*) . = ALIGN(4); } >FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } >FLASH

  .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } >FLASH
  .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); } >FLASH
  .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); } >FLASH

  _sidata = LOADADDR(.data);
  .data : {
    . = ALIGN(4);
    _sdata = .;
    *(.data) *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  .bss : {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss) *(.bss*) *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  ._user_heap_stack : {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
