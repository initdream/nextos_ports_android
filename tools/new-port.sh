#!/usr/bin/env bash
#
# new-port.sh — scaffold a new Android ARM64 .so -> NextOS/Linux ARM64 port.
#
# Usage:
#   tools/new-port.sh <game.apk | libgame.so> <port-name>
#
# What it does:
#   1. Extracts the native .so from the APK (lib/arm64-v8a/) + lists assets/OBB.
#   2. Sanity-checks it (android_main = NativeActivity? NEEDED libs?).
#   3. Reads every UND (imported) symbol and CLASSIFIES it:
#        passthrough  -> real libc/libm/GLES/zlib/png symbol (linker resolves it)
#        pthread      -> bionic->glibc wrapper (provided by core)
#        egl          -> egl_shim
#        opensles     -> opensles_shim
#        jni          -> jni_shim
#        android      -> android_shim (ALooper/AAssetManager/...)
#        UNKNOWN      -> logging stub + flagged for you to implement
#   4. Generates src/imports.gen.c (the import table) pre-filled.
#   5. Scaffolds ports/<port-name>/ from template/ + core/.
#   6. Prints a report: how many auto-resolved vs how many need manual work.
#
# This is a generator, not a black box: it gets you to a *compiling* skeleton
# fast; the UNKNOWN symbols + JNI responses are where your work begins.
#
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"   # repo root
TEMPLATE="$HERE/template"
CORE="$HERE/core"

