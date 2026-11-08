// PLB2 : nqueen
// Nombre de facons de placer n reines sur un echiquier n x n sans qu'elles se menacent,
// avec des operations sur les bits (comme le programme C de PLB2).
// noyau: reines

def reines(n: int) -> int {
    let a: list[int] = []
    let l: list[u32] = []
    let c: list[u32] = []
    let r: list[u32] = []
    for i in range(n) {
        a.append(-1)
        l.append(u32(0))
        c.append(u32(0))
        r.append(u32(0))
    }
    let y0: u32 = (u32(1) << n) - u32(1)
    let solutions: int = 0
    let k: int = 0
    while k >= 0 {
        let y: u32 = (l[k] | c[k] | r[k]) & y0
        if ((y ^ y0) >> (a[k] + 1)) != u32(0) {
            let i: int = a[k] + 1
            while i < n {
                if (y & (u32(1) << i)) == u32(0) {
                    break
                }
                i += 1
            }
            if k < n - 1 {
                let z: u32 = u32(1) << i
                a[k] = i
                k += 1
                l[k] = (l[k - 1] | z) << 1
                c[k] = c[k - 1] | z
                r[k] = (r[k - 1] | z) >> 1
            } else {
                solutions += 1
                k -= 1
            }
        } else {
            a[k] = -1
            k -= 1
        }
    }
    return solutions
}

def main() -> int {
    let n: int = input_int()
    print(reines(n))
    return 0
}
