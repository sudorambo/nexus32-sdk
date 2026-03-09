.section .text
.global mem_alloc
.global mem_free_all
.global mem_dma_copy
mem_alloc:
mem_free_all:
mem_dma_copy:
	addiu r2, r0, 0
	jr ra
