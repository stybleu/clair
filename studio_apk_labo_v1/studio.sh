#!/data/data/com.termux/files/usr/bin/sh
set -eu

LAB="$HOME/clair_labs/studio_apk_labo_v1"
PORT=8092
JETON="$LAB/clair/tools/studio/data/jeton"
URL="http://127.0.0.1:$PORT"

echo "===== CLAIR STUDIO LABO ====="

if [ ! -x "$LAB/clair/build/studio" ]; then
    echo "SERVEUR_CLAIR=ABSENT"
    exit 1
fi

if ! command -v curl >/dev/null 2>&1; then
    echo "CURL=ABSENT"
    exit 1
fi

HTTP=$(curl -s --max-time 2 -o /dev/null \
    -w '%{http_code}' "$URL/" || true)

if [ "$HTTP" = "000" ]; then
    echo "DEMARRAGE_SERVEUR=EN_COURS"

    nohup sh "$LAB/lancer_labo.sh" \
        > "$LAB/serveur.log" 2>&1 \
        < /dev/null &

else
    echo "SERVEUR=DEJA_ACTIF"
fi

PRET=NON

for i in $(seq 1 30); do
    HTTP=$(curl -s --max-time 2 -o /dev/null \
        -w '%{http_code}' "$URL/" || true)

    if [ "$HTTP" != "000" ] &&
       [ -s "$JETON" ]; then
        PRET=OUI
        break
    fi

    sleep 0.3
done

if [ "$PRET" != "OUI" ]; then
    echo "SERVEUR=ECHEC"
    tail -n 15 "$LAB/serveur.log"
    exit 1
fi

echo "SERVEUR_WEB_CLAIR=OK"

ADRESSE="$URL/?t=$(cat "$JETON")"

HTTP=$(curl -s --max-time 3 -o /dev/null \
    -w '%{http_code}' "$ADRESSE" || true)

if [ "$HTTP" != "302" ] &&
   [ "$HTTP" != "200" ]; then
    echo "AUTHENTIFICATION_SERVEUR=A_VERIFIER"
    exit 1
fi

echo "===== OUVERTURE APK ====="

am start \
    -n clair.studio.labo/.Studio \
    -a android.intent.action.VIEW \
    -d "$ADRESSE"

echo "===== BILAN ====="
echo "APPLICATION=CLAIR_STUDIO_LABO"
echo "SERVEUR=MODULE_WEB_CLAIR"
echo "COMPILATEUR=CLAIR_CLANG_LLVM"
echo "APK_STABLE=INCHANGEE"
echo "GITHUB=INCHANGE"
echo "===== TERMINE ====="
