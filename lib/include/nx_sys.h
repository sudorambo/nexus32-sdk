#ifndef NX_SYS_H
#define NX_SYS_H

void sys_init(void);
void sys_vsync(void);
void sys_set_irq_handler(int irq, void (*handler)(void));
unsigned sys_frame_count(void);
unsigned sys_cycles_used(void);
void sys_halt(void);

#endif
