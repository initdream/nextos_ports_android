/* patches.c — stubs dos patches da engine (audio/GL/game).
 *
 * Para o 1º bring-up (bootar + renderizar). Portar de gtactw_vita/loader/
 * {openal_patch.c, mpg123_patch.c, opengl_patch.c} depois:
 *   - patch_openal : bridge OpenSL/OpenAL -> áudio (openal-soft do /usr/lib32)
 *   - patch_mpg123 : streaming mp3 (rádio) -> libmpg123
 *   - patch_opengl : fixups GL (precision/highp p/ Utgard, BGRA->RGBA swizzle)
 *   - patch_game   : hooks específicos do jogo (se necessário)
 *
 * Enquanto stub: sem áudio, sem fixups. Se a tela vier preta por shader,
 * portar patch_opengl é o 1º suspeito.
 */
void patch_mpg123(void) {}
void patch_openal(void) {}
void patch_opengl(void) {}
void patch_game(void) {}
