// Briques de Python : xs[-1], x in xs, listes en comprehension, for k in dico

Class Classeur {
    poids: list[float]
}

def moyenne(v: list[float]) -> float {
    let s: float = 0.0
    for x in v {
        s += x
    }
    return s / float(len(v))
}

def main() -> int {
    // indices negatifs : comptent depuis la fin
    let xs: list[int] = [3, 1, 4, 1, 5, 9, 2, 6]
    print(xs[-1])
    print(xs[-8])
    xs[-2] = 7
    print(xs[6])
    let k: int = -3
    print(xs[k])
    let noms: list[string] = ["ada", "alan", "grace"]
    print(noms[-1])

    // x in xs, x not in xs
    print(4 in xs)
    print(8 in xs)
    print(8 not in xs)
    print("alan" in noms)
    print("ra" in "grace")
    print("z" not in "grace")
    let fs: list[float] = [0.5, 2.0, -1.0]
    print(2 in fs)
    let octets: list[u8] = [255, 0, 17]
    print(17 in octets)

    // listes en comprehension
    let carres: list[int] = [x * x for x in xs]
    print(carres[-1])
    let pairs: list[int] = [x for x in xs if x % 2 == 0]
    print(len(pairs))
    let n: int = 6
    let grille: list[float] = [float(i) / float(n) for i in range(n)]
    print(grille[3])
    let impairs: list[int] = [i for i in range(1, 20, 3) if i % 2 == 1]
    for v in impairs {
        print(v)
    }
    let longueurs: list[int] = [len(m) for m in noms if "a" in m]
    print(longueurs[-1])
    let c: Classeur = Classeur([1.0, 2.0, 4.0])
    let centres: list[float] = [p - moyenne(c.poids) for p in c.poids]
    print(centres[0])
    let communs: list[int] = [x for x in xs if x in pairs]
    print(len(communs))
    let r: list[float32] = [0.1 * float32(i) for i in range(3)]
    print(r[-1])

    // for k in dico : les cles, dans l'ordre ou elles ont ete ajoutees
    let stock: dict[string,int] = {"pommes": 3, "poires": 5}
    stock["kiwis"] = 2
    let total: int = 0
    for nom in stock {
        print(nom)
        total += stock[nom]
        stock[nom] = 0
    }
    print(total)
    print("kiwis" in stock)
    print("mangues" in stock)
    let cles: list[string] = [cle for cle in stock if cle != "poires"]
    print(len(cles))
    print("tab\tet\\ ligne")
    return 0
}
