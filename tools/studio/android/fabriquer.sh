# Clair Studio en application Android (V0.71.6) : construit l'APK sur le telephone avec les outils de Termux,
# puis le propose a l'installation.
#   sh ~/clair/tools/studio/android/fabriquer.sh               construit si besoin, propose l'installation
#   sh ~/clair/tools/studio/android/fabriquer.sh --verifier    (studio.sh) rien si la bonne version est installee,
#                                                              rien non plus si elle a deja ete proposee
#   sh ~/clair/tools/studio/android/fabriquer.sh --construire  construit seulement
# Code de sortie 0 : l'appli est installee, dans la version de ces sources (ou construite, avec --construire).
#
# La premiere fois, les outils sont installes (environ 300 Mo) : Java (openjdk-21), d8 (classes Java -> dex),
# aapt et aapt2 (les ressources), apksigner, et ecj (pour son android.jar, les classes d'Android).
# Les ressources sont liees a celles du telephone (/system/framework/framework-res.apk).
# La cle qui signe l'appli reste sur le telephone (~/.clair_studio.jks) : une mise a jour de l'appli doit etre
# signee par la meme cle, sinon Android la refuse (il faut alors desinstaller l'ancienne).
# Le detail de chaque etape est dans ~/clair_android.log.

ICI="$(cd "$(dirname "$0")" && pwd)"
RACINE="$(cd "$ICI/../../.." && pwd)"
SORTIE="$RACINE/build/studio-android"
APK="$SORTIE/clair-studio.apk"
LOG="$HOME/clair_android.log"
CLE="$HOME/.clair_studio.jks"
PAQUET=clair.studio
MODE="${1:-}"
VERSION="$(sed -n 's/.*android:versionCode="\([0-9]*\)".*/\1/p' "$ICI/AndroidManifest.xml" | head -n 1)"

dire() { echo "Clair Studio : $*"; }

# la version de l'appli installee sur le telephone (vide : pas installee)
installee() {
    pm list packages --show-versioncode "$PAQUET" 2> /dev/null | sed -n "s/^package:$PAQUET versionCode:\([0-9]*\).*/\1/p" | head -n 1
}

if [ -z "${PREFIX:-}" ] || ! command -v pm > /dev/null 2>&1; then
    dire "l'appli Android se construit dans Termux."
    exit 1
fi
mkdir -p "$SORTIE"
if [ "$MODE" = "--verifier" ]; then
    [ "$(installee)" = "$VERSION" ] && exit 0
    if [ -f "$SORTIE/essai-$VERSION" ]; then
        dire "l'appli n'est pas installee : le navigateur s'ouvre a sa place (sh $ICI/fabriquer.sh pour l'installer)."
        exit 1
    fi
fi
: > "$SORTIE/essai-$VERSION"
: > "$LOG"

# Termux doit accepter les commandes de l'appli (lancer le Studio, lire sa cle) : allow-external-apps = true
PROPS="$HOME/.termux/termux.properties"
[ ! -f "$PROPS" ] && [ -f "$HOME/.config/termux/termux.properties" ] && PROPS="$HOME/.config/termux/termux.properties"
if ! grep -q '^[[:space:]]*allow-external-apps[[:space:]]*=[[:space:]]*true' "$PROPS" 2> /dev/null; then
    mkdir -p "$(dirname "$PROPS")"
    [ -f "$PROPS" ] && sed -i '/^[[:space:]]*allow-external-apps[[:space:]]*=/d' "$PROPS"
    echo "allow-external-apps = true" >> "$PROPS"
    termux-reload-settings > /dev/null 2>&1
fi

# les outils
JAR="$PREFIX/share/java/android.jar"
manque() {
    for o in javac keytool d8 aapt aapt2 apksigner; do
        command -v "$o" > /dev/null 2>&1 || return 0
    done
    [ -f "$JAR" ] || return 0
    return 1
}
if manque; then
    dire "installation des outils Android (une seule fois, quelques minutes)..."
    yes 2> /dev/null | pkg install -y openjdk-21 d8 apksigner aapt aapt2 ecj >> "$LOG" 2>&1
    if manque; then
        dire "l'installation des outils a echoue :"
        grep -i -m 4 'error\|unable\|impossible' "$LOG"
        exit 1
    fi
