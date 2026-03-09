.section .text
.global sys_init
sys_init:
	jr ra

.global sys_vsync
sys_vsync:
	addiu r30, r30, -16
	sw r31, 0(r30)
	sw r16, 4(r30)
	lui r3, 0x0B00
	addiu r3, r3, 0x5000
	lw r2, 32(r3)
sys_vsync_loop:
	lw r16, 32(r3)
	beq r2, r16, sys_vsync_loop
	lw r31, 0(r30)
	lw r16, 4(r30)
	addiu r30, r30, 16
	jr ra

.global sys_set_irq_handler
sys_set_irq_handler:
	sll r3, r4, 2
	sw r5, 0(r3)
	jr ra

.global sys_frame_count
sys_frame_count:
	lui r2, 0x0B00
	addiu r2, r2, 0x5000
	lw r2, 32(r2)
	jr ra

.global sys_cycles_used
sys_cycles_used:
	lui r2, 0x0B00
	addiu r2, r2, 0x6000
	lw r2, 20(r2)
	jr ra

.global sys_halt
sys_halt:
	halt
	jr ra
