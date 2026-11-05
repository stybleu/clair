#!/usr/bin/env bash
set -euo pipefail

R="$HOME/clair"
L="$HOME/clair_labs/v62_frontend"
SRC="$R/src"
BUILD="$R/build"
SO="$L/libclairfront.so"

mkdir -p "$L"

echo "===== CLAIR V6.2 ====="
echo "OBJECTIF=FRONTEND_CLAIR_ARM64"

echo "===== VERIFICATIONS ====="

[ "$(uname -m)" = "aarch64" ] || {
    echo "ARCHITECTURE_INCOMPATIBLE"
    exit 1
}

for outil in clang python3 tcc; do
    command -v "$outil" >/dev/null || {
        echo "OUTIL_ABSENT=$outil"
        exit 1
    }
done

[ -s "$SRC/main.c" ] || {
    echo "SOURCE_COMPILATEUR_ABSENTE"
    exit 1
}

[ -x "$BUILD/clair" ] || {
    echo "COMPILATEUR_PRINCIPAL_ABSENT"
    exit 1
}

for h in \
  clair_big_runtime.h \
  clair_measure_runtime.h \
  clair_web_module.h \
  clair_web_runtime.h \
  clair_web_prelude.h \
  clair_os_module.h \
  clair_os_runtime.h \
  clair_plot_module.h \
  clair_plot_runtime.h \
  clair_gui_module.h \
  clair_gui_runtime.h \
  clair_nbody_module.h \
  clair_nbody_runtime.h
do
    [ -s "$BUILD/$h" ] || {
        echo "ENTETE_GENERE_ABSENT=$h"
        exit 1
    }
done

echo "ENTETES_COMPILATEUR=OK"

echo "===== COPIE ADAPTEE DU FRONTEND ====="

python3 - "$SRC/main.c" "$L/main_front.c" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1])
dest = Path(sys.argv[2])

s = source.read_text(encoding="utf-8")

debut = "static int run_clang("
fin = "/* DCE V7.2"

if s.count(debut) != 1 or s.count(fin) != 1:
    sys.exit("STRUCTURE_MAIN_INATTENDUE")

a = s.index(debut)
b = s.index(fin, a)

s = (
    s[:a]
    + '''
static int run_clang(
    const char *cpath,
    const char *output
) {
    (void)cpath;
    (void)output;
    return 0;
}

'''
    + s[b:]
)

ancien = "int main(int argc, char **argv) {"
nouveau = (
    "int clair_lab_compiler_main("
    "int argc, char **argv) {"
)

if s.count(ancien) != 1:
    sys.exit("POINT_ENTREE_INATTENDU")

s = s.replace(ancien, nouveau, 1)

dest.write_text(s, encoding="utf-8")

print("MAIN_ORIGINAL=INCHANGE")
print("MAIN_LABORATOIRE=GENERE")
print("APPEL_CLANG=DESACTIVE_DANS_COPIE")
PY

echo "===== INTERFACE DU COMPILATEUR ====="

cat > "$L/bridge.c" <<'C'
#include <stddef.h>

extern int clair_lab_compiler_main(
    int argc, char **argv
);

__attribute__((visibility("default")))
int clair_lab_generate_c(
    const char *source,
    const char *base
) {
    if (!source || !base)
        return -1;

    char *arguments[] = {
        (char*)"clair",
        (char*)source,
        (char*)"-o",
        (char*)base,
        NULL
    };

    return clair_lab_compiler_main(
        4, arguments
    );
}
C

echo "===== COMPILATION BIBLIOTHEQUE ====="

SOURCES=(
    "$L/main_front.c"
    "$L/bridge.c"
)

for f in \
  diagnostics.c \
  dce.c \
  ad.c \
  ad_reverse.c \
  lexer.c \
  ast.c \
  parser.c \
  semantic.c \
  modules.c \
  units.c \
  codegen_c.c \
  web.c \
  testing.c
do
    SOURCES+=("$SRC/$f")
done

if clang \
    -std=c11 -O2 -fPIC -shared \
    -fwrapv \
    -D_GNU_SOURCE \
    -Wl,--no-undefined \
    -I "$SRC" \
    -I "$BUILD" \
    "${SOURCES[@]}" \
    -lm -ldl -pthread \
    -o "$SO" \
    >"$L/compilation.log" 2>&1
then
    echo "FRONTEND_ARM64=COMPILATION_OK"
else
    echo "FRONTEND_ARM64=COMPILATION_ECHEC"
    tail -n 35 "$L/compilation.log"
    exit 1
fi

[ -s "$SO" ] || {
    echo "BIBLIOTHEQUE_ABSENTE"
    exit 1
}

