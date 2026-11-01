# Clair Studio : un IDE pour Clair dans le navigateur du telephone (V0.58).
#   sh ~/clair/tools/studio/studio.sh [dossier] [port]
# Le dossier des programmes est ~/clair_projets si on n'en donne pas. Ctrl+C arrete le serveur.
ICI="$(cd "$(dirname "$0")" && pwd)"
DOSSIER="${1:-$HOME/clair_projets}"
mkdir -p "$DOSSIER" && DOSSIER="$(cd "$DOSSIER" && pwd)" || exit 1
cd "$ICI" || exit 1
if [ ! -x ../../build/studio ] || [ studio.cl -nt ../../build/studio ] || [ ../../build/clair -nt ../../build/studio ]; then
    echo "Clair Studio : compilation..."
    ../../build/clair studio.cl -o ../../build/studio || exit 1
fi
exec ../../build/studio "$DOSSIER" "${2:-8090}"
