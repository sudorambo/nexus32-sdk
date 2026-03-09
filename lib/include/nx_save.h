#ifndef NX_SAVE_H
#define NX_SAVE_H

void save_read(void *buf, unsigned offset, unsigned size);
void save_write(const void *buf, unsigned offset, unsigned size);

#endif
