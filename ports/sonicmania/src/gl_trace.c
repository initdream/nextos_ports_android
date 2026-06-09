/* gl_trace.c — wrappers que logam as chamadas GL do engine p/ a RE do render.
 * A tabela imports.gen.c aponta glX -> my_glX; my_glX loga + chama o GL real. */
#include <GLES2/gl2.h>
#include <stdio.h>

static int nd, nf, np, ns, nc;

void my_glDrawArrays(GLenum m, GLint f, GLsizei c) {
  if (nd++ < 12) fprintf(stderr, "[GL] DrawArrays mode=%d count=%d err=0x%x\n", m, c, glGetError());
  glDrawArrays(m, f, c);
}
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
