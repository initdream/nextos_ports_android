#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <SDL2/SDL.h>
#include <signal.h>
#include <ucontext.h>
#include <pthread.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "so_util.h"
#include "imports.h"
#include "jni_shim.h"
static uintptr_t g_mono_base=0, g_unity_base=0;
static void getmem_trace(const char*tag);
static long my_write(int fd,const void*b,unsigned long n){ if(b&&n>=7&&memmem(b,n,"GET_MEM",7))getmem_trace("write"); return write(fd,b,n); }
/* BRIDGE stdio bionic: __sF[3] (stdin/out/err) do bionic tem layout != glibc. Forneco um
   array marcador + intercepto fprintf/fputs/etc pra mapear &__sF[i] -> stream real do glibc.
   Assim o LOG de erro da Unity (que vai pro stderr bionic) aparece. */
static char g_sf[3*84+16];
static FILE *sf_map(FILE *fp){ uintptr_t p=(uintptr_t)fp,b=(uintptr_t)g_sf;
  if(p>=b && p<b+sizeof(g_sf)){ int i=(int)((p-b)/84); return i<=0?stdin:(i==1?stdout:stderr); } return fp; }
static int my_fprintf(FILE*fp,const char*fmt,...){ if(fmt&&strstr(fmt,"GET_MEM"))getmem_trace("fprintf"); va_list ap; va_start(ap,fmt); int r=vfprintf(sf_map(fp),fmt,ap); va_end(ap); return r; }
static int my_vfprintf(FILE*fp,const char*fmt,va_list ap){ if(fmt&&strstr(fmt,"GET_MEM"))getmem_trace("vfprintf"); return vfprintf(sf_map(fp),fmt,ap); }
static void getmem_trace(const char*tag){
  volatile void *anchor; uintptr_t sp=(uintptr_t)&anchor;
  fprintf(stderr,"[GETMEM-TRACE %s] frames libmono na pilha:\n",tag);
  int found=0;
  for(int k=0;k<1024 && found<24;k++){ uintptr_t v=*(uintptr_t*)(sp+k*4);
    if(g_mono_base&&v>=g_mono_base+0x10000&&v<g_mono_base+0x396c84){ fprintf(stderr,"  libmono+0x%lx\n",(unsigned long)(v-g_mono_base)); found++; } }
  fflush(stderr); }
static int my_fputs(const char*str,FILE*fp){ if(str&&strstr(str,"GET_MEM"))getmem_trace("fputs"); return fputs(str,sf_map(fp)); }
static size_t my_fwrite(const void*p,size_t a,size_t b,FILE*fp){ if(p&&a*b>=7&&memmem(p,a*b,"GET_MEM",7))getmem_trace("fwrite"); return fwrite(p,a,b,sf_map(fp)); }
static int my_fputc(int c,FILE*fp){ return fputc(c,sf_map(fp)); }
static int my_fflush(FILE*fp){ return fflush(fp?sf_map(fp):NULL); }
/* loga dlopen/dlsym -> ve se a engine tenta carregar libmono (Mono runtime C#) */
static so_module *g_m_mono=NULL; static so_module *g_m_unity=NULL;
static char g_dl_self; /* sentinela do handle global/self */
static void *my_dlopen(const char *nm,int flag){
  if(!nm||!nm[0]||strstr(nm,"libc")||strstr(nm,"libunity")||strstr(nm,"libmain")||strstr(nm,"libmono")){
    fprintf(stderr,"[DLOPEN] \"%s\" -> SELF\n",nm?nm:"(null)"); return &g_dl_self; }
  void *h=dlopen(nm,flag); fprintf(stderr,"[DLOPEN] \"%s\" -> %p\n",nm,h); return h?h:&g_dl_self; }
static void *my_dlsym(void *h,const char *nm){ void *p=0;
  fprintf(stderr,"[DLSYM?] %s\n",nm?nm:"?"); fflush(stderr);
  if(h==&g_dl_self){ p=(void*)so_find_addr_safe(nm);
    if(!p && g_m_mono){ so_module *cur=so_save(); so_use(g_m_mono); p=(void*)so_find_addr_safe(nm); so_use(cur); free(cur); }
    if(!p) p=dlsym(RTLD_DEFAULT,nm); }
  else p=dlsym(h,nm);
  fprintf(stderr,"[DLSYM=] %s -> %p\n",nm?nm:"?",p); return p; }
