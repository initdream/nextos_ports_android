/* softfp_shim.c -- ponte de ABI float.
   libunity.so/libmono.so sao SOFTFP (base AAPCS): double/float passam em
   registradores INTEIROS (r0:r1 ...). O glibc libm/libc do device e HARDFP
   (Tag_ABI_VFP_args=VFP): double/float em d0..d7. Chamar libm direto do codigo
   softfp do Unity passa os args nos registradores errados -> ex: modf(512.0,iptr)
   le iptr do word baixo de 512.0 = 0 -> escreve em NULL (SEGV em modfl+132).

   Fix: wrappers declarados pcs("aapcs") (softfp) -> recebem como o Unity manda e
   chamam a func glibc (hardfp); o GCC faz a traducao de registradores. Roteados
   via re4_resolve (imports.gen.c) ANTES do dlsym(RTLD_DEFAULT). */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SF __attribute__((pcs("aapcs")))

/* 1 arg double->double */
#define W1D(n)  SF double sf_##n(double x){ return n(x); }
/* 1 arg float->float */
#define W1F(n)  SF float  sf_##n(float x){ return n(x); }
/* 2 arg double,double->double */
#define W2D(n)  SF double sf_##n(double a,double b){ return n(a,b); }
/* 2 arg float,float->float */
#define W2F(n)  SF float  sf_##n(float a,float b){ return n(a,b); }

W1D(acos) W1D(asin) W1D(atan) W1D(cos) W1D(sin) W1D(tan)
W1D(cosh) W1D(sinh) W1D(tanh)
W1D(exp) W1D(exp2) W1D(log) W1D(log10) W1D(sqrt)
W1D(ceil) W1D(floor) W1D(round) W1D(trunc) W1D(rint)
W1F(acosf) W1F(asinf) W1F(atanf) W1F(cosf) W1F(sinf) W1F(tanf)
W1F(expf) W1F(logf) W1F(sqrtf) W1F(fabsf) W1F(ceilf) W1F(floorf) W1F(roundf) W1F(truncf)
W2D(atan2) W2D(fmod) W2D(pow) W2D(remainder)
W2F(atan2f) W2F(fmodf) W2F(powf)

SF double sf_modf(double x,double *iptr){ return modf(x,iptr); }
SF float  sf_modff(float x,float *iptr){ return modff(x,iptr); }
SF double sf_frexp(double x,int *e){ return frexp(x,e); }
SF double sf_ldexp(double x,int e){ return ldexp(x,e); }
SF double sf_strtod(const char *s,char **end){ return strtod(s,end); }

struct sfent { const char *nm; void *fn; };
static const struct sfent SFTAB[] = {
  {"acos",sf_acos},{"asin",sf_asin},{"atan",sf_atan},{"cos",sf_cos},{"sin",sf_sin},{"tan",sf_tan},
  {"cosh",sf_cosh},{"sinh",sf_sinh},{"tanh",sf_tanh},
  {"exp",sf_exp},{"exp2",sf_exp2},{"log",sf_log},{"log10",sf_log10},{"sqrt",sf_sqrt},
  {"ceil",sf_ceil},{"floor",sf_floor},{"round",sf_round},{"trunc",sf_trunc},{"rint",sf_rint},
  {"acosf",sf_acosf},{"asinf",sf_asinf},{"atanf",sf_atanf},{"cosf",sf_cosf},{"sinf",sf_sinf},{"tanf",sf_tanf},
  {"expf",sf_expf},{"logf",sf_logf},{"sqrtf",sf_sqrtf},{"fabsf",sf_fabsf},
  {"ceilf",sf_ceilf},{"floorf",sf_floorf},{"roundf",sf_roundf},{"truncf",sf_truncf},
  {"atan2",sf_atan2},{"fmod",sf_fmod},{"pow",sf_pow},{"remainder",sf_remainder},
  {"atan2f",sf_atan2f},{"fmodf",sf_fmodf},{"powf",sf_powf},
  {"modf",sf_modf},{"modff",sf_modff},{"frexp",sf_frexp},{"ldexp",sf_ldexp},{"strtod",sf_strtod},
};

void *softfp_resolve(const char *nm){
  if(!nm) return 0;
  for(unsigned i=0;i<sizeof(SFTAB)/sizeof(SFTAB[0]);i++)
    if(!strcmp(nm,SFTAB[i].nm)) return SFTAB[i].fn;
  return 0;
}
