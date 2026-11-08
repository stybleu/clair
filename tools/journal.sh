#!/bin/sh
# V0.70 : envoie un compte rendu dans la branche "journal" de GitHub, sans toucher a main.
#   commande 2>&1 | sh ~/clair/tools/journal.sh "titre"      (la sortie d'une commande)
#   sh ~/clair/tools/journal.sh "titre" fichier              (un fichier)
# Le compte rendu : la date, la version de Clair, le processeur, le nombre de coeurs, Android, clang,
# puis le texte (ses 400 dernieres lignes). Rien de personnel : pas de nom, pas de modele, pas de chemin.
titre=${1:-sortie}
fichier=""
if [ -n "$2" ]; then
    case "$2" in /*) fichier=$2 ;; *) fichier=$PWD/$2 ;; esac
    [ -r "$fichier" ] || { echo "journal pas envoye : $2 introuvable"; exit 1; }
fi
depot=${CLAIR_DEPOT:-$HOME/clair}
cd "$depot" 2> /dev/null || { echo "journal pas envoye : $depot introuvable"; exit 1; }
f=$(mktemp 2> /dev/null || echo "$HOME/.clair_journal_$$")
{
    echo "date : $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "titre : $titre"
    echo "clair : $(git --no-pager log --oneline -1 2> /dev/null | cut -c1-70)"
    echo "processeur : $(uname -m), coeurs : $(nproc 2> /dev/null || getconf _NPROCESSORS_ONLN 2> /dev/null)"
    echo "android : $(getprop ro.build.version.release 2> /dev/null)"
    echo "clang : $(clang --version 2> /dev/null | head -n 1)"
    echo "---"
    if [ -n "$fichier" ]; then tail -n 400 "$fichier"; else tail -n 400; fi
} > "$f"
nom="journal/$(date -u +%Y%m%d_%H%M%S)_$(printf '%s' "$titre" | tr -c 'A-Za-z0-9._-' '_' | cut -c1-40).txt"
blob=$(git hash-object -w "$f")
rm -f "$f"
base=""
git fetch -q origin +refs/heads/journal:refs/remotes/origin/journal 2> /dev/null && base=$(git rev-parse -q --verify refs/remotes/origin/journal)
index="$depot/.git/clair_journal_index"
rm -f "$index"
[ -n "$base" ] && GIT_INDEX_FILE="$index" git read-tree "$base"
GIT_INDEX_FILE="$index" git update-index --add --cacheinfo 100644,"$blob","$nom" || { rm -f "$index"; echo "journal pas envoye"; exit 1; }
arbre=$(GIT_INDEX_FILE="$index" git write-tree)
rm -f "$index"
if [ -n "$base" ]; then commit=$(git commit-tree "$arbre" -p "$base" -m "journal : $titre"); else commit=$(git commit-tree "$arbre" -m "journal : $titre"); fi
git push -q origin "$commit:refs/heads/journal" 2> /dev/null && echo "journal envoye : $nom" || echo "journal pas envoye (pas de reseau ?)"
