// moyenne de deux images en niveaux de gris, un octet (u8) par pixel
def moyenne(a: list[u8], b: list[u8], c: list[u8]) -> int {
    for i in range(len(a)) {
        c[i] = (a[i] >> 1) + (b[i] >> 1) + (a[i] & b[i] & 1)
    }
    return 0
}

def main() -> int {
    let a: list[u8] = []
    let b: list[u8] = []
    let c: list[u8] = []
    for i in range(65536) {
        a.append(u8(i * 7))
        b.append(u8(i * 13 + 5))
        c.append(0)
    }
    for r in range(60000) {
        moyenne(a, b, c)
        a[r % 65536] = c[r * 3 % 65536]
    }
    let total: int = 0
    for v in c {
        total += v
    }
    print(total)
    return 0
}
