// V0.40 : stress et alias sous recuperation memoire pendant les boucles.
def plus_un(v: bigint) -> bigint {
    return v + bigint("1")
}
def main() -> int {
    let x: bigint = bigint("0")
    let alias: bigint = x
    let un: bigint = bigint("1")
    let i: int = 0
    while i < 300000 {
        x = x + un
        i += 1
    }
    if x != bigint("300000") {
        print("ECHEC BIGINT")
        return 1
    }
    if alias != bigint("0") {
        print("ECHEC ALIAS")
        return 2
    }
    let y: bigint = plus_un(x)
    if y != bigint("300001") {
        print("ECHEC FONCTION")
        return 3
    }
    let a: bigfloat = bigfloat("0", 80)
    let b: bigfloat = bigfloat("1", 80)
    let k: int = 0
    while k < 100000 {
        a = a + b
        k += 1
    }
    if a != bigfloat("100000", 80) {
        print("ECHEC BIGFLOAT")
        return 4
    }
    print("MEMOIRE V0.40 : RESULTATS EXACTS")
    return 0
}
