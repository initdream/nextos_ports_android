#!/usr/bin/env bash
#
# new-port-arm.sh — variante ARMHF (ARM 32-bit / armeabi-v7a) do new-port.sh.
#
# Igual ao new-port.sh, mas:
#   - extrai de lib/armeabi-v7a/ (não arm64-v8a)
#   - exige ELF ARM 32-bit (não AArch64)
#   - usa o so_util ARMHF (REL relocs) em vez do arm64 (RELA)
#
# Usage: tools/new-port-arm.sh <game.apk|libgame.so> <port-name>
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
TEMPLATE="$HERE/template"
CORE="$HERE/core"
ARM_SO_UTIL="${ARM_SO_UTIL:-$HOME/gtavc-build/loader/so_util.c}"  # so_util armhf

die() { echo "ERRO: $*" >&2; exit 1; }
[ $# -eq 2 ] || die "uso: new-port-arm.sh <game.apk|libgame.so> <port-name>"
INPUT="$1"; PORT="$2"
[ -f "$INPUT" ] || die "arquivo nao existe: $INPUT"
command -v readelf >/dev/null || die "precisa de binutils (readelf)"
[ -f "$ARM_SO_UTIL" ] || die "so_util armhf nao achado em $ARM_SO_UTIL (set ARM_SO_UTIL=)"

OUT="$HERE/ports/$PORT"
[ -e "$OUT" ] && die "ja existe: $OUT"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT

SO=""
case "$INPUT" in
  *.apk|*.zip)
    echo ">> extraindo APK..."
    unzip -oq "$INPUT" -d "$WORK/apk"
    mapfile -t SOS < <(find "$WORK/apk/lib/armeabi-v7a" -name '*.so' 2>/dev/null | sort)
    [ ${#SOS[@]} -gt 0 ] || die "nenhum .so em lib/armeabi-v7a/ (jogo nao e armhf nativo?)"
    SO="$(ls -S "${SOS[@]}" | head -1)"
    echo "   .so principal (maior): $(basename "$SO")"
    [ ${#SOS[@]} -gt 1 ] && printf "   outros .so: %s\n" "$(basename -a "${SOS[@]}" | grep -vF "$(basename "$SO")" | paste -sd' ')"
    ;;
  *.so) SO="$INPUT";;
  *) die "input deve ser .apk ou .so";;
esac

echo ">> analisando $(basename "$SO")"
readelf -h "$SO" | grep -q 'ARM' || die "nao e ARM 32-bit. Use new-port.sh p/ arm64."
readelf -h "$SO" | grep -q 'ELF32' || die "nao e ELF32."
HAS_MAIN=$(readelf -sW "$SO" 2>/dev/null | grep -cw 'android_main' || true)
HAS_JNI=$(readelf -sW "$SO" 2>/dev/null | grep -cw 'JNI_OnLoad' || true)
if [ "$HAS_MAIN" -gt 0 ]; then echo "   ✓ android_main -> NativeActivity"; fi
if [ "$HAS_JNI" -gt 0 ];  then echo "   ✓ JNI_OnLoad (entry JNI, igual GTA CTW/NVEvent)"; fi
echo "   NEEDED: $(readelf -dW "$SO" | awk '/NEEDED/{gsub(/[][]/,"");print $NF}' | paste -sd' ')"

readelf --dyn-syms -W "$SO" \
  | awk '$7=="UND" && $8!="" {print $8}' \
  | sed 's/@.*//' | sort -u > "$WORK/und.txt"
TOTAL=$(wc -l < "$WORK/und.txt")
echo ">> $TOTAL simbolos importados (UND) — classificando..."

PASS_RE='^(mem(cpy|move|set|cmp|chr)|str(len|cpy|ncpy|cat|ncat|cmp|ncmp|chr|rchr|str|dup|tok|tol|toul|casecmp|ncasecmp|coll|error)|sn?printf|vsn?printf|f?printf|s?scanf|fwrite|fread|fopen|fclose|fseek|ftell|fflush|fputs|fgets|fputc|fgetc|putc|getc|puts|fdopen|fileno|rewind|setvbuf|perror|malloc|calloc|realloc|free|posix_memalign|aligned_alloc|abort|exit|atexit|getenv|setenv|qsort|bsearch|rand|srand|rand48|[ls]rand48|abs|labs|atoi|atol|atof|strtol|strtoul|strtod|strtof|isalpha|isdigit|isalnum|isspace|isupper|islower|toupper|tolower|sin|cos|tan|asin|acos|atan|atan2|sqrt|cbrt|pow|exp|exp2|log|log2|log10|floor|ceil|round|trunc|fmod|fabs|fabsf|fmin|fmax|hypot|ldexp|frexp|modf|sinf|cosf|tanf|sqrtf|powf|expf|logf|floorf|ceilf|roundf|fmodf|atan2f|__isfinitef|nanf?|memalign|usleep|sleep|nanosleep|clock_gettime|gettimeofday|time|ctime|localtime|gmtime|mktime|strftime|open|close|read|write|lseek|stat|fstat|lstat|access|mkdir|rmdir|unlink|rename|opendir|readdir|closedir|getcwd|chdir|realpath|dup|dup2|pipe|fcntl|ioctl|mmap|munmap|mprotect|madvise|sysconf|getpagesize|gettid|getpid|sched_yield|sched_.*|errno|__errno|setlocale|localeconv|iconv.*|socket|bind|connect|listen|accept|send|sendto|sendmsg|recv|recvfrom|recvmsg|setsockopt|getsockopt|getsockname|shutdown|select|poll|inet_.*|gethostby.*|_toupper_tab_|inflate|inflateInit.*|inflateEnd|deflate|deflateInit.*|deflateEnd|crc32|adler32|uncompress|compress2?|png_.*|jpeg_.*|z_.*)$'

declare -A CAT
gen() { CAT["$1"]="$2"; }
while read -r s; do
  case "$s" in
    pthread_*|sem_*)                         gen "$s" pthread ;;
    egl*|eglGetProcAddress)                   gen "$s" egl ;;
    sl*|SL_*|slCreateEngine)                  gen "$s" opensles ;;
    gl[A-Z]*|glActiveTexture)                 gen "$s" gles ;;
    Java_*|JNI_*|_JNIEnv*)                    gen "$s" jni ;;
    A[A-Z]*|android_app_*|android_main|ANativeActivity_*) gen "$s" android ;;
    __android_log_*)                          gen "$s" liblog ;;
    __cxa_*|__gxx_*|_Znw*|_Zna*|_ZdlPv*|_ZdaPv*|__cxxabiv*) gen "$s" cxx ;;
    __aeabi_*|__stack_chk_*|__gnu_*)          gen "$s" abi ;;
    *) if [[ "$s" =~ $PASS_RE ]]; then gen "$s" pass; else gen "$s" UNKNOWN; fi ;;
  esac