/* getpwuid/getpwnam do glibc fazem dlopen de NSS -> crasha no so-loader. Stub fake. */
static struct passwd g_pw;
static struct passwd *my_getpwuid(unsigned u){ (void)u; g_pw.pw_name=(char*)"user"; g_pw.pw_passwd=(char*)"";
  g_pw.pw_uid=0; g_pw.pw_gid=0; g_pw.pw_gecos=(char*)""; g_pw.pw_dir=(char*)"/storage/roms/re4-recon"; g_pw.pw_shell=(char*)"/bin/sh";
  fprintf(stderr,"[PWUID] stub\n"); return &g_pw; }
static const char *my_dlerror(void){ return 0; } /* sem erro -> evita _dl_exception_create */
static int my_dladdr(const void *a,void *info){ (void)a;(void)info; return 0; }
static int my_dlclose(void *h){ (void)h; return 0; }
/* hooks pra VER a excecao que o Mono lanca (antes do throw crashar) */
static void hook_exc_msg(void *img,const char *ns,const char *name,const char *msg){ (void)img;
  fprintf(stderr,"\n*** [MONO-EXC] %s.%s : %s ***\n",ns?ns:"?",name?name:"?",msg?msg:"(sem msg)"); fflush(stderr); _exit(42); }
static void hook_exc_two(void *img,const char *ns,const char *name,const char *m1,const char *m2){ (void)img;(void)m2;
  fprintf(stderr,"\n*** [MONO-EXC2] %s.%s : %s ***\n",ns?ns:"?",name?name:"?",m1?m1:"?"); fflush(stderr); _exit(42); }
static void hook_exc_name(void *img,const char *ns,const char *name){ (void)img;
  fprintf(stderr,"\n*** [MONO-EXC-N] %s.%s ***\n",ns?ns:"?",name?name:"?"); fflush(stderr); _exit(42); }
/* loga mmap/mprotect EXEC + falhas -> ve a alocacao de exec-mem do JIT que da NULL */
static void *my_mmap(void *a,size_t l,int prot,int flags,int fd,long off){
  if(l>1024UL*1024*1024){ fprintf(stderr,"[MMAP-BIG] %zu valloc=%p GC-caller=%p\n",l,__builtin_return_address(0),__builtin_return_address(1)); }
  void *p=mmap(a,l,prot,flags,fd,off);
  if((prot&PROT_EXEC)||p==MAP_FAILED){ static int n=0; if(n++<60) fprintf(stderr,"[MMAP] len=%zu prot=0x%x -> %p\n",l,prot,p==MAP_FAILED?(void*)-1:p); } return p; }
static int my_mprotect(void *a,size_t l,int prot){ int r=mprotect(a,l,prot);
  if(prot&PROT_EXEC){ static int n=0; if(n++<60) fprintf(stderr,"[MPROT-X] %p len=%zu prot=0x%x -> %d(%s)\n",a,l,prot,r,r?strerror(errno):"ok"); } return r; }
/* sysconf(_SC_PHYS_PAGES)=0 no so-loader -> Mono faz 0-used=negativo=3.8GB. Damos 512MB. */
static long my_sysconf(int name){ long r=sysconf(name);
  if((name==_SC_PHYS_PAGES||name==_SC_AVPHYS_PAGES) && r<=0){ long ps=sysconf(_SC_PAGESIZE); if(ps<=0)ps=4096;
    r=(512L*1024*1024)/ps; fprintf(stderr,"[SYSCONF] name=%d 0->%ld (512MB)\n",name,r); } return r; }
/* BUG ACHADO: mono_pagesize() retorna 3.8GB (lixo) em vez de 4096 -> sgen pede mmap gigante.
   Forco 4096 (valor correto) -> conserta TODOS os tamanhos/alinhamentos do Mono. */