die() { echo "ERRO: $*" >&2; exit 1; }
[ $# -eq 2 ] || die "uso: new-port.sh <game.apk|libgame.so> <port-name>"
INPUT="$1"; PORT="$2"
[ -f "$INPUT" ] || die "arquivo nao existe: $INPUT"
command -v readelf >/dev/null || die "precisa de binutils (readelf)"

OUT="$HERE/ports/$PORT"
[ -e "$OUT" ] && die "ja existe: $OUT (apague ou escolha outro nome)"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT

# ---------------------------------------------------------------------------
# 1. Obter o .so (do APK ou direto)
# ---------------------------------------------------------------------------
SO=""
case "$INPUT" in
  *.apk|*.zip)
    echo ">> extraindo APK..."
    unzip -oq "$INPUT" -d "$WORK/apk"
    mapfile -t SOS < <(find "$WORK/apk/lib/arm64-v8a" -name '*.so' 2>/dev/null | sort)
    [ ${#SOS[@]} -gt 0 ] || die "nenhum .so em lib/arm64-v8a/ (jogo nao e arm64 nativo?)"
    # heuristica: o maior .so costuma ser o engine principal
    SO="$(ls -S "${SOS[@]}" | head -1)"
    echo "   .so principal (maior): $(basename "$SO")"
    [ ${#SOS[@]} -gt 1 ] && printf "   outros .so: %s\n" "$(basename -a "${SOS[@]}" | grep -vF "$(basename "$SO")" | paste -sd' ')"
    echo "   assets/OBB: $(find "$WORK/apk" -path '*/assets/*' -o -name '*.obb' 2>/dev/null | wc -l) arquivos (ver AndroidManifest p/ package)"
    ;;
  *.so) SO="$INPUT";;
  *) die "input deve ser .apk ou .so";;
esac

# ---------------------------------------------------------------------------
# 2. Sanity-check do .so
# ---------------------------------------------------------------------------
echo ">> analisando $(basename "$SO")"
readelf -h "$SO" | grep -q 'AArch64' || die "nao e ARM64 (AArch64). Este loader e arm64-only."
HAS_MAIN=$(readelf -sW "$SO" 2>/dev/null | grep -cw 'android_main' || true)
if [ "$HAS_MAIN" -gt 0 ]; then
  echo "   ✓ android_main encontrado -> NativeActivity (este approach FUNCIONA)"
else
  echo "   ⚠ android_main NAO encontrado -> jogo provavelmente Java/JNI-heavy (port MUITO mais dificil)"
fi
echo "   NEEDED: $(readelf -dW "$SO" | awk '/NEEDED/{gsub(/[][]/,"");print $NF}' | paste -sd' ')"

# ---------------------------------------------------------------------------
# 3. Coletar e classificar os simbolos UND (importados)
# ---------------------------------------------------------------------------
readelf --dyn-syms -W "$SO" \
  | awk '$7=="UND" && $8!="" {print $8}' \
  | sed 's/@.*//' | sort -u > "$WORK/und.txt"
TOTAL=$(wc -l < "$WORK/und.txt")
echo ">> $TOTAL simbolos importados (UND) — classificando..."

# Lista de libc/libm/zlib/png/etc. de PASSTHROUGH direto (linker resolve no alvo).
# (Pode crescer com o tempo — viver em tools/passthrough.txt seria ideal.)
PASS_RE='^(mem(cpy|move|set|cmp|chr)|str(len|cpy|ncpy|cat|ncat|cmp|ncmp|chr|rchr|str|dup|tok|tol|toul|casecmp|ncasecmp|coll|error)|sn?printf|vsn?printf|f?printf|s?scanf|fwrite|fread|fopen|fclose|fseek|ftell|fflush|fputs|fgets|fputc|fgetc|putc|getc|puts|fdopen|fileno|rewind|setvbuf|perror|malloc|calloc|realloc|free|posix_memalign|aligned_alloc|abort|exit|atexit|getenv|setenv|qsort|bsearch|rand|srand|abs|labs|atoi|atol|atof|strtol|strtoul|strtod|strtof|ato.*|isalpha|isdigit|isalnum|isspace|isupper|islower|toupper|tolower|sin|cos|tan|asin|acos|atan|atan2|sqrt|cbrt|pow|exp|exp2|log|log2|log10|floor|ceil|round|trunc|fmod|fabs|fabsf|fmin|fmax|hypot|ldexp|frexp|modf|sinf|cosf|tanf|sqrtf|powf|expf|logf|floorf|ceilf|roundf|fmodf|atan2f|nanf?|memalign|usleep|sleep|nanosleep|clock_gettime|gettimeofday|time|localtime|gmtime|mktime|strftime|open|close|read|write|lseek|stat|fstat|lstat|access|mkdir|rmdir|unlink|rename|opendir|readdir|closedir|getcwd|chdir|realpath|dup|dup2|pipe|fcntl|ioctl|mmap|munmap|mprotect|madvise|sysconf|getpagesize|gettid|getpid|sched_yield|errno|__errno|setlocale|localeconv|iconv.*|inflate|inflateInit.*|inflateEnd|deflate|deflateInit.*|deflateEnd|crc32|adler32|uncompress|compress2?|png_.*|jpeg_.*|z_.*)$'

declare -A CAT
gen() { CAT["$1"]="$2"; }
while read -r s; do
  case "$s" in
    pthread_*|sem_*)                         gen "$s" pthread ;;
    egl*|eglGetProcAddress)                   gen "$s" egl ;;
    sl*|SL_*|slCreateEngine)                  gen "$s" opensles ;;
    gl[A-Z]*|glActiveTexture)                 gen "$s" gles ;;     # passthrough GLES
    Java_*|JNI_*|_JNIEnv*)                    gen "$s" jni ;;
    A[A-Z]*|android_app_*|android_main|ANativeActivity_*) gen "$s" android ;;
    __android_log_*)                          gen "$s" liblog ;;
    __cxa_*|__gxx_*|_Znw*|_Zna*|_ZdlPv*|_ZdaPv*|__cxxabiv*) gen "$s" cxx ;;
    __aeabi_*|__stack_chk_*|__gnu_*)          gen "$s" abi ;;
    *) if [[ "$s" =~ $PASS_RE ]]; then gen "$s" pass; else gen "$s" UNKNOWN; fi ;;
  esac
done < "$WORK/und.txt"

# contagem por categoria
declare -A N
for s in "${!CAT[@]}"; do N["${CAT[$s]}"]=$(( ${N["${CAT[$s]}"]:-0} + 1 )); done

