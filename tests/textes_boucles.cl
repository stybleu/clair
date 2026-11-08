// V0.54 : ce qu'une boucle garde reste, le reste est rendu a la fin de chaque tour

Class Nom {
    texte: string

    def renomme(t: string) -> int {
        self.texte = t
        return len(t)
    }
}

def garde(xs: list[string], s: string) -> int {
    xs.append(s)
    return len(xs)
}

def cherche(n: int) -> string {
    for i in range(n) {
        let s: string = "v" + str(i * i)
        if i == 3 {
            return s
        }
    }
    return "rien"
}

def main() -> int {
    // garde : une variable, une liste, un dictionnaire, un champ, une fonction qui garde
    let dernier: string = ""
    let tous: list[string] = []
    let d: dict[string, string] = {}
    let o: Nom = Nom("debut")
    let gardes: list[string] = []
    for i in range(4) {
        let s: string = "n" + str(i)
        dernier = s + "!"
        tous.append(s + "?")
        d["k" + str(i)] = s + "#"
        o.renomme(s + "*")
        garde(gardes, s + "+")
    }
    print(dernier)
    for x in tous {
        print(x)
    }
    for k in d {
        print(k + "=" + d[k])
    }
    print(o.texte)
    for x in gardes {
        print(x)
    }

    // rendu : rien n'est garde, sauf les cles du dictionnaire (copiees)
    let n: int = 0
    let compte: dict[string, int] = {}
    for i in range(1000) {
        let ligne: string = "a b " + str(i % 3)
        for m in split(ligne, " ") {
            if m in compte {
                compte[m] += 1
            } else {
                compte[m] = 1
            }
        }
        n += len(ligne)
    }
    print(n)
    for k in compte {
        print(k + ":" + str(compte[k]))
    }

    // une condition de while qui fabrique un texte, break et continue
    let j: int = 0
    while len("x" + str(j)) < 3 {
        j += 1
        if j == 4 {
            continue
        }
        let t: string = "tour " + str(j)
        if j == 7 {
            print(t)
            break
        }
    }
    print(j)

    // un return dans la boucle : le texte rendu reste
    print(cherche(10))
    print(cherche(2))
    return 0
}
