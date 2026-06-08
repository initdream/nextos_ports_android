/* zip_fs.c (DEVICE) -- NO-OP. No PC o zip_fs serviu 0 arquivos (os recursos,
 * incl. whitetexture, vêm via NvAPK/asset_archive, não fopen-into-zip). Logo no
 * device não precisa de minizip: w_fopen usa só o disco; zip_fs_fopen=NULL. */
#include "zip_fs.h"
int zip_fs_init(void) { return 0; }
FILE *zip_fs_fopen(const char *path) { (void)path; return NULL; }
