/* xell-reloaded's xell-1f.lds, plus the .*.* input sections -ffunction-sections and -fdata-sections make. The linker
   places .opd (function descriptors, which startup.S reads start's TOC from) and .sfpr among these itself. */
ENTRY(_start)
SECTIONS
{
	. = 0x800000001c000000;
	.text : { *(.text) *(.text.*) }
	.data : { *(.data) *(.data.*) }
	.sdata : { *(.sdata) *(.sdata.*) }
	.rodata : { *(.rodata) *(.rodata.*) }
	blob_end = .;

	. = 0x800000001c080000;

	bss_start = .;
	.bss : { *(.bss) *(.bss.*) }
	.sbss : { *(.sbss) *(.sbss.*) }
	bss_end = .;
}
