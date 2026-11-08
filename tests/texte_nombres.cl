// V0.43 : str() et format(), comme en Python
def main() -> int {
    let n: int = 42
    let f: float = 0.1
    let g: float32 = 0.1
    let u: u64 = 0xFFFFFFFFFFFFFFFF
    let b: bigint = bigint(n) * bigint("1000000000000000000000")
    print("n=" + str(n) + " f=" + str(f) + " g=" + str(g) + " u=" + str(u))
    print(str(100.0) + " " + str(1e20) + " " + str(1.5e-7) + " " + str(true) + " " + str(b))
    print(str(-0.0) + " " + str(123456789.0) + " " + str(1e16) + " " + str(0.0001) + " " + str(i8(-5)))
    let grand: float32 = 12345678.0
    let petit: float32 = 0.0001
    print(str(grand) + " " + str(petit))
    print(format(3.14159265358979, ".9f") + "|" + format(n, "5d") + "|" + format(2.5, "10.3e") + "|" + format(7, ".2f"))
    print(format(0.000123456, ".3g") + "|" + format(-0.169075164, "f") + "|" + format(1.0, "") + "|" + format(u, "d"))
    print(format(2.675, ".2f") + "|" + format(0.5, ".0f") + "|" + format(1.5, ".0f") + "|" + format(-7, "4d"))
    let ligne: string = ""
    for i in range(5) {
        ligne = ligne + str(i * i) + ","
    }
    print(ligne)
    return 0
}