echo "TAILLE_SO_OCTETS=$(wc -c < "$SO" | tr -d ' ')"

echo "===== DEPENDANCES NATIVES ====="

if command -v llvm-readelf >/dev/null; then
    llvm-readelf -d "$SO" |
        grep NEEDED || true
fi

echo "===== CHARGEUR DE TEST ====="

cat > "$L/charger.c" <<'C'
#include <stdio.h>
#include <dlfcn.h>

typedef int (*Generer)(
    const char*, const char*
);

int main(int argc, char **argv) {
    if (argc < 4 || (argc % 2) != 0) {
        fprintf(stderr, "Arguments invalides\n");
        return 2;
    }

    void *bibliotheque = dlopen(
        argv[1], RTLD_NOW | RTLD_LOCAL
    );

    if (!bibliotheque) {
        fprintf(stderr,
            "ERREUR_DLOPEN=%s\n", dlerror());
        return 1;
    }

    Generer generer = (Generer)dlsym(
        bibliotheque,
        "clair_lab_generate_c"
    );

    if (!generer) {
        puts("FONCTION_FRONTEND_ABSENTE");
        dlclose(bibliotheque);
        return 1;
    }

    for (int i=2; i+1<argc; i+=2) {
        int rc = generer(
            argv[i], argv[i+1]
        );

        printf(
            "GENERATION_%d=%s\n",
            i/2,
            rc == 0 ? "OK" : "ECHEC"
        );

        fflush(stdout);

        if (rc != 0) {
            dlclose(bibliotheque);
            return 1;
        }
    }

    dlclose(bibliotheque);
    return 0;
}
C

clang "$L/charger.c" \
    -ldl -o "$L/charger"

echo "===== PROGRAMMES CLAIR ====="

cat > "$L/simple.cl" <<'CL1'
def main() -> int {
    print(42)
    return 0
}
CL1

cat > "$L/boucle.cl" <<'CL2'
def main() -> int {
    let somme: int = 0

    for i in range(2500000) {
        somme = somme + (i % 97)
    }

    print(somme)
    return 0
}
CL2

cat > "$L/erreur.cl" <<'CL3'
def main() -> int {
    let x: int = 5 +
    return x
}
CL3

echo "===== CHARGEMENT ET GENERATION ====="

"$L/charger" \
    "$SO" \
    "$L/simple.cl" "$L/simple" \
    "$L/boucle.cl" "$L/boucle"

echo "===== VERIFICATION TINYCC ====="

OK=0

for nom in simple boucle; do
    F="$L/$nom.generated.c"

    [ -s "$F" ] || {
        echo "C_GENERE_ABSENT=$nom"
        exit 1
    }

    if ! timeout 25 tcc -lm -run "$F" \
        >"$L/$nom.sortie" \
        2>"$L/$nom.erreur"
    then
        echo "TINYCC_$nom=ECHEC"
        head -n 15 "$L/$nom.erreur"
        exit 1
    fi

    OBTENU="$(cat "$L/$nom.sortie")"

    if [ "$nom" = simple ]; then
        ATTENDU=42
    else
        ATTENDU=119999259
    fi

    echo "PROGRAMME=$nom"
    echo "RESULTAT=$OBTENU"

    if [ "$OBTENU" = "$ATTENDU" ]; then
        echo "RESULTAT_CORRECT=OUI"
        OK=$((OK + 1))
    else
        echo "RESULTAT_CORRECT=NON"
        exit 1
    fi
done

echo "===== DIAGNOSTIC CLAIR ====="

rm -f "$L/erreur.generated.c"

if "$L/charger" "$SO" \
    "$L/erreur.cl" "$L/erreur" \
    >"$L/diagnostic.log" 2>&1
then
    echo "ERREUR_CLAIR=NON_DETECTEE"
    exit 1
else
    echo "ERREUR_CLAIR=DETECTEE"
    head -n 5 "$L/diagnostic.log"
fi

if [ -e "$L/erreur.generated.c" ]; then
    echo "C_INVALIDE_GENERE=OUI"
    exit 1
fi

echo "===== BILAN V6.2 ====="
echo "FRONTEND_ARM64=CONSTRUIT"
echo "CHARGEMENT_DYNAMIQUE=OK"
echo "COMPILATIONS_SUCCESSIVES=OK"
echo "TESTS_TINYCC=$OK/2"
echo "DIAGNOSTICS_CLAIR=OK"
echo "COMPILATEUR_DANS_APK=NON_ENCORE_TESTE"
echo "COMPILATEUR_CLAIR=INCHANGE"
echo "CLAIR_STUDIO=INCHANGE"
echo "GITHUB_MAIN=INCHANGE"
