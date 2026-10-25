// V0.42 : 3 millions de calculs bigint dans une fonction, puis dans une methode
// qui range le resultat dans un champ. Avant la V0.42, la memoire grandissait
// a chaque tour (plus d'un gigaoctet) ; maintenant elle reste petite.
Class Suite {
    valeur: bigint

    def avance(n: int) -> int {
        let x: bigint = self.valeur
        let k: bigint = bigint("3")
        let m: bigint = bigint("1000000000000000000000000000000000000007")
        for i in range(n) {
            x = (x * k + k) % m
        }
        self.valeur = x
        return 0
    }
}

def boucle(n: int) -> bigint {
    let x: bigint = bigint("1")
    let k: bigint = bigint("3")
    let m: bigint = bigint("1000000000000000000000000000000000000007")
    for i in range(n) {
        x = (x * k + k) % m
    }
    return x
}

def main() -> int {
    print(boucle(1500000))
    let s: Suite = Suite(bigint("1"))
    s.avance(1500000)
    print(s.valeur)
    return 0
}
