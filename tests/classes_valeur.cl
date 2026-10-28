// V0.51 : classes valeur (champs jamais changes) et objets sur la pile (qui ne sortent pas de leur fonction)

Class Vec {
    x: float
    y: float

    def plus(o: Vec) -> Vec {
        return Vec(self.x + o.x, self.y + o.y)
    }

    def fois(k: float) -> Vec {
        return Vec(self.x * k, self.y * k)
    }
}

Class Segment {
    a: Vec
    b: Vec
    nom: string

    def milieu() -> Vec {
        return self.a.plus(self.b).fois(0.5)
    }
}

Class Sac {
    xs: list[int]
    k: int

    def met(v: int) -> int {
        self.xs.append(v * self.k)
        return len(self.xs)
    }
}

Class Compteur {
    n: int
    pas: int

    def avance() -> int {
        self.n = self.n + self.pas
        return self.n
    }
}

Class Equipe {
    chef: Compteur
    taille: int
}

Class Gros {
    a: int
    b: int
    c: int
    d: int
    e: int
    f: int
    g: int
    h: int
    i: int
}

Class Compte {
    solde: bigint
    nom: string
}

def double_pas(c: Compteur) -> int {
    c.pas = c.pas * 2
    return c.pas
}

def garde(c: Compteur) -> Compteur {
    return c
}

def somme(g: Gros) -> int {
    return g.a + g.b + g.c + g.d + g.e + g.f + g.g + g.h + g.i
}

def main() -> int {
    // une valeur : copier puis remplacer ne change pas la copie
    let p: Vec = Vec(1.0, 2.0)
    let q: Vec = p
    p = p.plus(Vec(10.0, 20.0))
    print(q.x)
    print(p.x)
    let s: Vec = Vec(0.0, 0.0)
    for i in range(1000) {
        s = s.plus(Vec(1.0, 0.5).fois(i))
    }
    print(s.y)

    // une valeur qui contient des valeurs
    let seg: Segment = Segment(Vec(0.0, 0.0), Vec(4.0, 2.0), "AB")
    let m: Vec = seg.milieu()
    print(m.x)
    print(seg.nom + " " + str(seg.b.y))

    // une valeur avec une liste : la liste reste partagee, comme avant
    let sac: Sac = Sac([], 3)
    let autre: Sac = sac
    sac.met(1)
    autre.met(2)
    print(len(sac.xs))
    print(autre.xs[0] + sac.xs[1])

    // objets sur la pile : changes, passes a une fonction qui les change
    let c: Compteur = Compteur(0, 5)
    c.avance()
    double_pas(c)
    c.avance()
    print(c.n)
    for i in range(3) {
        let t: Compteur = Compteur(i, 1)
        t.avance()
        print(t.n)
    }

    // objets qui sortent : un alias, un champ, une valeur rendue
    let d: Compteur = Compteur(100, 1)
    let alias: Compteur = d
    alias.avance()
    print(d.n)
    let e: Compteur = Compteur(7, 7)
    let eq: Equipe = Equipe(e, 3)
    eq.chef.avance()
    print(e.n)
    let f: Compteur = Compteur(1, 1)
    let g: Compteur = garde(f)
    g.avance()
    print(f.n)

    // plus de 64 octets : un objet, sur la pile
    let gros: Gros = Gros(1, 2, 3, 4, 5, 6, 7, 8, 9)
    print(somme(gros))

    // un champ bigint : le collecteur compte ses champs, l'objet ne change pas de place
    let compte: Compte = Compte(bigint("100000000000000000000") * 3, "Ada")
    print(compte.solde)
    return 0
}
