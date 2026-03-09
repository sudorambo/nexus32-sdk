.section .text
.global gfx_init
gfx_init:
	jr ra

.global gfx_clear
gfx_clear:
	addiu r30, r30, -16
	sw r16, 0(r30)
	sw r17, 4(r30)
	lui r16, 0x0A00
	addiu r17, r0, 16
	ori r17, r17, 1
	sw r17, 0(r16)
	sw r0, 4(r16)
	sll r2, r4, 24
	sll r17, r5, 16
	or r2, r2, r17
	or r2, r2, r6
	sll r2, r2, 8
	ori r2, r2, 255
	sw r2, 8(r16)
	sw r0, 12(r16)
	lw r16, 0(r30)
	lw r17, 4(r30)
	addiu r30, r30, 16
	jr ra

.global gfx_present
gfx_present:
	addiu r30, r30, -16
	sw r16, 0(r30)
	lui r16, 0x0A00
	lui r2, 4
	ori r2, r2, 0xFF
	sw r2, 16(r16)
	lui r16, 0x0B00
	addiu r2, r0, 1
	sw r2, 4(r16)
	addiu r2, r0, 20
	sw r2, 12(r16)
	lw r16, 0(r30)
	addiu r30, r30, 16
	jr ra
