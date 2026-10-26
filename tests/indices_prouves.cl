// V0.44 : un indice qui monte et descend (une pile, un retour en arriere)
// est prouve une seule fois avant la boucle
def compte(n: int) -> int {
    let pile: list[int] = []
    let vu: list[int] = []
    for i in range(n) {
        pile.append(0)
        vu.append(0)
    }
    let k: int = 0
    let feuilles: int = 0
    while k >= 0 {
        vu[k] += 1
        if pile[k] < 2 {
            pile[k] += 1
            if k < n - 1 {
                k += 1
                pile[k] = 0
            } else {
                feuilles += 1
            }
        } else {
            k -= 1
        }
    }
    let total: int = 0
    for v in vu {
        total += v
    }
    print(total)
    return feuilles
}

def main() -> int {
    print(compte(1))
    print(compte(5))
    print(compte(10))
    return 0
}
