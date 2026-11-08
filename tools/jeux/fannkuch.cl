// The Computer Language Benchmarks Game : fannkuch-redux
// Pour chaque permutation de 1..n : retourner les crepes du debut jusqu'a ce que 1 soit devant.
// noyau: fannkuch

def fannkuch(n: int) -> int {
    let perm: list[int] = []
    let perm1: list[int] = []
    let count: list[int] = []
    for i in range(n) {
        perm.append(0)
        perm1.append(i)
        count.append(0)
    }
    let max_flips: int = 0
    let checksum: int = 0
    let perm_count: int = 0
    let r: int = n
    while true {
        while r != 1 {
            count[r - 1] = r
            r -= 1
        }
        for i in range(n) {
            perm[i] = perm1[i]
        }
        let flips: int = 0
        let k: int = perm[0]
        while k != 0 {
            let lo: int = 0
            let hi: int = k
            while lo < hi {
                let t: int = perm[lo]
                perm[lo] = perm[hi]
                perm[hi] = t
                lo += 1
                hi -= 1
            }
            flips += 1
            k = perm[0]
        }
        if flips > max_flips {
            max_flips = flips
        }
        if perm_count % 2 == 0 {
            checksum += flips
        } else {
            checksum -= flips
        }
        // permutation suivante
        while true {
            if r == n {
                print(checksum)
                return max_flips
            }
            let perm0: int = perm1[0]
            for i in range(r) {
                perm1[i] = perm1[i + 1]
            }
            perm1[r] = perm0
            count[r] -= 1
            if count[r] > 0 {
                break
            }
            r += 1
        }
        perm_count += 1
    }
    return 0
}

def main() -> int {
    let n: int = input_int()
    let m: int = fannkuch(n)
    print("Pfannkuchen(" + str(n) + ") = " + str(m))
    return 0
}
