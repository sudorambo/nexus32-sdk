.section .text
.global input_buttons
input_buttons:
	lui r2, 0x0B00
	addiu r2, r2, 0x4000
	sll r3, r4, 7
	add r2, r2, r3
	lhu r2, 0(r2)
	jr ra
