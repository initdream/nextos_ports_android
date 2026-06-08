#ifndef ZIP_FS_H
#define ZIP_FS_H
#include <stdio.h>
/* I/O transparente backed por zip: serve fopen() pegando entradas de dentro
 * dos assets/data_*.zip (resources que o jogo abre via fopen, ex whitetexture). */
int zip_fs_init(void);
FILE *zip_fs_fopen(const char *path);
#endif
