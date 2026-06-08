/* zip_fs.c — I/O transparente backed por zip (minizip), porta do bully-NX
 * adaptada p/ glibc (fopencookie no lugar do funopen do Switch/newlib).
 * Abre assets/data_*.zip e serve fopen() lendo a entrada pra memória.
 * É a peça de I/O que faltava: Resource::LoadVerified abre recursos (ex:
 * whitetexture) via fopen, e eles estão DENTRO dos data zips. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <minizip/unzip.h>

#include "zip_fs.h"

#define MAX_ZIPS 8
static unzFile s_zips[MAX_ZIPS];
static int s_zip_count = 0;
static pthread_mutex_t s_zip_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct { uint8_t *buf; size_t size; size_t pos; } ZipMem;

static ssize_t zmf_read(void *cookie, char *out, size_t n) {
  ZipMem *m = (ZipMem *)cookie;
  if (m->pos >= m->size) return 0;
  size_t avail = m->size - m->pos;
  if (n > avail) n = avail;
  memcpy(out, m->buf + m->pos, n);
  m->pos += n;
  return (ssize_t)n;
}
static int zmf_seek(void *cookie, off64_t *offp, int whence) {
  ZipMem *m = (ZipMem *)cookie;
  off64_t np;
  switch (whence) {
    case SEEK_SET: np = *offp; break;
    case SEEK_CUR: np = (off64_t)m->pos + *offp; break;
    case SEEK_END: np = (off64_t)m->size + *offp; break;
    default: return -1;
  }
  if (np < 0 || (size_t)np > m->size) return -1;
  m->pos = (size_t)np;
  *offp = np;
  return 0;
}
static int zmf_close(void *cookie) {
  ZipMem *m = (ZipMem *)cookie;
  free(m->buf);
  free(m);
  return 0;
}

int zip_fs_init(void) {
  char path[256];
  s_zip_count = 0;
  for (int i = 0; i < MAX_ZIPS; i++) {
    snprintf(path, sizeof(path), "assets/data_%d.zip", i);
    unzFile zf = unzOpen(path);
    if (!zf) break;
    s_zips[s_zip_count++] = zf;
    fprintf(stderr, "[zip_fs] opened %s\n", path);
  }
  fprintf(stderr, "[zip_fs] %d archives loaded\n", s_zip_count);
  return s_zip_count;
}

/* localiza 'lookup' nos zips, lê pra memória, retorna FILE* (chamar sob mutex) */
static FILE *try_lookup(const char *lookup) {
  for (int i = 0; i < s_zip_count; i++) {
    if (unzLocateFile(s_zips[i], lookup, 1) != UNZ_OK) continue;
    unz_file_info info;
    if (unzGetCurrentFileInfo(s_zips[i], &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK) continue;
    if (unzOpenCurrentFile(s_zips[i]) != UNZ_OK) continue;
    size_t sz = info.uncompressed_size;
    uint8_t *buf = malloc(sz ? sz : 1);
    if (!buf) { unzCloseCurrentFile(s_zips[i]); continue; }
    int rd = unzReadCurrentFile(s_zips[i], buf, sz);
    unzCloseCurrentFile(s_zips[i]);
    if (rd != (int)sz) { free(buf); continue; }
    ZipMem *m = calloc(1, sizeof(ZipMem));
    m->buf = buf; m->size = sz; m->pos = 0;
    cookie_io_functions_t io = { zmf_read, NULL, zmf_seek, zmf_close };
    FILE *f = fopencookie(m, "rb", io);
    if (!f) { free(buf); free(m); return NULL; }
    fprintf(stderr, "[zip_fs] served \"%s\" (%u bytes) from data_%d.zip\n",
            lookup, (unsigned)sz, i);
    return f;
  }
  return NULL;
}

FILE *zip_fs_fopen(const char *path) {
  if (!path || s_zip_count == 0) return NULL;
  const char *lookup = path;
  if (strncmp(lookup, "assets/", 7) == 0) lookup += 7;
  while (lookup[0] == '.' && lookup[1] == '/') lookup += 2;
  pthread_mutex_lock(&s_zip_mutex);
  FILE *f = try_lookup(lookup);
  /* os dados reais ficam sob bullyorig/ -> tenta mapear bully/ -> bullyorig/ */
  if (!f && strncmp(lookup, "bully/", 6) == 0) {
    char alt[600];
    snprintf(alt, sizeof(alt), "bullyorig/%s", lookup + 6);
    f = try_lookup(alt);
  }
  pthread_mutex_unlock(&s_zip_mutex);
  return f;
}
