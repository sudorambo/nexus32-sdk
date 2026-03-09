int main(void) {
	sys_init();
	gfx_clear(0, 0, 64, 0);
	gfx_present();
	while (1) sys_vsync();
}