static int my_mono_pagesize(void){ return 4096; }
/* valloc clamp como rede de seguranca (caso algum tamanho absurdo escape) */
static void *my_mono_valloc(void *addr,size_t size,int flags,int type){ (void)type;(void)addr;
  if(size>256UL*1024*1024){ fprintf(stderr,"[VALLOC-CLAMP] %zu -> 256MB\n",size); size=256UL*1024*1024; }
  int prot=0; if(flags&1)prot|=PROT_READ; if(flags&2)prot|=PROT_WRITE; if(flags&4)prot|=PROT_EXEC;
  void *p=mmap(0,size,prot?prot:PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0); return p==MAP_FAILED?0:p; }
/* getter do jit_tls (libmono+0x1a6a8) asserta se a thread nao foi atachada ao Mono.
   Hook + trampolim: atacha a thread (1x) e roda o getter original. */
static void* (*g_orig_jitgetter)(void)=0;
static void* (*g_grd_fn)(void)=0; static void (*g_jatt_fn)(void*)=0;
static __thread int g_jit_attached=0;
static void* my_jit_tls_getter(void){
  static int ent=0; if(ent++<6){ fprintf(stderr,"[JITTLS] getter chamado #%d attached=%d tid=%p\n",ent,g_jit_attached,(void*)pthread_self()); fflush(stderr); }
  if(!g_jit_attached){ g_jit_attached=1;
    void* d = g_grd_fn?g_grd_fn():0;
    if(d && g_jatt_fn){ g_jatt_fn(d); fprintf(stderr,"[JITTLS] thread atachada d=%p tid=%p\n",d,(void*)pthread_self()); fflush(stderr); }
    else { fprintf(stderr,"[JITTLS] sem attach (d=%p jatt=%p)\n",d,(void*)g_jatt_fn); fflush(stderr); }
  }
  return g_orig_jitgetter?g_orig_jitgetter():0;
}
/* handler de assert do Mono (libmono+0x2bcf90): r0=arquivo r1=linha. Loga e NAO aborta. */
static void my_assert_handler(const char* file, int line, const char* a, const char* b){ (void)a;(void)b;
  static int n=0; if(n++<60){ fprintf(stderr,"[ASSERT-SKIP] %s:%d\n", file?file:"?", line); fflush(stderr); }
}
/* loga open/fopen -> acha a fonte de memoria (/proc/meminfo etc) */
static FILE* my_fopen(const char*p,const char*m){ if(p&&(strstr(p,"proc")||strstr(p,"mem")||strstr(p,"sys"))) fprintf(stderr,"[FOPEN] %s\n",p);
  if(p&&!strcmp(p,"/proc/meminfo")){ fprintf(stderr,"[FOPEN] meminfo -> fake 512MB\n");
    FILE*t=tmpfile(); if(t){ fputs("MemTotal:      524288 kB\nMemFree:       262144 kB\nMemAvailable:  262144 kB\n",t); rewind(t); return t; } }
  return fopen(p,m); }
static int my_open(const char*p,int fl,...){ if(p&&(strstr(p,"proc")||strstr(p,"mem"))) fprintf(stderr,"[OPEN] %s\n",p);
  va_list ap; va_start(ap,fl); int mo=va_arg(ap,int); va_end(ap); return open(p,fl,mo); }
extern void *text_virtbase;
extern void re4_fill(void);
extern void recon_wire_pthread(void (*)(const char *, void *));
extern void *jni_find_native(const char *);
extern void egl_shim_create_window(void);
FILE *stderr_fake;
void *asset_open(const char *p){ (void)p; return NULL; }
typedef int jint; typedef jint (*JNI_OnLoad_t)(void*,void*);
#define SO_NAME "libunity.so"
static void re4_set_import(const char *name, void *fn){
  for(size_t i=0;i<dynlib_numfunctions;i++)
    if(!strcmp(dynlib_functions[i].symbol,name)){ dynlib_functions[i].func=(uintptr_t)fn; return; }
}
static void *N(const char*n){ void*p=jni_find_native(n); fprintf(stderr,"  native %s = %p\n",n,p); return p; }
/* BRIDGE de TLS bionic auto-contido: as keys do engine viram SLOTS minhas (1 glibc key so
   guarda o array por-thread). O engine nunca toca o pthread_key do glibc -> sem corrupcao. */