done < "$WORK/und.txt"

declare -A N
for s in "${!CAT[@]}"; do N["${CAT[$s]}"]=$(( ${N["${CAT[$s]}"]:-0} + 1 )); done

mkdir -p "$OUT/src"
cp "$CORE"/*.c "$CORE"/*.h "$OUT/src/" 2>/dev/null || true
cp "$TEMPLATE"/src/*.c "$TEMPLATE"/src/*.h "$OUT/src/" 2>/dev/null || true
# >>> troca o so_util arm64 pelo ARMHF <<<
cp "$ARM_SO_UTIL" "$OUT/src/so_util.c"
echo ">> so_util ARMHF instalado ($ARM_SO_UTIL)"

SONAME="$(basename "$SO")"
GEN="$OUT/src/imports.gen.c"
{
  echo "// imports.gen.c — GERADO por new-port-arm.sh para '$PORT' ($SONAME)"
  echo "// $TOTAL simbolos. Resolva os UNKNOWN no fim do arquivo."
  echo '#include "imports.h"'
  echo '#include "so_util.h"'
  echo '#include <stdio.h>'
  echo
  echo "DynLibFunction dynlib_functions[] = {"
  for s in $(printf '%s\n' "${!CAT[@]}" | sort); do
    c="${CAT[$s]}"
    case "$c" in
      pass|gles|liblog|cxx|abi) echo "  {\"$s\", (uintptr_t)&$s},  // $c" ;;
      pthread)  echo "  {\"$s\", (uintptr_t)&${s}_fake},  // pthread wrapper (core)" ;;
      egl)      echo "  {\"$s\", (uintptr_t)&${s}_shim},  // egl_shim" ;;
      opensles) echo "  {\"$s\", (uintptr_t)&${s}_shim},  // opensles_shim" ;;
      android|jni) : ;;
      UNKNOWN)  echo "  // TODO {\"$s\", (uintptr_t)&stub_$s},  // <<< IMPLEMENTAR" ;;
    esac
  done
  echo "};"
  echo "const int dynlib_functions_count = sizeof(dynlib_functions)/sizeof(dynlib_functions[0]);"
  echo
  echo "// ===================== SIMBOLOS A IMPLEMENTAR (UNKNOWN) ====================="
  for s in $(printf '%s\n' "${!CAT[@]}" | sort); do
    [ "${CAT[$s]}" = "UNKNOWN" ] && echo "//   $s"
  done
} > "$GEN"

echo
echo "==================== RELATORIO ARMHF: $PORT ===================="
printf "  %-12s %s\n" "passthrough" "${N[pass]:-0}"
printf "  %-12s %s\n" "gles"        "${N[gles]:-0}"
printf "  %-12s %s\n" "pthread"     "${N[pthread]:-0}"
printf "  %-12s %s\n" "egl"         "${N[egl]:-0}"
printf "  %-12s %s\n" "opensles"    "${N[opensles]:-0}"
printf "  %-12s %s\n" "android"     "${N[android]:-0}"
printf "  %-12s %s\n" "jni"         "${N[jni]:-0}"
printf "  %-12s %s\n" "cxx/abi/log" "$(( ${N[cxx]:-0}+${N[abi]:-0}+${N[liblog]:-0} ))"
echo "  -------------------------------------"
AUTO=$(( TOTAL - ${N[UNKNOWN]:-0} ))
printf "  AUTO-RESOLVIDOS: %s / %s  (%s%%)\n" "$AUTO" "$TOTAL" "$(( AUTO*100/TOTAL ))"
printf "  >>> A IMPLEMENTAR (UNKNOWN): %s <<<\n" "${N[UNKNOWN]:-0}"
echo "=========================================================="
echo "Port em: $OUT"
