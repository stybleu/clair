#!/data/data/com.termux/files/usr/bin/sh
set -eu

LAB="$HOME/clair_labs/studio_apk_labo_v1"
T="$LAB/clair/tools/studio"

export CLAIR="$HOME/clair/build/clair"
export TMPDIR="$LAB/tmp"
export CLAIR_STUDIO_OUVRIR=0
export CLAIR_STUDIO_APPLI=0

[ -x "$CLAIR" ] || {
    echo "Compilateur Clair absent"
    exit 1
}

[ -x "$LAB/clair/build/studio" ] || {
    echo "Serveur de laboratoire absent"
    exit 1
}

mkdir -p "$TMPDIR" "$T/data" "$T/projets"

cd "$T" || exit 1

exec "$LAB/clair/build/studio" "$T/projets" "${2:-8092}"
