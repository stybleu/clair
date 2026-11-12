// V0.76 : les tuples, comme Python : (a, b), return a, b, let a, b = ..., a, b = b, a, for a, b in ...,
// t[0], ==, <, les cles de tri, les cles de dictionnaire, zip, d.items(), d.get(), partition.
// tuples.expected est la sortie du meme programme en Python (tests/tuples.py).
Class Ville {
    nom: string
    pos: tuple[float, float]
    habitants: int
}

def divise(a: int, b: int) -> tuple[int, int] {
    return a / b, a % b
}

def bornes(xs: list[int]) {
    return min(xs), max(xs)
}

def stats(xs: list[float]) -> tuple[float, int] {
    let s = 0.0
    for x in xs {
        s += x
    }
    return s, len(xs)
}

def fib(n: int) -> int {
    let a, b = 0, 1
    for i in range(n) {
        a, b = b, a + b
    }
    return a
}

def chemins(i: int, j: int, memo: dict[tuple[int, int], int]) -> int {
    // le nombre de chemins d'une grille, avec un memo a deux arguments
    if i == 0 or j == 0 {
        return 1
    }
    if (i, j) in memo {
        return memo[(i, j)]
    }
    let n = chemins(i - 1, j, memo) + chemins(i, j - 1, memo)
    memo[(i, j)] = n
    return n
}

def voisins(p: tuple[int, int]) -> list[tuple[int, int]] {
    let x, y = p
    return [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
}

def largeur(depart: tuple[int, int], murs: list[tuple[int, int]], n: int) -> dict[tuple[int, int], int] {
    // un parcours en largeur sur une grille n x n
    let dist: dict[tuple[int, int], int] = {}
    dist[depart] = 0
    let file = [depart]
    let i = 0
    while i < len(file) {
        let p = file[i]
        i += 1
        for q in voisins(p) {
            let x, y = q
            if x >= 0 and x < n and y >= 0 and y < n and q not in murs and q not in dist {
                dist[q] = dist[p] + 1
                file.append(q)
            }
        }
    }
    return dist
}

def main() {
    // des tuples, lus par leur indice
    let t = (3, "trois", 2.5, true)
    print(t[0])
    print(t[1])
    print(t[2])
    print(t[3])
    print(t[-1])
    print(len(t))
    let (q, r) = divise(17, 5)
    print(q * 10 + r)
    let lo, hi = bornes([4, 9, -2, 7])
    print(lo)
    print(hi)
    let s, n = stats([1.5, 2.5, 4.25])
    print(s)
    print(n)
    print(fib(90))
    let _, milieu, _ = (1, 2, 3)
    print(milieu)

    // echanger sans variable de plus
    let a, b = "gauche", "droite"
    a, b = b, a
    print(a + " " + b)
    let xs = [1, 2, 3, 4, 5]
    xs[0], xs[4] = xs[4], xs[0]
    print(" ".join(map(str, xs)))

    // des listes de tuples
    let gens = [("Léa", 30), ("Bo", 25), ("Ana", 30), ("Zoé", 25), ("Éric", 41)]
    for nom, age in gens {
        print(nom + " " + str(age))
    }
    for nom, age in sorted(gens, key=lambda g: (-g[1], g[0])) {
        print(nom)
    }
    for g in sorted(gens) {
        print(g[0])
    }
    print(max(gens)[0])
    print(min(gens, key=lambda g: (g[1], g[0]))[0])
    print(("Bo", 25) in gens)
    print(("Bo", 26) in gens)
    print(sum(map(lambda g: g[1], gens)))

    // comparer des tuples et des textes, comme Python
    print((1, 2) == (1, 2))
    print((1, 2) != (1, 3))
    print((1, 2) < (1, 3))
    print((2, "a") > (1, "z"))
    print((1, "b") <= (1, "a"))
    print(((1, 2), "x") == ((1, 2), "x"))
    print("pomme" < "poire")
    print("Zèbre" < "abricot")
    print("été" > "eau")

    // zip, et les dictionnaires
    let noms = ["un", "deux", "trois"]
    let nombres = [1, 2, 3, 4]
    for nom, k in zip(noms, nombres) {
        print(nom + "=" + str(k))
    }
    print(len(zip(nombres, noms)))
    let compte: dict[string, int] = {}
    for mot in "le chat et le chien et le rat".split() {
        compte[mot] = compte.get(mot, 0) + 1
    }
    for mot, c in compte.items() {
        print(mot + " " + str(c))
    }
    for mot, c in sorted(compte.items(), key=lambda kv: (-kv[1], kv[0])) {
        print(mot)
    }
    print(" ".join(compte.keys()))
    print(sum(compte.values()))
    print(compte.get("loup", -1))

    // des dictionnaires a cles tuples
    let memo: dict[tuple[int, int], int] = {}
    print(chemins(10, 10, memo))
    print(len(memo))
    let murs = [(1, 0), (1, 1), (1, 2), (3, 1), (3, 2), (3, 3)]
    let dist = largeur((0, 0), murs, 5)
    print(dist[(4, 4)])
    print(len(dist))
    let loin = max(dist.items(), key=lambda kv: (kv[1], kv[0]))
    print(str(loin[0][0]) + "," + str(loin[0][1]) + " " + str(loin[1]))
    let cases = sorted(dist)
    print(str(cases[0][0]) + str(cases[0][1]) + " " + str(cases[len(cases) - 1][0]) + str(cases[len(cases) - 1][1]))
    let vu: dict[tuple[int, bool], string] = {(1, true): "a"}
    vu[(1, false)] = "b"
    print(vu[(1, true)] + vu[(1, false)])
    print(len(vu))

    // des tuples dans les objets, avec des objets, des listes et des fonctions
    let v = Ville("Lyon", (45.75, 4.85), 522000)
    print(v.pos[0])
    let villes = [v, Ville("Nice", (43.7, 7.25), 342000)]
    let paire = (villes[1], villes)
    paire[1].append(Ville("Lille", (50.63, 3.06), 236000))
    print(len(villes))
    print(paire[0].nom)
    let ops: list[tuple[string, def(int) -> int]] = [("double", lambda x: 2 * x), ("carre", lambda x: x * x)]
    for nom, f in ops {
        print(nom + " " + str(f(7)))
    }
    let profond = (1, (2, (3, "fond")))
    print(profond[1][1][1])

    // partition et rpartition
    let cle, signe, valeur = "nom = Léa = moi".partition(" = ")
    print(cle + "|" + signe + "|" + valeur)
    let chemin = "/maison/docs/notes.txt".rpartition("/")
    print(chemin[0] + " " + chemin[2])
    print("abc".partition("x")[0] + "." + "abc".rpartition("x")[2])

    // une boucle qui garde le meilleur tuple : ses textes restent en memoire
    let meilleur = ("", 0)
    for i in range(3000) {
        let nom = "n" + str(i * 7 % 1000)
        if i * 7 % 1000 > meilleur[1] {
            meilleur = (nom, i * 7 % 1000)
        }
        let junk = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" + str(i)
        if len(junk) < 3 {
            print("faux")
        }
    }
    print(meilleur[0])
}