#define NSLOT 1024
static pthread_key_t g_tls_base; static int g_tls_init=0;
static int g_slot_next=1; static pthread_mutex_t g_slot_mtx=PTHREAD_MUTEX_INITIALIZER;
static void tls_dtor(void *p){ free(p); }
static void tls_ensure(void){ if(!g_tls_init){ pthread_key_create(&g_tls_base,tls_dtor); g_tls_init=1; } }
static void **tls_slots(void){ tls_ensure(); void **s=(void**)pthread_getspecific(g_tls_base);
  if(!s){ s=(void**)calloc(NSLOT,sizeof(void*)); pthread_setspecific(g_tls_base,s); } return s; }
static int sh_key_create(pthread_key_t *k, void(*d)(void*)){ static int kc=0; if(kc++<8)fprintf(stderr,"[TLS] key_create dtor=%p\n",d); pthread_mutex_lock(&g_slot_mtx);
  int n=g_slot_next++; pthread_mutex_unlock(&g_slot_mtx); if(n>=NSLOT) return 11; *k=(pthread_key_t)n; return 0; }
static int sh_key_delete(pthread_key_t k){ fprintf(stderr,"[TLS] key_delete %d (no-op)\n",(int)k); return 0; }
static void *sh_getspecific(pthread_key_t k){ if((int)k<=0||(int)k>=NSLOT){ static int g=0; if(g++<30)fprintf(stderr,"[TLS-GET-BADKEY] k=%d tid=%ld\n",(int)k,(long)pthread_self()); return NULL; } void**arr=tls_slots(); void*v=arr[(int)k]; if(!v){static int g2=0; if(g2++<999)fprintf(stderr,"[TLS-GET-NULL] k=%d tid=%p arr=%p\n",(int)k,(void*)pthread_self(),(void*)arr);} return v; }
static int sh_setspecific(pthread_key_t k, const void *v){ if((int)k<=0||(int)k>=NSLOT) return 22; void**arr=tls_slots(); if(v){static int st=0; if(st++<2000)fprintf(stderr,"[TLS-SET] k=%d v=%p tid=%p arr=%p\n",(int)k,v,(void*)pthread_self(),(void*)arr);} arr[(int)k]=(void*)v; return 0; }
/* __android_log REAL -> stderr (sem isso, o erro do engine antes do abort some) */
static int my_alog_print(int prio,const char*tag,const char*fmt,...){ va_list ap; va_start(ap,fmt);
  fprintf(stderr,"[ALOG:%d %s] ",prio,tag?tag:"?"); vfprintf(stderr,fmt,ap); fprintf(stderr,"\n"); va_end(ap); return 0; }
static int my_alog_write(int prio,const char*tag,const char*msg){ fprintf(stderr,"[ALOG:%d %s] %s\n",prio,tag?tag:"?",msg?msg:""); return 0; }
static int my_alog_vprint(int prio,const char*tag,const char*fmt,va_list ap){ fprintf(stderr,"[ALOG:%d %s] ",prio,tag?tag:"?"); vfprintf(stderr,fmt,ap); fprintf(stderr,"\n"); return 0; }
/* bloqueia o engine de instalar handler de crash p/ SIGSEGV/ABRT/etc -> MEU handler pega o crash REAL */
static int my_sigaction(int sig,const void*act,void*old){
  if(getenv("RE4_NOSIGH")&&(sig==4||sig==5||sig==6||sig==7||sig==8||sig==11)){ return 0; } /* debug: deixa o segv original chegar no gdb */ (void)old;
  if(!act) return sigaction(sig,NULL,NULL);
  void *h=*(void* const*)act; /* sa_handler/sa_sigaction @ offset 0 (bionic==glibc) */
  struct sigaction g; memset(&g,0,sizeof g);
  g.sa_sigaction=(void(*)(int,siginfo_t*,void*))h; sigemptyset(&g.sa_mask); g.sa_flags=SA_SIGINFO|SA_RESTART;
  static int sn=0; if(sn++<12) fprintf(stderr,"[SIGACT] sig=%d handler=%p (struct bionic->glibc)\n",sig,h);
  return sigaction(sig,&g,NULL); }
