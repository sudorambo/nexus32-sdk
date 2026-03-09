#ifndef NX_VEC_H
#define NX_VEC_H

typedef struct { float x, y, z, w; } nx_vec4_t;

/* Vector intrinsics — map to V-type instructions per spec §2.4. Implementations in asm or codegen. */
nx_vec4_t __nx_vadd(nx_vec4_t a, nx_vec4_t b);
nx_vec4_t __nx_vsub(nx_vec4_t a, nx_vec4_t b);
nx_vec4_t __nx_vmul(nx_vec4_t a, nx_vec4_t b);
nx_vec4_t __nx_vdiv(nx_vec4_t a, nx_vec4_t b);
float __nx_vdot(nx_vec4_t a, nx_vec4_t b);
nx_vec4_t __nx_vcross(nx_vec4_t a, nx_vec4_t b);
nx_vec4_t __nx_vnorm(nx_vec4_t a);
nx_vec4_t __nx_vlerp(nx_vec4_t a, nx_vec4_t b, float t);
nx_vec4_t __nx_vmov(nx_vec4_t a);
nx_vec4_t __nx_vmin(nx_vec4_t a, nx_vec4_t b);
nx_vec4_t __nx_vmax(nx_vec4_t a, nx_vec4_t b);
nx_vec4_t __nx_vmad(nx_vec4_t a, nx_vec4_t b, nx_vec4_t c);
nx_vec4_t __nx_vneg(nx_vec4_t a);
nx_vec4_t __nx_vabs(nx_vec4_t a);

#endif
