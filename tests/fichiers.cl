// Fichiers : CSV de nombres, lignes de texte, split, strip, nombres ecrits dans des chaines

def main() -> int {
    // un CSV avec une ligne d'en-tete : 30 fleurs, 4 colonnes
    let d: matrix[float] = load_csv("tests/donnees/fleurs.csv")
    print(rows(d))
    print(cols(d))
    // moyenne de chaque colonne
    for j in range(cols(d)) {
        let s: float = 0.0
        for i in range(rows(d)) {
            s += d[i][j]
        }
        print(s / float(rows(d)))
    }
    // les memes en float32, et la derniere colonne en entiers
    let d32: matrix[float32] = load_csv("tests/donnees/fleurs.csv", "float32")
    print(d32[29][2])
    let total: int = 0
    for i in range(rows(d)) {
        total += int(d[i][3])
    }
    print(total)

    // valeurs separees par des espaces ou des tabulations, fins de ligne Windows, ligne vide
    let m: matrix[float] = load_csv("tests/donnees/mesures.txt")
    print(rows(m))
    print(m[2][0] + m[2][1])

    // ecrire un CSV puis le relire
    let r: matrix[float] = matrix(3, 3)
    for i in range(3) {
        for j in range(3) {
            r[i][j] = d[i][j] / 3.0
        }
    }
    r[2][2] = 1e20
    save_csv("build/fichiers_sortie.csv", r)
    for ligne in read_lines("build/fichiers_sortie.csv") {
        print(ligne)
    }
    let relu: matrix[float] = load_csv("build/fichiers_sortie.csv")
    let pareil: bool = true
    for i in range(3) {
        for j in range(3) {
            if relu[i][j] != r[i][j] {
                pareil = false
            }
        }
    }
    print(pareil)
    let n: matrix[int] = matrix(2, 2)
    n[0][0] = -5
    n[1][1] = 9223372036854775807
    save_csv("build/fichiers_entiers.csv", n)
    let n2: matrix[int] = load_csv("build/fichiers_entiers.csv", "int")
    print(n2[1][1] + n2[0][0])

    // du texte : split, strip, int() et float() de chaines
    for ligne in read_lines("tests/donnees/notes.txt") {
        let parties: list[string] = split(ligne, ":")
        let nom: string = strip(parties[0])
        let somme: float = 0.0
        let nombre: int = 0
        for note in split(parties[1], ",") {
            somme += float(note)
            nombre += 1
        }
        print(nom + " " + strip(split(parties[1], ",")[0]))
        print(somme / float(nombre))
    }
    let mots: list[string] = split("  le chat\tdort\n")
    print(len(mots))
    print(mots[2])
    print(len(split("a,,b,", ",")))
    print(int("  -0042 ") + int("1_000"))
    print(float("-2.5e3") + float(" inf ") > 0.0)
    print(float32("0.1"))
    print("tab:\tfin")
    return 0
}
