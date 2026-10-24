// for v in liste : somme des valeurs au-dessus d'un seuil
// noyau: au_dessus
def au_dessus(x: list[int], seuil: int) -> int {
    let total: int = 0
    for v in x {
        if v > seuil {
            total = total + v
        }
    }
    return total
}

def main() -> int {
    let x: list[int] = []
    for i in range(4096) {
        x.append(i * 7919 % 1000)
    }
    let s: int = 0
    for r in range(100000) {
        s = s + au_dessus(x, r % 1000)
    }
    print(s)
    return 0
}