fi
FW="${CLAIR_ANDROID_FRAMEWORK:-/system/framework/framework-res.apk}"
if [ ! -r "$FW" ]; then
    dire "impossible de lire $FW (les ressources d'Android)."
    exit 1
fi

# l'APK : refait quand il manque, ou quand une source est plus recente
if [ ! -f "$APK" ] || [ ! -f "$SORTIE/version" ] || [ "$(cat "$SORTIE/version")" != "$VERSION" ] \
    || [ -n "$(find "$ICI" -newer "$APK" -type f 2> /dev/null | head -n 1)" ]; then
    dire "construction de l'appli..."
    T="$SORTIE/travail"
    rm -rf "$T"
    mkdir -p "$T/classes" "$T/dex"
    etape() {
        echo "== $1" >> "$LOG"
        shift
        "$@" >> "$LOG" 2>&1 && return 0
        dire "echec : $*" | cut -c1-120
        grep -i -m 6 'error\|erreur\|exception' "$LOG"
        exit 1
    }
    etape "java" javac -source 8 -target 8 -bootclasspath "$JAR" -Xlint:-options -encoding UTF-8 -d "$T/classes" $(find "$ICI/java" -name '*.java')
    etape "dex" d8 --release --min-api 29 --lib "$JAR" --output "$T/dex" $(find "$T/classes" -name '*.class')
    etape "ressources" aapt2 compile --dir "$ICI/res" -o "$T/res.zip"
    etape "lien" aapt2 link -I "$FW" --manifest "$ICI/AndroidManifest.xml" -o "$T/brut.apk" "$T/res.zip"
    etape "classes.dex" sh -c 'cd "$1" && aapt add "$2" classes.dex' sh "$T/dex" "$T/brut.apk"
    # aapt2 aligne deja ce qu'il range sans compression (resources.arsc) ; zipalign verifie, s'il est la
    if command -v zipalign > /dev/null 2>&1; then
        etape "alignement" zipalign -f -p 4 "$T/brut.apk" "$T/aligne.apk"
    else
        cp "$T/brut.apk" "$T/aligne.apk"
    fi
    if [ ! -f "$CLE" ]; then
        etape "cle" keytool -genkeypair -keystore "$CLE" -storepass clair-studio -keypass clair-studio -alias clair \
            -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Clair Studio"
    fi
    etape "signature" apksigner sign --ks "$CLE" --ks-pass pass:clair-studio --key-pass pass:clair-studio --ks-key-alias clair \
        --out "$T/signe.apk" "$T/aligne.apk"
    mv -f "$T/signe.apk" "$APK"
    echo "$VERSION" > "$SORTIE/version"
    rm -rf "$T"
fi
[ "$MODE" = "--construire" ] && { dire "appli construite : $APK"; exit 0; }
[ "$(installee)" = "$VERSION" ] && { dire "appli installee (version $VERSION)."; exit 0; }

# l'installation : Android la fait confirmer
dire "touche « Installer » sur l'ecran d'Android, puis reviens ici."
termux-open --content-type application/vnd.android.package-archive "$APK" > /dev/null 2>&1
n=0
while [ "$n" -lt 90 ]; do
    sleep 2
    [ "$(installee)" = "$VERSION" ] && { dire "appli installee (version $VERSION)."; exit 0; }
    n=$((n + 1))
done
if [ -n "$(installee)" ]; then
    dire "la mise a jour de l'appli n'est pas faite. Si Android la refuse, desinstalle l'ancienne appli Clair Studio, puis : sh $ICI/fabriquer.sh"
else
    dire "appli pas installee. Pour la proposer de nouveau : sh $ICI/fabriquer.sh"
fi
exit 1
