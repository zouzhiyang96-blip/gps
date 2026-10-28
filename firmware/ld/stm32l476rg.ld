ENTRY(Reset_Handler)
_Min_Heap_Size = 0x400;
_Min_Stack_Size = 0x1000;
MEMORY {
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 768K
  COMMANDS (rx) : ORIGIN = 0x080C0000, LENGTH = 248K
  CONFIG (r) : ORIGIN = 0x080FE000, LENGTH = 4K
  JOURNAL (r) : ORIGIN = 0x080FF000, LENGTH = 4K
  RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 96K
  RAM2 (xrw) : ORIGIN = 0x10000000, LENGTH = 32K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
 .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } > FLASH
 .text : { *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); _etext = .; } > FLASH
 .ARM.extab : { *(.ARM.extab*) } > FLASH
 .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
 .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } > FLASH
 .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); } > FLASH
 .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); } > FLASH
 .command_table : { . = ALIGN(4); KEEP(*(.command_table)) *(.command_handlers*) . = ALIGN(4); } > COMMANDS
 _sidata = LOADADDR(.data);
 .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
 .bss (NOLOAD) : { . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
 .heap_stack (NOLOAD) : { . = ALIGN(8); _end = .; end = .; . += _Min_Heap_Size; . += _Min_Stack_Size; . = ALIGN(8); } > RAM
 /DISCARD/ : { *(.note.GNU-stack) }
}
ASSERT(SIZEOF(.command_table) <= LENGTH(COMMANDS), "Command region overflow")
ASSERT(_ebss + _Min_Heap_Size + _Min_Stack_Size <= _estack, "SRAM overflow")
