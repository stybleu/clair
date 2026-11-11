// V0.75, correctif de la V0.74 : une boucle rend la memoire de ses textes et de ses objets a chaque
// tour, sauf si elle les range plus loin. Une liste d'objets, une liste de listes de textes ou un
// dictionnaire d'objets rangee dans une variable d'avant la boucle doit garder ses textes.
// regions_listes.expected est la sortie du meme programme en Python (tests/regions_listes.py).
Class Point {
    x: float
    nom: string
}

Class Boite {
    items: list[Point]
}

def fabrique(i: int) -> list[Point] {
    return [Point(float(i), "f" + str(i))]
}

def main() {
    let best: list[Point] = []
    let noms: list[list[string]] = []
    let d: dict[string, Point] = {}
    let par_cle: dict[int, list[string]] = {}
    let boite = Boite([])
    let regle: def(int) -> string = lambda x: "rien"
    // chaque cas dans sa boucle : une seule chose rangee plus loin suffit a garder la memoire
    for i in range(3) {
        let ps = [Point(float(i), "p" + str(i)), Point(1.0, "q" + str(i))]
        best = ps
    }
    for i in range(3) {
        let ms = ["a" + str(i), "b" + str(i)]
        noms = [ms]
    }
    for i in range(3) {
        let tmp: dict[string, Point] = {}
        tmp["k" + str(i)] = Point(2.0, "z" + str(i))
        d = tmp
    }
    for i in range(3) {
        par_cle[i] = ["v" + str(i)]
    }
    for i in range(3) {
        boite.items = fabrique(i)
    }
    for i in range(3) {
        let suffixe = "#" + str(i)
        regle = lambda x: str(x) + suffixe
    }
    // des tours qui font beaucoup de textes : la memoire rendue est reprise
    for i in range(2000) {
        let junk = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" + str(i)
        let o = Point(9.0, junk)
        if len(o.nom) != len(junk) {
            print("faux")
        }
    }
    print(best[0].nom + " " + best[1].nom)
    print(noms[0][1])
    for k in d {
        print(k + " " + d[k].nom)
    }
    for k in par_cle {
        print(str(k) + " " + par_cle[k][0])
    }
    print(boite.items[0].nom)
    print(regle(7))
}