# ---------------------------------------------------------------------------
# 4. Scaffold do port (core + template) + imports.gen.c
# ---------------------------------------------------------------------------
mkdir -p "$OUT/src"
cp "$CORE"/*.c "$CORE"/*.h "$OUT/src/" 2>/dev/null || true
cp "$TEMPLATE"/src/*.c "$TEMPLATE"/src/*.h "$OUT/src/" 2>/dev/null || true

SONAME="$(basename "$SO")"
GEN="$OUT/src/imports.gen.c"
{
  echo "// imports.gen.c — GERADO por new-port.sh para '$PORT' ($SONAME)"
  echo "// $TOTAL simbolos. Resolva os UNKNOWN no fim do arquivo."
  echo '#include "imports.h"'
  echo '#include "so_util.h"'
  echo '#include <stdio.h>'
  echo
  echo "// === passthrough/pthread/shim: ligados automaticamente ==="
  echo "DynLibFunction dynlib_functions[] = {"
  # passthrough direto
  for s in $(printf '%s\n' "${!CAT[@]}" | sort); do
    c="${CAT[$s]}"
    case "$c" in
      pass|gles|liblog|cxx|abi) echo "  {\"$s\", (uintptr_t)&$s},  // $c" ;;
      pthread)  echo "  {\"$s\", (uintptr_t)&${s}_fake},  // pthread wrapper (core)" ;;
      egl)      echo "  {\"$s\", (uintptr_t)&${s}_shim},  // egl_shim" ;;
      opensles) echo "  {\"$s\", (uintptr_t)&${s}_shim},  // opensles_shim" ;;
      android|jni) : ;;  # resolvidos pelos shims via init, nao na tabela
      UNKNOWN)  echo "  // TODO {\"$s\", (uintptr_t)&stub_$s},  // <<< IMPLEMENTAR" ;;
    esac
  done
  echo "};"
  echo "const int dynlib_functions_count = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);"
  echo
  echo "// ===================== SIMBOLOS A IMPLEMENTAR ====================="
  for s in $(printf '%s\n' "${!CAT[@]}" | sort); do
    [ "${CAT[$s]}" = "UNKNOWN" ] && echo "//   $s"
  done
} > "$GEN"

# launch.sh + README do port
cat > "$OUT/launch.sh" <<EOF
#!/bin/sh
cd "\$(dirname "\$0")"
./$PORT &> ./log.txt
EOF
chmod +x "$OUT/launch.sh"

cat > "$OUT/README.md" <<EOF
# $PORT — NextOS Android port

Port gerado por \`nextos_ports_android\`. \`.so\`: \`$SONAME\`.

## Game files (BYO — você fornece, do seu APK legítimo)
- \`$SONAME\` (de lib/arm64-v8a/)
- assets / OBB conforme o jogo

## Estado
- [ ] Resolver UNKNOWN em src/imports.gen.c ($(( ${N[UNKNOWN]:-0} )) símbolos)
- [ ] JNI: package name + OBB path em jni_shim.c
- [ ] Paths/resolução/input em android_shim.c
- [ ] Testar no device Mali
EOF

# ---------------------------------------------------------------------------
# 5. Relatorio
# ---------------------------------------------------------------------------
echo
echo "==================== RELATORIO: $PORT ===================="
printf "  %-12s %s\n" "passthrough" "${N[pass]:-0} (libc/libm/zlib/png)"
printf "  %-12s %s\n" "gles"        "${N[gles]:-0}"
printf "  %-12s %s\n" "pthread"     "${N[pthread]:-0} (wrappers)"
printf "  %-12s %s\n" "egl"         "${N[egl]:-0}"
printf "  %-12s %s\n" "opensles"    "${N[opensles]:-0}"
printf "  %-12s %s\n" "android"     "${N[android]:-0} (android_shim)"
printf "  %-12s %s\n" "jni"         "${N[jni]:-0} (jni_shim)"
printf "  %-12s %s\n" "cxx/abi/log" "$(( ${N[cxx]:-0}+${N[abi]:-0}+${N[liblog]:-0} ))"
echo "  -------------------------------------"
AUTO=$(( TOTAL - ${N[UNKNOWN]:-0} ))
printf "  AUTO-RESOLVIDOS: %s / %s  (%s%%)\n" "$AUTO" "$TOTAL" "$(( AUTO*100/TOTAL ))"
printf "  >>> A IMPLEMENTAR (UNKNOWN): %s  <<<\n" "${N[UNKNOWN]:-0}"
echo "=========================================================="
echo "Port em: $OUT"
echo "Proximo: editar src/imports.gen.c (UNKNOWNs) + jni_shim.c, depois 'make'."
