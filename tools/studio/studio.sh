# Clair Studio : un IDE pour Clair dans le navigateur du telephone (V0.58, V0.62.1).
#   sh ~/clair/tools/studio/studio.sh [dossier] [port]
# Le dossier des programmes est ~/clair_projets si on n'en donne pas. Ctrl+C arrete le serveur.
# V0.62.1 : se suffit a lui-meme. Sur un appareil ou Clair n'a jamais ete compile (ou apres une mise
# a jour de ses sources), il compile d'abord Clair, en installant clang, cmake et make s'il le faut.
ICI="$(cd "$(dirname "$0")" && pwd)"
RACINE="$(cd "$ICI/../.." && pwd)"
CLAIR="$RACINE/build/clair"
DOSSIER="${1:-}"
if [ -n "$DOSSIER" ]; then
    mkdir -p "$DOSSIER" && DOSSIER="$(cd "$DOSSIER" && pwd)" || exit 1
fi

# Clair lui-meme : absent, ou plus ancien qu'un de ses fichiers sources
a_jour() {
    [ -x "$CLAIR" ] || return 1
    command -v find > /dev/null 2>&1 || return 0
    [ -z "$(find "$RACINE/src" "$RACINE/tools/embed_file.cmake" "$RACINE/CMakeLists.txt" -newer "$CLAIR" 2> /dev/null | head -n 1)" ]
}
if ! a_jour; then
    MANQUE=""
    for outil in clang cmake make; do
        command -v "$outil" > /dev/null 2>&1 || MANQUE="$MANQUE $outil"
    done
    if [ -n "$MANQUE" ]; then
        if ! command -v pkg > /dev/null 2>&1; then
            echo "Clair Studio : il manque$MANQUE pour compiler Clair."
            exit 1
        fi
        echo "Clair Studio : installation de$MANQUE (une seule fois)..."
        yes 2> /dev/null | pkg install -y clang cmake make libgmp > "$HOME/clair_install.log" 2>&1 || {
            echo "Clair Studio : l'installation a echoue :"
            tail -n 4 "$HOME/clair_install.log"
            exit 1
        }
    fi
    if [ -x "$CLAIR" ]; then
        echo "Clair Studio : Clair a change, compilation..."
    else
        echo "Clair Studio : premiere compilation de Clair (quelques minutes)..."
    fi
    { cmake -S "$RACINE" -B "$RACINE/build" && cmake --build "$RACINE/build" -j2; } < /dev/null > "$HOME/clair_build.log" 2>&1 || {
        echo "Clair Studio : la compilation de Clair a echoue :"
        grep -m 4 -i error "$HOME/clair_build.log"
        exit 1
    }
fi

# le Studio : la premiere fois, puis quand studio.cl ou Clair change
cd "$ICI" || exit 1
if [ ! -x "$RACINE/build/studio" ] || [ studio.cl -nt "$RACINE/build/studio" ] || [ "$CLAIR" -nt "$RACINE/build/studio" ]; then
    echo "Clair Studio : compilation du Studio..."
    "$CLAIR" studio.cl -o "$RACINE/build/studio" || exit 1
fi
# V0.71.6 : l'appli Clair Studio (Android), sans les menus du navigateur. Dans Termux, elle est construite
# (les outils s'installent la premiere fois) et proposee a l'installation quand le telephone n'a pas la
# version de ces sources. CLAIR_STUDIO_APP=0 : le navigateur, comme avant. Lance par l'appli elle-meme
# (CLAIR_STUDIO_OUVRIR=0), le Studio ne verifie rien.
# Android : ouvrir l'application sans dependre
# de la detection de version par le systeme.
if [ "${CLAIR_STUDIO_OUVRIR:-1}" = 1 ] &&
   [ "${CLAIR_STUDIO_APP:-1}" = 1 ] &&
   command -v am >/dev/null 2>&1; then
    export CLAIR_STUDIO_APPLI=1
fi

exec "$RACINE/build/studio" "$DOSSIER" "${2:-8090}"
