#ifndef NX_MEM_H
#define NX_MEM_H

void *mem_alloc(unsigned size);
void mem_free_all(void);
void mem_dma_copy(void *dst, const void *src, unsigned size);

#endif
