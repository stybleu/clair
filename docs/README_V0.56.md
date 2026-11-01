# Clair V0.56

## Le module os : lancer des programmes, les fichiers, les dossiers, le temps

```text
import os

def main() -> int {
    let r: RunResult = os.run(["clang", "--version"])
    print(r.code)                     // 0 : tout s'est bien passé
    print(r.out)                      // ce qu'il a écrit

    for nom in os.listdir(".") {
        if os.extension(nom) == ".cl" {
            print(nom + " : " + str(os.size(nom)) + " octets")
        }
    }
    return 0
}
```

C'est la première brique d'un IDE écrit en Clair : compiler un programme, le lancer, lire ce qu'il
écrit pendant qu'il tourne, l'arrêter, ranger les fichiers. Rien à installer, et un programme qui
n'importe pas `os` ne change pas : son C est identique à la V0.55.

### Lancer un programme

| | |
|---|---|
| `os.run(["ls", "-l"])` | le lance, attend la fin, donne un `RunResult` : `.code`, `.out`, `.err` |
| `os.run_with(commande, entree, 5.0)` | pareil, avec un texte pour son entrée, arrêté au bout de 5 s (`-1.0` : jamais) |
| `os.shell("ls -l \| wc -l")` | une ligne de commande pour le shell (`sh -c`) |
| `os.start(commande)` | le lance sans attendre : un `Process` |
| `os.start_merged(commande)` | pareil, ses erreurs mêlées à sa sortie, dans l'ordre où il les écrit |
| `os.which("php")` | où est ce programme (`""` s'il n'est pas installé) |

Un `Process` :

| | |
|---|---|
| `p.read()`, `p.read_errors()` | ce qu'il a écrit depuis la dernière fois (`""` si rien) : n'attend jamais |
| `p.write(texte)`, `p.close_input()` | lui donner du texte à lire, puis la fin de son entrée |
| `p.running()` | tourne-t-il encore ? |
| `p.stop()` | l'arrête, lui et ce qu'il a lancé |
| `p.wait()`, `p.wait_for(2.0)` | attend la fin et donne son code (`-1` s'il tourne encore après 2 s) |

Le code est celui que le programme donne en sortant, ou 128 + le signal qui l'a arrêté (137 après
`p.stop()`), ou 127 quand le programme n'existe pas (`r.err` dit pourquoi). La commande est une
liste : `os.run(["clair", nom, "-o", sortie])` passe `nom` tel quel, même avec des espaces.

```text
let p: Process = os.start_merged(["./mon_programme"])
while p.running() {
    write(p.read())                   // montré pendant qu'il tourne
    os.sleep(0.02)
}
write(p.read())
print("code " + str(p.wait()))
```

Ce qu'un programme écrit est lu à chaque fois qu'on le regarde (`read`, `running`, `wait`,
`write`) : il ne reste jamais bloqué parce que personne ne le lit, même pour des millions de lignes.

### Fichiers et dossiers

| | |
|---|---|
| `os.listdir(dossier)` | les noms, triés, sans `.` ni `..` |
| `os.exists(p)`, `os.is_dir(p)`, `os.is_file(p)` | |
| `os.size(p)`, `os.modified(p)` | en octets, et l'heure du dernier changement (secondes depuis 1970) ; `-1` s'il n'existe pas |
| `os.mkdir(p)` | le dossier et ceux au-dessus (rien à faire s'il existe) |
| `os.remove(p)` | un fichier ou un dossier vide |
| `os.rename(p, vers)`, `os.copy_file(p, vers)` | |
| `os.cwd()`, `os.chdir(p)` | le dossier courant |
| `os.join("src", "a.cl")`, `os.basename`, `os.dirname`, `os.extension` | `src/a.cl`, `a.cl`, `src`, `.cl` |

Les fonctions qui peuvent échouer rendent `false`, et `os.error()` dit pourquoi :
`cannot remove nexiste: No such file or directory`. Lire et écrire un fichier :
`read_text`, `read_lines`, `write_file`, comme avant.

### Le programme lui-même

| | |
|---|---|
| `os.args()` | ce qui suit son nom : `./prog a "b c"` donne `["a", "b c"]` |
| `os.env("HOME", "")`, `os.set_env(nom, valeur)`, `os.home()` | l'environnement (les programmes lancés après le reçoivent) |
| `os.exit(3)` | s'arrête tout de suite, avec ce code |
| `os.time()`, `os.clock()`, `os.sleep(0.5)` | l'heure (secondes depuis 1970), une horloge pour mesurer une durée, une pause |

### Avec le module web

`import web` et `import os` vont ensemble : une page peut lancer un programme ou lister un dossier
(`tests/os_web.cl`). C'est la base de Clair Studio, l'IDE dans le navigateur.

## Exemple : le bouton ▶ d'un IDE

`exemples/os/lancer.cl` compile un programme Clair, montre ses erreurs, sinon le lance et montre ce
qu'il écrit pendant qu'il tourne, avec les durées :

```text
cd ~/clair/exemples/os && ../../build/clair lancer.cl -o lancer && ./lancer ../../tests/hello.cl
--- hello.cl compile en 0.08 s
Hello from Clair
--- termine, code 0, en 0.02 s
```

Le programme lancé n'est pas branché au clavier : `input()` y lit une entrée vide.

## À savoir

Construire un texte morceau par morceau dans une boucle (`s = s + ligne`) garde en mémoire tous
les textes intermédiaires jusqu'à la fin de la fonction (une boucle qui garde un texte garde tout,
voir la V0.54) : 3 000 lignes vont bien, 50 000 lignes demandent des gigaoctets. C'est la prochaine
chose à améliorer dans la langue pour un IDE.

## Tests

- `os_module.cl` : arguments (avec un espace), `os.run`, `os.shell` (sortie, erreurs, code 3), une
  entrée donnée à `cat`, un programme absent (127), dossiers, copie, liste, renommage, suppression,
  erreur expliquée, chemins, environnement transmis, temps, un programme lu pendant qu'il tourne,
  un programme arrêté (137), `os.exit(4)` ;
- `exemples/os/lancer.cl` sur `tests/hello.cl`, et sur un programme faux (son erreur est montrée) ;
- `os_web.cl` : un serveur dont une page lance un programme et une autre lit un dossier ;
- à part : 2000 programmes lancés de suite, 20 Mo de sortie lus sans blocage, 3000 lignes données à
  `cat` pendant qu'il les rend, 200 programmes lancés et attendus : aucun descripteur perdu, aucun
  processus zombie ; le tout sous AddressSanitizer et UBSan, sans erreur ;
- le C des 145 programmes de test, de jeux et d'exemples qui n'importent pas `os` est identique à
  la V0.55.
