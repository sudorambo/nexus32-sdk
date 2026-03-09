/* sys.c — System control. Stub for Phase 1. */
void sys_init(void) {}
void sys_vsync(void) {}
void sys_set_irq_handler(int irq, void (*handler)(void)) { (void)irq; (void)handler; }
unsigned sys_frame_count(void) { return 0; }
unsigned sys_cycles_used(void) { return 0; }
void sys_halt(void) {}
