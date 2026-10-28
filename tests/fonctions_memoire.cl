// V0.52 : un million de tours qui fabriquent des listes dans des fonctions ; la memoire ne grandit pas

def paire(a: int) -> list[int] {
    return [a, a + 1]
}

def carres(n: int) -> list[int] {
    return [i * i for i in range(n)]
}

def somme(xs: list[int]) -> int {
    let s: int = 0
    for x in xs {
        s += x
    }
    return s
}

def main() -> int {
    let t: int = 0
    for i in range(1000000) {
        t += somme(paire(i))
        t += carres(8)[i % 8]
        for v in paire(i % 5) {
            t += v
        }
        if 2 in paire(i % 3) {
            t += 1
        }
        t += len(carres(i % 4)[1:])
    }
    print(t)
    return 0
}
