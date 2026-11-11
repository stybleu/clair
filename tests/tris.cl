// V0.75 : sorted, sort, min, max, sum, map, filter, comme Python ;
// tris.expected est la sortie du meme programme en Python (tests/tris.py).
Class Ville {
    nom: string
    habitants: int
    surface: float
}

def montre(xs: list[int]) {
    print(" ".join(map(str, xs)))
}

def montre_f(xs: list[float]) {
    print(" ".join(map(str, xs)))
}

def alea(graine: int, n: int) -> list[int] {
    // un generateur lineaire congruentiel, le meme en Python
    let xs: list[int] = []
    let x = graine
    for i in range(n) {
        x = (x * 1103515245 + 12345) % 2147483648
        xs.append(x % 1000)
    }
    return xs
}

def empreinte(xs: list[int]) -> int {
    // depend de l'ordre de chaque element
    let h = 0
    for i in range(len(xs)) {
        h = (h * 31 + xs[i] * (i + 1)) % 1000000007
    }
    return h
}

def main() {
    // des entiers
    let xs = [5, 3, 9, 1, 3, -2, 7]
    montre(sorted(xs))
    montre(sorted(xs, reverse=true))
    montre(xs)
    print(min(xs))
    print(max(xs))
    print(sum(xs))
    montre(map(lambda x: x * x, xs))
    montre(filter(lambda x: x % 2 == 1, xs))
    let seuil = 4
    montre(filter(lambda x: x > seuil, xs))
    montre(sorted(xs, key=abs))
    montre(sorted(xs, key=lambda x: -x))
    xs.sort()
    montre(xs)
    xs.sort(reverse=true)
    montre(xs)
    let vide: list[int] = []
    print(len(sorted(vide)))
    print(sum(vide))
    vide.sort()

    // des textes : par point de code, comme Python
    let mots = ["poire", "Banane", "été", "abricot", "kiwi", "Zèbre", "eau", "à"]
    print(" ".join(sorted(mots)))
    print(" ".join(sorted(mots, key=str.lower)))
    print(" ".join(sorted(mots, key=len)))
    print(" ".join(sorted(mots, key=len, reverse=true)))
    print(min(mots))
    print(max(mots))
    print(max(mots, key=len))
    print(min(mots, key=len))
    print(" ".join(map(str.upper, mots)))
    print(" ".join(filter(lambda m: "e" in m, mots)))
    print(" ".join(map(lambda m: m[0], sorted(mots))))
    montre(map(len, mots))

    // des objets, par une cle
    let villes = [Ville("Lyon", 522000, 47.87), Ville("Paris", 2103000, 105.4),
                  Ville("Nice", 342000, 71.92), Ville("Lille", 236000, 34.51)]
    for v in sorted(villes, key=lambda v: v.habitants) {
        print(v.nom)
    }
    for v in sorted(villes, key=lambda v: float(v.habitants) / v.surface, reverse=true) {
        print(v.nom)
    }
    print(max(villes, key=lambda v: v.surface).nom)
    print(min(villes, key=lambda v: v.nom).nom)
    print(sum(map(lambda v: v.habitants, villes)))
    print(len(filter(lambda v: v.habitants > 300000, villes)))
    villes.sort(key=lambda v: v.nom)
    print(villes[0].nom + " " + villes[3].nom)

    // des booleens et des flottants
    let bs = [true, false, true, true]
    print(sum(bs))
    print(min(bs))
    print(max(bs))
    let fs = [2.5, -1.25, 0.5, 10.75, -0.0]
    montre_f(sorted(fs))
    montre_f(sorted(fs, reverse=true))
    print(sum(fs))
    print(max(fs))
    let dixiemes: list[float] = [0.1] * 10
    print(sum(dixiemes) == 1.0)
    print(sum([1.0e100, 1.0, -1.0e100]) == 1.0)
    print(sum([0.1, 0.2, 0.3]) == 0.6)

    // des listes de listes, par une cle
    let grille = [[3, 1], [1, 2, 3], [2], [5, 5]]
    for ligne in sorted(grille, key=len) {
        montre(ligne)
    }
    montre(max(grille, key=sum))
    montre(min(grille, key=len))
    montre(map(sum, grille))
    montre(map(max, grille))

    // des dictionnaires : leurs cles
    let stock = {"pommes": 12, "kiwis": 30, "poires": 7}
    for k in sorted(stock) {
        print(k + " " + str(stock[k]))
    }
    print(max(stock, key=lambda k: stock[k]))
    print(min(stock))
    let cubes: dict[int, int] = {}
    for i in range(-3, 4) {
        cubes[i * i * i] = i
    }
    montre(sorted(cubes))
    montre(sorted(cubes, reverse=true))
    print(sum(cubes))

    // un grand tri : 3000 nombres
    let gs = alea(42, 3000)
    let ts = sorted(gs)
    let ok = true
    for i in range(1, len(ts)) {
        if ts[i - 1] > ts[i] {
            ok = false
        }
    }
    print(ok)
    print(ts[0])
    print(ts[1500])
    print(ts[2999])
    print(sum(ts))
    print(empreinte(ts))
    // stable : a cle egale, l'ordre d'arrivee, aussi avec reverse
    let ids = [i for i in range(3000)]
    print(empreinte(sorted(ids, key=lambda i: gs[i] % 10)))
    print(empreinte(sorted(ids, key=lambda i: gs[i] % 10, reverse=true)))
    let copie = gs[0:3000]
    copie.sort(key=lambda g: g % 7)
    print(empreinte(copie))
    print(empreinte(gs))
}
