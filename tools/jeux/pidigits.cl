// The Computer Language Benchmarks Game : pidigits
// Les n premiers chiffres de pi, par l'algorithme "spigot" de Gibbons, sur des bigint.
// noyau: chiffres

def chiffres(n: int) -> int {
    let acc: bigint = bigint(0)
    let den: bigint = bigint(1)
    let num: bigint = bigint(1)
    let k: int = 0
    let i: int = 0
    let ligne: string = ""
    while i < n {
        k += 1
        let k2: int = 2 * k + 1
        acc = (acc + num * 2) * k2
        den = den * k2
        num = num * k
        if num > acc {
            continue
        }
        let d: bigint = (num * 3 + acc) / den
        if d != (num * 4 + acc) / den {
            continue
        }
        ligne = ligne + str(d)
        i += 1
        if i % 10 == 0 {
            print(ligne + "\t:" + str(i))
            ligne = ""
        }
        acc = (acc - den * d) * 10
        num = num * 10
    }
    if i % 10 != 0 {
        while len(ligne) < 10 {
            ligne = ligne + " "
        }
        print(ligne + "\t:" + str(i))
    }
    return 0
}

def main() -> int {
    chiffres(input_int())
    return 0
}
