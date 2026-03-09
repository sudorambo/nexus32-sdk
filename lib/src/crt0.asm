.section .text
.global start
.global _start
_start:
start:
	lui r30, 0x01FF
	ori r30, r30, 0xFFF0
	jal sys_init
	jal main
_start_loop:
	jal sys_vsync
	j _start_loop
