// V0.75 : les fonctions comme valeurs, lambda, les fermetures, les listes et les champs de fonctions ;
// fonctions_valeurs.expected est la sortie du meme programme en Python (tests/fonctions_valeurs.py).
Class Compteur {
    n: int
}

Class Bouton {
    nom: string
    action: def(int) -> int
}

Class Vec {
    x: float
    y: float
    def norme2 {
        return self.x * self.x + self.y * self.y
    }
    def plus(o: Vec) -> Vec {
        return Vec(self.x + o.x, self.y + o.y)
    }
}

def carre(x: float) -> float {
    return x * x
}

def double(n: int) -> int {
    return 2 * n
}

def integrer(f: def(float) -> float, a: float, b: float, n: int) -> float {
    // la methode du point milieu
    let h = (b - a) / float(n)
    let s = 0.0
    for i in range(n) {
        s += f(a + (float(i) + 0.5) * h)
    }
    return s * h
}

def ajouteur(n: int) -> def(int) -> int {
    return lambda x: x + n
}

def compose(f: def(int) -> int, g: def(int) -> int) -> def(int) -> int {
    return lambda x: f(g(x))
}

def applique_n(f: def(int) -> int, n: int, x: int) -> int {
    let v = x
    for i in range(n) {
        v = f(v)
    }
    return v
}

def lecteur(c: Compteur) -> def(int) -> int {
    // c est un objet : la lambda voit ses changements, comme en Python
    return lambda pas: c.n + pas
}

def note(acc: list[int], x: int) {
    acc.append(x * 10)
}

def pour_chaque(xs: list[int], f: def(int)) {
    for x in xs {
        f(x)
    }
}

def derivee(f: def(float) -> float) -> def(float) -> float {
    let h = 0.5
    return lambda x: (f(x + h) - f(x - h)) / (2.0 * h)
}

def appelle(f: def() -> string) -> string {
    return f() + "!"
}

def main() {
    // une fonction donnee a une autre, une lambda qui garde k
    print(int(integrer(carre, 0.0, 3.0, 1000) * 1000.0))
    let k = 3.0
    print(int(integrer(lambda x: k * x, 0.0, 2.0, 8)))
    let f: def(float) -> float = carre
    print(f(1.5))
    f = lambda x: x + 0.25
    print(f(1.5))
    let g = double
    print(g(21))

    // des fonctions qui en font d'autres
    let plus3 = ajouteur(3)
    print(plus3(4))
    print(ajouteur(10)(5))
    let h = compose(plus3, double)
    print(h(5))
    print(compose(double, plus3)(5))
    print(applique_n(lambda v: v * 3, 4, 1))
    print(applique_n(plus3, 5, 0))
    let d = derivee(carre)
    print(d(3.0))
    print(appelle(lambda: "salut"))

    // une lambda garde la valeur qu'avaient les variables quand elle a ete faite
    let m = 2
    let fois_m: def(int) -> int = lambda x: x * m
    m = 100
    print(fois_m(7))
    let fs: list[def(int) -> int] = []
    for i in range(3) {
        fs.append(lambda x: x + i)
    }
    for fi in fs {
        print(fi(10))
    }

    // un objet garde est partage : ses changements se voient
    let c = Compteur(1)
    let lire = lecteur(c)
    print(lire(0))
    c.n = 41
    print(lire(1))

    // une fonction qui ne rend rien
    let acc: list[int] = []
    pour_chaque([1, 2, 3], lambda x: note(acc, x))
    print(len(acc))
    print(acc[2])

    // listes, dictionnaires et champs de fonctions
    let ops: list[def(int) -> int] = [double, plus3, lambda x: x * x]
    for op in ops {
        print(op(7))
    }
    print(ops[2](9))
    print(len(ops))
    let table: dict[string, def(int) -> int] = {}
    table["double"] = double
    table["carre"] = lambda x: x * x
    table["plus3"] = plus3
    for nom in table {
        print(nom + " " + str(table[nom](6)))
    }
    let b = Bouton("ok", lambda x: x - 1)
    print(b.action(10))
    b.action = double
    print(b.nom + " " + str(b.action(10)))
    let boutons = [Bouton("a", plus3), Bouton("b", h)]
    for bt in boutons {
        print(bt.action(1))
    }

    // des methodes et des fonctions de Clair comme valeurs
    let vs = [Vec(3.0, 4.0), Vec(1.0, 1.0), Vec(0.0, 2.0)]
    for v in sorted(vs, key=Vec.norme2) {
        print(v.x)
    }
    let somme: def(Vec, Vec) -> Vec = Vec.plus
    print(somme(vs[0], vs[1]).y)
    print(" ".join(map(str.upper, ["ab", "cd"])))
    print(" ".join(map(str, [1, 22, 333])))
    let longueur: def(string) -> int = len
    print(longueur("bonjour"))
    // carre prend un float : avec des entiers, Clair passe par une conversion
    print(" ".join(map(str, sorted([3, -1, 2], key=carre))))
    let en_float: def(int) -> float = carre
    print(en_float(4))
}
