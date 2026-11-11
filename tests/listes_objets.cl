// V0.74 : les listes d'objets (list[Point]), les listes de listes (list[list[int]], a trois niveaux),
// un arbre (Class Noeud { enfants: list[Noeud] }), [[i, i * i] for i in range(5)] ;
// listes_objets.expected est la sortie du meme programme en Python.
Class Point {
    x: float
    y: float
}

Class Fixe {
    a: int
    b: int
}

Class Noeud {
    nom: string
    enfants: list[Noeud]
}

def nuage(n: int) -> list[Point] {
    let ps: list[Point] = []
    for i in range(n) {
        let p = Point(float(i), float(i * i))
        ps.append(p)
    }
    return ps
}

def ajoute_point(ps: list[Point], x: float) {
    ps.append(Point(x, -x))
}

def arbre(prof: int, nom: string) -> Noeud {
    let n = Noeud(nom, [])
    if prof > 0 {
        for i in range(2) {
            n.enfants.append(arbre(prof - 1, nom + str(i)))
        }
    }
    return n
}

def feuilles(n: Noeud, acc: list[string]) {
    if len(n.enfants) == 0 {
        acc.append(n.nom)
    }
    for e in n.enfants {
        feuilles(e, acc)
    }
}

def matrice(n: int) -> list[list[int]] {
    let m: list[list[int]] = []
    for i in range(n) {
        m.append([0] * n)
        for j in range(n) {
            m[i][j] = i * n + j
        }
    }
    return m
}

def transpose(m: list[list[int]]) -> list[list[int]] {
    let t: list[list[int]] = []
    for j in range(len(m[0])) {
        let ligne: list[int] = []
        for i in range(len(m)) {
            ligne.append(m[i][j])
        }
        t.append(ligne)
    }
    return t
}

def main {
    let ps = nuage(6)
    print(len(ps))
    print(ps[5].y)
    ajoute_point(ps, 2.5)
    print(len(ps))
    print(ps[-1].y)
    let morceau = ps[1:3]
    print(len(morceau))
    morceau[0].x = 100.0
    print(ps[1].x)
    let triple = [Point(0.0, 0.0)] * 3
    triple[0].x = 7.0
    print(triple[2].x)
    let fs: list[Fixe] = []
    for i in range(4) {
        fs.append(Fixe(i, i * 2))
    }
    let s = 0
    for f in fs {
        s += f.a + f.b
    }
    print(s)
    print(fs[3].b)
    let racine = arbre(4, "r")
    let noms: list[string] = []
    feuilles(racine, noms)
    print(len(noms))
    print(noms[0] + " " + noms[15])
    let m = matrice(4)
    print(m[3][2])
    let t = transpose(m)
    print(t[2][3])
    print(matrice(3)[2][1])
    let carres = [[i, i * i] for i in range(5)]
    print(carres[4][1])
    let pts = [Point(float(i), 0.0) for i in range(3)]
    print(pts[2].x)
    let mots: list[list[string]] = []
    for i in range(3) {
        let ligne: list[string] = []
        for j in range(i + 1) {
            ligne.append("m" + str(i) + str(j))
        }
        mots.append(ligne)
    }
    print(mots[2][1])
    let total = 0
    for ligne in mots {
        total += len(ligne)
    }
    print(total)
    let grille = [[0] * 3 for i in range(2)]
    grille[1][2] += 5
    print(grille[1][2])
    let profond: list[list[list[int]]] = [[[1, 2], [3]], [[4]]]
    profond[0][1].append(9)
    profond[0][1][1] = 8
    print(profond[0][1][1])
    print(len(profond[0]))
    let cube: list[list[list[int]]] = []
    for a in range(2) {
        let plan: list[list[int]] = []
        for b in range(2) {
            plan.append([a, b])
        }
        cube.append(plan)
    }
    print(cube[1][0][0] + cube[1][1][1])
    let index: dict[int, list[Point]] = {}
    for i in range(6) {
        let k = i % 3
        if not (k in index) {
            index[k] = []
        }
        index[k].append(Point(float(i), 0.0))
    }
    print(len(index[2]))
    print(index[2][1].x)
    let par_nom: dict[string, Noeud] = {}
    par_nom["r"] = racine
    print(par_nom["r"].enfants[1].enfants[0].nom)
}
