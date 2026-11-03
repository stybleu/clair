# Clair Studio en application Android (V0.71.6 a V0.71.8) : construit l'APK sur le telephone avec les outils de Termux,
# puis le propose a l'installation.
#   sh ~/clair/tools/studio/android/fabriquer.sh               construit si besoin, propose l'installation
#   sh ~/clair/tools/studio/android/fabriquer.sh --verifier    (studio.sh) rien si la bonne version est installee,
#                                                              rien non plus si elle a deja ete proposee
#   sh ~/clair/tools/studio/android/fabriquer.sh --construire  construit seulement
# Code de sortie 0 : l'appli est installee, dans la version de ces sources (ou construite, avec --construire).
#
# La premiere fois, les outils sont installes (environ 300 Mo) : Java (openjdk-21), d8 (classes Java -> dex),
# aapt et aapt2 (les ressources), apksigner, et ecj (pour son android.jar, les classes d'Android).
# V0.71.8 : les ressources de l'appli sont liees a un petit cadre d'Android construit ici (cadre/, voir
# cadre/generer.py) : l'aapt2 de Termux ne sait pas lire /system/framework/framework-res.apk d'Android 16.
# CLAIR_ANDROID_FRAMEWORK : un android.jar complet (ou un framework-res.apk) a la place du cadre.
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

# la version de l'appli installee sur le telephone (vide : pas installee). V0.71.7 : l'appli la donne elle-meme
# (son recepteur Version) ; pm est refuse a Termux sur Android 16. -f 32 : meme une appli jamais ouverte repond.
installee() {
    # Detection non bloquante : Android peut refuser
    # les commandes de gestion des paquets a Termux.
    v="$(cmd package list packages --show-versioncode "$PAQUET" 2>/dev/null || true)"
    n="$(printf '%s\n' "$v" | sed -n "s/^package:$PAQUET versionCode:\([0-9][0-9]*\).*$/\1/p" | head -n 1)"

    if [ -n "$n" ]; then
        printf '%s\n' "$n"
        return 0
    fi

    am broadcast -n "$PAQUET/.Version" -f 32 2>/dev/null |
        sed -n "s/.*data=\"$PAQUET \([0-9]*\)\".*/\1/p" |
        head -n 1
}


# Lance l'application apres son installation.
# Ne pas lancer pendant --verifier (utilise par studio.sh).
lancer() {
    [ "$MODE" = "--verifier" ] && return 0

    dire "demarrage automatique de Clair Studio..."

    resultat="$(am start -n "$PAQUET/.Studio" 2>&1)"
    code=$?

    printf '%s\n' "$resultat" >> "$LOG"

    if [ "$code" -ne 0 ] || printf '%s\n' "$resultat" | grep -Eiq 'Error:|SecurityException|Permission Denial'; then
        dire "ECHEC DU DEMARRAGE"
        printf '%s\n' "$resultat"
        return 1
    fi

    dire "demande de lancement envoyee."
    return 0
}

if [ -z "${PREFIX:-}" ] || ! command -v am > /dev/null 2>&1; then
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
FW="${CLAIR_ANDROID_FRAMEWORK:-}"
if [ -n "$FW" ] && [ ! -r "$FW" ]; then
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
    # V0.71.7 : le d8 de Termux (R8 3.3) plante sur l'attribut MethodParameters que Java 21 met aux classes
    # anonymes et internes : l'appli n'en a pas, on le verifie avant d8 pour le dire clairement
    if command -v javap > /dev/null 2>&1 && javap -v $(find "$T/classes" -name '*.class') 2> /dev/null | grep -q MethodParameters; then
        dire "echec : une classe anonyme ou interne dans java/ (le d8 de Termux ne la lit pas) :"
        for f in $(find "$T/classes" -name '*.class'); do javap -v "$f" 2> /dev/null | grep -q MethodParameters && echo "  $(basename "$f")"; done
        exit 1
    fi
    etape "dex" d8 --release --min-api 29 --lib "$JAR" --output "$T/dex" $(find "$T/classes" -name '*.class')
    etape "ressources" aapt2 compile --dir "$ICI/res" -o "$T/res.zip"
    if [ -z "$FW" ]; then
        etape "cadre" aapt2 compile --dir "$ICI/cadre/res" -o "$T/cadre.zip"
        etape "cadre, lien" aapt2 link -x --manifest "$ICI/cadre/AndroidManifest.xml" -o "$T/cadre.apk" "$T/cadre.zip"
        FW="$T/cadre.apk"
    fi
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
[ "$(installee)" = "$VERSION" ] && { dire "appli installee (version $VERSION)."; lancer; exit $?; }

# Installation Android et demarrage de Clair Studio.
# --verifier ne doit jamais bloquer le lancement du serveur.
if [ "$MODE" = "--verifier" ]; then
    dire "installation non confirmee par Android."
    exit 1
fi

# L'APK est copie dans les telechargements Android.
if [ ! -d "$HOME/storage/downloads" ]; then
    termux-setup-storage || exit 1
fi

DEST="$HOME/storage/downloads/clair-studio.apk"

if [ ! -d "$HOME/storage/downloads" ]; then
    dire "autorise l'acces au stockage puis relance."
    exit 1
fi

cp -f "$APK" "$DEST" || {
    dire "echec de copie de l'APK."
    exit 1
}

dire "APK prepare : $DEST"
echo ""
echo "===== INSTALLATION ANDROID ====="
echo "Ouvre Mes fichiers > Telechargements."
echo "Touche clair-studio.apk, puis Installer."
echo ""
printf "Une fois l'installation terminee, reviens ici et appuie sur Entree : "
IFS= read -r confirmation || exit 1

echo ""
dire "demarrage de Clair Studio..."

resultat="$(am start -n clair.studio/.Studio 2>&1)"
code=$?

printf '%s\n' "$resultat"

if [ "$code" -ne 0 ] || printf '%s\n' "$resultat" | grep -Eiq 'Error:|Error type|SecurityException|Permission Denial'; then
    dire "ECHEC : installation ou ouverture impossible."
    exit 1
fi

dire "DEMANDE_DE_DEMARRAGE=OK"
exit 0
