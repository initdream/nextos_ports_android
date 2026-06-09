/* gl_trace.c — wrappers que logam as chamadas GL do engine p/ a RE do render.
 * A tabela imports.gen.c aponta glX -> my_glX; my_glX loga + chama o GL real. */
#include <GLES2/gl2.h>
#include <stdio.h>

static int nd, nf, np, ns, nc;

int g_drawcount = 0;
void my_glDrawArrays(GLenum m, GLint f, GLsizei c) { g_drawcount++; glDrawArrays(m, f, c); }
void my_glDrawElements(GLenum m, GLsizei c, GLenum t, const void *i) {
  if (nd++ < 12) fprintf(stderr, "[GL] DrawElements count=%d err=0x%x\n", c, glGetError());
  glDrawElements(m, c, t, i);
}
void my_glBindFramebuffer(GLenum t, GLuint fb) {
  if (nf++ < 20) fprintf(stderr, "[GL] BindFramebuffer %u\n", fb);
  glBindFramebuffer(t, fb);
}
void my_glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
  if (nf++ < 20) fprintf(stderr, "[GL] Viewport %d,%d %dx%d\n", x, y, w, h);
  glViewport(x, y, w, h);
}
GLuint my_glCreateProgram(void) {
  GLuint p = glCreateProgram();
  if (np++ < 10) fprintf(stderr, "[GL] CreateProgram -> %u\n", p);
  return p;
}
void my_glLinkProgram(GLuint p) {
  glLinkProgram(p);
  GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) { char l[512]; glGetProgramInfoLog(p, 512, 0, l); fprintf(stderr, "[GL] LINK FAIL prog %u: %s\n", p, l); }
}
void my_glCompileShader(GLuint s) {
  glCompileShader(s);
  GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) { char l[512]; glGetShaderInfoLog(s, 512, 0, l); fprintf(stderr, "[GL] SHADER FAIL %u: %s\n", s, l); }
  else if (ns++ < 12) fprintf(stderr, "[GL] shader %u compilou ok\n", s);
}
void my_glClear(GLbitfield m) {
  if (nc++ < 6) fprintf(stderr, "[GL] Clear 0x%x\n", m);
  glClear(m);
}

void my_glGenTextures(GLsizei n, GLuint *t) { fprintf(stderr, "[GL] GenTextures n=%d\n", n); glGenTextures(n, t); }
void my_glBindTexture(GLenum tg, GLuint tex) { static int c; if (c++ < 10) fprintf(stderr, "[GL] BindTexture %u\n", tex); glBindTexture(tg, tex); }
void my_glTexImage2D(GLenum tg, GLint l, GLint ifmt, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const void *p) { fprintf(stderr, "[GL] TexImage2D %dx%d ifmt=0x%x f=0x%x\n", w, h, ifmt, f); glTexImage2D(tg, l, ifmt, w, h, b, f, ty, p); }
void my_glGenFramebuffers(GLsizei n, GLuint *fb) { fprintf(stderr, "[GL] GenFramebuffers n=%d\n", n); glGenFramebuffers(n, fb); }
void my_glGenBuffers(GLsizei n, GLuint *b) { fprintf(stderr, "[GL] GenBuffers n=%d\n", n); glGenBuffers(n, b); }

unsigned my_glCreateShader(GLenum t) { unsigned s = glCreateShader(t); fprintf(stderr, "[GL] CreateShader type=0x%x -> %u\n", t, s); return s; }
void my_glActiveTexture(GLenum t) { static int c; if (c++ < 4) fprintf(stderr, "[GL] ActiveTexture 0x%x\n", t); glActiveTexture(t); }
void my_glBufferData(GLenum tg, GLsizeiptr sz, const void *d, GLenum u) { fprintf(stderr, "[GL] BufferData sz=%ld\n", (long)sz); glBufferData(tg, sz, d, u); }

void my_glTexSubImage2D(GLenum tg, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void *p) {
  static int c=0;
  if (c++<3 && p) {
    const unsigned char *b=p; long nz=0,n=(long)w*h*4;
    for (long i=0;i<n;i++) if (b[i]>10) nz++;
    fprintf(stderr, "[GL] TexSubImage2D %dx%d fmt=0x%x  NAO-PRETO=%ld/%ld\n", w,h,f, nz, n);
  }
  glTexSubImage2D(tg,l,x,y,w,h,f,ty,p);
}

void my_glShaderSource(GLuint sh, GLsizei n, const char *const *str, const GLint *len) {
  static int c=0;
  if (c++<8 && n>0 && str) {
    fprintf(stderr, "[GL] ShaderSource sh=%u parts=%d:", sh, n);
    for (int i=0;i<n && i<4;i++) if(str[i]) { char buf[40]; int j=0; for(;j<39 && str[i][j] && str[i][j]!='\n';j++) buf[j]=str[i][j]; buf[j]=0; fprintf(stderr, " [%s]", buf); }
    fprintf(stderr, "\n");
  }
  glShaderSource(sh,n,str,len);
}

/* O engine NAO cria shaders (o Java criava). Crio um programa de blit GLES2
 * e intercepto glUseProgram pra bindar o meu (atribs pos@0/color@1/uv@2 +
 * sampler na unidade 0, igual o formato RenderVertex do RSDK). */
static GLuint g_blitprog = 0;
static GLuint make_blit_program(void) {
  const char *vs = "attribute vec3 in_pos;\nattribute vec4 in_color;\nattribute vec2 in_UV;\nvarying vec2 ex_UV;\nvarying vec4 ex_color;\nvoid main(){ gl_Position=vec4(in_pos,1.0); ex_UV=in_UV; ex_color=in_color; }\n";
  const char *fs = "precision mediump float;\nuniform sampler2D tex;\nvarying vec2 ex_UV;\nvarying vec4 ex_color;\nvoid main(){ gl_FragColor=texture2D(tex, ex_UV); }\n";
  GLuint v=glCreateShader(GL_VERTEX_SHADER); glShaderSource(v,1,&vs,0); glCompileShader(v);
  GLint ok=0; glGetShaderiv(v,GL_COMPILE_STATUS,&ok); if(!ok){char l[512];glGetShaderInfoLog(v,512,0,l);fprintf(stderr,"[blit] VS FAIL: %s\n",l);}
  GLuint f=glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(f,1,&fs,0); glCompileShader(f);
  glGetShaderiv(f,GL_COMPILE_STATUS,&ok); if(!ok){char l[512];glGetShaderInfoLog(f,512,0,l);fprintf(stderr,"[blit] FS FAIL: %s\n",l);}
  GLuint p=glCreateProgram(); glAttachShader(p,v); glAttachShader(p,f);
  glBindAttribLocation(p,0,"in_pos"); glBindAttribLocation(p,1,"in_color"); glBindAttribLocation(p,2,"in_UV");
  glLinkProgram(p); glGetProgramiv(p,GL_LINK_STATUS,&ok);
  fprintf(stderr,"[blit] programa=%u link=%d\n", p, ok);
  return p;
}
void my_glUseProgram(GLuint id) {
  (void)id;
  if (!g_blitprog) g_blitprog = make_blit_program();
  glUseProgram(g_blitprog);
}
