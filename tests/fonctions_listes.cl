// V0.52 : les listes faites par une fonction et utilisees tout de suite sont rendues a la memoire

Class Sac {
    xs: list[int]
    n: int
}

def paire(a: int) -> list[int] {
    return [a, a + 1]
}

def carres(n: int) -> list[int] {
    return [i * i for i in range(n)]
}

def queue(xs: list[int]) -> list[int] {
    return xs[1:]
}

def suite(a: int) -> list[int] {
    if a < 0 {
        return []
    }
    let r: list[int] = [a]
    r.append(a * 10)
    return r
}

def somme(xs: list[int]) -> int {
    let s: int = 0
    for x in xs {
        s += x
    }
    return s
}

def somme_rec(xs: list[int]) -> int {
    if len(xs) == 0 {
        return 0
    }
    return xs[0] + somme_rec(xs[1:])
}

def ajoute(xs: list[int], v: int) -> int {
    xs.append(v)
    return len(xs)
}

def garde(xs: list[int]) -> list[int] {
    return xs
}

def emballe(xs: list[int]) -> Sac {
    return Sac(xs, len(xs))
}

def premier_grand(n: int, seuil: int) -> int {
    for v in carres(n) {
        if v > seuil {
            return v
        }
    }
    return -1
}

def main() -> int {
    // utilisees tout de suite : argument, [i], len, in, condition, for ... in
    let t: int = 0
    for i in range(1000) {
        t += somme(paire(i))
        t += paire(i)[1]
        t += len(carres(i % 7))
        t += somme(queue(queue(carres(5))))
        if 3 in paire(i) {
            t += 1000
        }
        if somme(suite(i)) > 5000 {
            t += 1
        }
        for v in suite(i % 3) {
            t += v
        }
        ajoute(paire(i), 5)
    }
    print(t)
    print(somme_rec(carres(30)))
    print(premier_grand(10, 20))
    print(premier_grand(3, 20))
    print(len(suite(-1)))

    // gardees : un deuxieme nom, une valeur rendue, un champ d'objet
    let a: list[int] = paire(7)
    let b: list[int] = garde(a)
    b.append(9)
    print(len(a))
    let s: Sac = emballe(paire(4))
    print(somme(s.xs) + s.n)
    let c: list[int] = garde(paire(1))
    c.append(2)
    print(somme(c))

    // des mots
    let mots: int = 0
    for i in range(100) {
        mots += len(split("un deux trois", " "))
        mots += len(split("a,b", ",")[0])
    }
    print(mots)
    return 0
}
