// Listes et matrices rendues pendant le programme (sinon environ 900 Mo)

Class Sac {
    items: list[int]
}

def make(n: int, seed: int) -> list[float] {
    let out: list[float] = []
    for i in range(n) {
        out.append(float((i * 7 + seed) % 11))
    }
    return out
}

def somme(xs: list[float]) -> float {
    let s: float = 0.0
    for v in xs {
        s = s + v
    }
    return s
}

def garde(s: Sac, xs: list[int]) -> int {
    s.items = xs
    return 0
}

def premier_pair(n: int) -> float {
    for i in range(n) {
        let essai: list[float] = make(50, i)
        if essai[0] > 4.0 {
            return essai[0] + float(i)
        }
    }
    return 0.0
}

def main() -> int {
    let acc: float = 0.0
    let h: list[float] = []
    let sac: Sac = Sac([0])
    for epoch in range(100000) {
        let grads: list[float] = make(1000, epoch)
        h = make(100, epoch + 1)
        acc = acc + somme(grads) + h[3]
        let m: matrix[float] = matrix(20, 20)
        m[1][2] = 1.0
        acc = acc + m[1][2]
        if epoch % 25000 == 3 {
            continue
        }
        let kept: list[int] = [epoch]
        if epoch % 30000 == 0 {
            garde(sac, kept)
        }
        if epoch == 99998 {
            break
        }
    }
    print(acc)
    print(sac.items[0])
    print(len(h))
    print(premier_pair(10))
    return 0
}