/* intercepta abort/raise/pthread_kill: loga o caller (engine) + NAO mata -> vejo o pos-fatal */
static int my_raise(int sig){ fprintf(stderr,"[RAISE] sig=%d caller=unity+0x%lx -> IGNORADO\n",sig,(unsigned long)__builtin_return_address(0)-(unsigned long)text_virtbase); return 0; }
static void my_abort(void){ fprintf(stderr,"[ABORT] caller=unity+0x%lx -> IGNORADO\n",(unsigned long)__builtin_return_address(0)-(unsigned long)text_virtbase); }
static int my_ptkill(unsigned long t,int sig){ (void)t; fprintf(stderr,"[PTKILL] sig=%d caller=unity+0x%lx -> IGNORADO\n",sig,(unsigned long)__builtin_return_address(0)-(unsigned long)text_virtbase); return 0; }
extern void *text_virtbase;
static void on_segv(int sig, siginfo_t *si, void *uc_){
  ucontext_t *uc=(ucontext_t*)uc_;
  unsigned long pc=uc->uc_mcontext.arm_pc, lr=uc->uc_mcontext.arm_lr;
  unsigned long base=(unsigned long)text_virtbase;
  fprintf(stderr,"\n[SEGV] fault=%p pc=0x%lx lr=0x%lx",si->si_addr,pc,lr);
  if(pc>=base && pc<base+0x2000000) fprintf(stderr," (unity+0x%lx)",pc-base);
  if(g_mono_base && pc>=g_mono_base && pc<g_mono_base+0x600000) fprintf(stderr," (libmono+0x%lx)",pc-g_mono_base);
  if(g_mono_base && lr>=g_mono_base && lr<g_mono_base+0x600000) fprintf(stderr," (lr=libmono+0x%lx)",lr-g_mono_base);
  fprintf(stderr," sig=%d\n",sig);
  /* engole SIGABRT (Boehm ABORT) -> tenta seguir em frente (nao parar no 1o erro) */
  if(getenv("RE4_EATABRT") && sig==SIGABRT){ static int ac=0; if(ac++<200){ if(ac<12)fprintf(stderr,"[ABRT-SWALLOW %d]\n",ac); return; } }
  FILE *m=fopen("/proc/self/maps","r"); char ln[300];
  while(m && fgets(ln,sizeof ln,m)){ unsigned long a,b; if(sscanf(ln,"%lx-%lx",&a,&b)==2 && pc>=a && pc<b){ fprintf(stderr,"[SEGV-LIB] %s",ln); break; } }
  if(m) fclose(m);
  static int sc=0; if(sc++) _exit(139);
  fprintf(stderr,"[REGS] r0=0x%lx r1=0x%lx r2=0x%lx r3=0x%lx r4=0x%lx\n",
    uc->uc_mcontext.arm_r0,uc->uc_mcontext.arm_r1,uc->uc_mcontext.arm_r2,uc->uc_mcontext.arm_r3,uc->uc_mcontext.arm_r4);
  unsigned long sp=uc->uc_mcontext.arm_sp;
  fprintf(stderr,"[BACKTRACE frames sp..+8k]\n");
  for(int k=0;k<2048;k++){ unsigned long v=*(unsigned long*)(sp+k*4);
    if(v>=base && v<base+0x2000000) fprintf(stderr,"  unity+0x%lx\n",v-base);
    else if(g_mono_base && v>=g_mono_base && v<g_mono_base+0x600000) fprintf(stderr,"  libmono+0x%lx\n",v-g_mono_base); }
  _exit(139);
}
int main(void){
  struct sigaction sa; memset(&sa,0,sizeof sa); sa.sa_sigaction=on_segv; sa.sa_flags=SA_SIGINFO;
  sigaction(SIGSEGV,&sa,0); sigaction(SIGBUS,&sa,0); sigaction(SIGABRT,&sa,0); sigaction(SIGILL,&sa,0); sigaction(SIGTRAP,&sa,0); sigaction(SIGFPE,&sa,0);
  fprintf(stderr,"=== RE4 Unity 2018 (ARM32 GLES2) ===\n");
  size_t hs=48*1024*1024;
  void *heap=mmap(NULL,hs,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(heap==MAP_FAILED){perror("mmap");return 1;}
  if(so_load(SO_NAME,heap,hs)<0){fprintf(stderr,"load FALHOU\n");return 1;}
  if(so_relocate()<0){fprintf(stderr,"reloc FALHOU\n");return 1;}
  re4_fill(); recon_wire_pthread(re4_set_import);
  re4_set_import("pthread_key_create",(void*)sh_key_create);
  re4_set_import("pthread_key_delete",(void*)sh_key_delete);
  re4_set_import("pthread_getspecific",(void*)sh_getspecific);
  re4_set_import("pthread_setspecific",(void*)sh_setspecific);
  re4_set_import("__android_log_print",(void*)my_alog_print);
  re4_set_import("__android_log_write",(void*)my_alog_write);
  re4_set_import("__android_log_vprint",(void*)my_alog_vprint);
  re4_set_import("abort",(void*)my_abort);
  re4_set_import("fopen",(void*)my_fopen);
  re4_set_import("open",(void*)my_open);
  re4_set_import("write",(void*)my_write);
  re4_set_import("raise",(void*)my_raise);
  re4_set_import("pthread_kill",(void*)my_ptkill);
  re4_set_import("sigaction",(void*)my_sigaction);
  re4_set_import("dlopen",(void*)my_dlopen);
  re4_set_import("dlsym",(void*)my_dlsym);
  re4_set_import("getpwuid",(void*)my_getpwuid);
  re4_set_import("getpwnam",(void*)my_getpwuid);
  re4_set_import("dlerror",(void*)my_dlerror);
  re4_set_import("dladdr",(void*)my_dladdr);
  re4_set_import("dlclose",(void*)my_dlclose);
  re4_set_import("mmap",(void*)my_mmap);
  re4_set_import("mprotect",(void*)my_mprotect);
  re4_set_import("sysconf",(void*)my_sysconf);
  re4_set_import("__sF",(void*)g_sf);
  re4_set_import("fprintf",(void*)my_fprintf);
  re4_set_import("vfprintf",(void*)my_vfprintf);
  re4_set_import("fputs",(void*)my_fputs);
  re4_set_import("fwrite",(void*)my_fwrite);
  re4_set_import("fputc",(void*)my_fputc);
  re4_set_import("fflush",(void*)my_fflush);
  so_resolve(dynlib_functions,dynlib_numfunctions,0); so_finalize();
  so_execute_init_array();
  fprintf(stderr,"[A] engine init OK (372 ctors)\n");
  g_m_unity=so_save();
  { size_t msz=24*1024*1024; void *mh=mmap(NULL,msz,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(mh!=MAP_FAILED && so_load("libmono.so",mh,msz)>=0){ so_relocate(); so_resolve(dynlib_functions,dynlib_numfunctions,0);
      { uintptr_t a; a=so_find_addr_safe("mono_exception_from_name_msg"); if(a)hook_arm64(a,(uintptr_t)hook_exc_msg);
        a=so_find_addr_safe("mono_exception_from_name_two_strings"); if(a)hook_arm64(a,(uintptr_t)hook_exc_two);
        a=so_find_addr_safe("mono_exception_from_name"); if(a)hook_arm64(a,(uintptr_t)hook_exc_name);
        a=so_find_addr_safe("mono_valloc"); if(a)hook_arm64(a,(uintptr_t)my_mono_valloc);
        a=so_find_addr_safe("mono_pagesize"); if(a){hook_arm64(a,(uintptr_t)my_mono_pagesize); fprintf(stderr,"[HOOK] mono_pagesize -> 4096\n");}
        { uintptr_t ps=so_find_addr_safe("mono_pagesize"); uintptr_t base=ps-0x29d7e4; uintptr_t gt=base+0x1a6a8;
          g_mono_base=base;
          g_grd_fn=(void*)so_find_addr_safe("mono_get_root_domain"); g_jatt_fn=(void*)so_find_addr_safe("mono_jit_thread_attach");
          unsigned char*tr=(unsigned char*)mmap(0,32,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
          if(tr!=MAP_FAILED){ memcpy(tr,(void*)gt,8); *(uint32_t*)(tr+8)=0xe51ff004u; *(uint32_t*)(tr+12)=(uint32_t)(gt+8);
            __builtin___clear_cache((char*)tr,(char*)tr+16); g_orig_jitgetter=(void*(*)(void))tr;
            hook_arm64(gt,(uintptr_t)my_jit_tls_getter); fprintf(stderr,"[HOOK] jit_tls getter @0x1a6a8 base=%p tramp=%p\n",(void*)base,(void*)tr); }
          hook_arm64(base+0x2bcfdc,(uintptr_t)my_assert_handler); fprintf(stderr,"[HOOK] assert handler @0x2bcfdc (g_log fatal, nao-fatal)\n"); }
        so_flush_caches(); }
      so_finalize(); so_execute_init_array(); g_m_mono=so_save(); fprintf(stderr,"[MONO] libmono carregado+init OK\n"); }
    else fprintf(stderr,"[MONO] FALHOU carregar libmono\n"); }
  so_use(g_m_unity);
  void *vm=NULL,*env=NULL; jni_shim_init(&vm,&env);
  uintptr_t onload=so_find_addr_safe("JNI_OnLoad");
  jint ver=((JNI_OnLoad_t)onload)(vm,NULL);
  fprintf(stderr,"[B] JNI_OnLoad=0x%x\n",ver);
  static long t=0xA1, surf=0x5F, ctx=0xC0;
  /* janela GLES2 do device (egl_shim do core, provado) */
  if(SDL_Init(SDL_INIT_VIDEO)!=0) fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());
  egl_shim_create_window();
  fprintf(stderr,"[C] janela SDL/GLES2 criada\n");
  void *fn;
  if((fn=N("initJni"))) ((void(*)(void*,void*,void*))fn)(env,&t,&ctx);
  fprintf(stderr,"[D] initJni OK\n");
  if((fn=N("nativeRecreateGfxState"))) ((void(*)(void*,void*,int,void*))fn)(env,&t,0,&surf);
  if((fn=N("nativeResume"))) ((void(*)(void*,void*))fn)(env,&t);
  if((fn=N("nativeSendSurfaceChangedEvent"))) ((void(*)(void*,void*))fn)(env,&t);
  if((fn=N("nativeFocusChanged"))) ((void(*)(void*,void*,int))fn)(env,&t,1);
  fprintf(stderr,"[E] lifecycle OK -> nativeRender loop\n");
  /* ATTACH da thread ao Mono -> seta o jit_tls (senao assert mini.c:2215) */
  { so_module*c=so_save(); so_use(g_m_mono);
    void*(*grd)(void)=(void*)so_find_addr_safe("mono_get_root_domain");
    void*(*att)(void*)=(void*)so_find_addr_safe("mono_thread_attach");
    void*(*jatt)(void*)=(void*)so_find_addr_safe("mono_jit_thread_attach");
    so_use(c); free(c);
    void *d = grd?grd():NULL;
    fprintf(stderr,"[MONO] root_domain=%p att=%p jatt=%p\n",d,(void*)att,(void*)jatt);
    if(d && jatt){ void*th=jatt(d); fprintf(stderr,"[MONO] jit_thread_attach -> %p\n",th); }
    else if(d && att){ void*th=att(d); fprintf(stderr,"[MONO] thread_attach -> %p\n",th); }
  }
  void *render=N("nativeRender");
  for(int f=0; render && f<1200; f++){
    ((unsigned char(*)(void*,void*))render)(env,&t);
    if(f<5||f%100==0) fprintf(stderr,"[render %d]\n",f);
  }
  fprintf(stderr,"=== render loop terminou ===\n");
  return 0;
}
