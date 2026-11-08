// Entiers machine u8 u16 u32 u64, i8 i16 i32 (i64 = int), operations sur les bits,
// nombres en hexadecimal, binaire et octal

Class Empreinte {
    h: u32
    octets: u64
}

// nombre de bits a 1
def popcount(x: u64) -> int {
    let y: u64 = x
    let n: int = 0
    while y != 0 {
        y &= y - 1
        n += 1
    }
    return n
}

// rotation a gauche sur 32 bits
def rotl(x: u32, r: int) -> u32 {
    return x << r | x >> (32 - r)
}

// generateur pseudo-aleatoire xorshift64
def xorshift(s: u64) -> u64 {
    let x: u64 = s
    x ^= x << 13
    x ^= x >> 7
    x ^= x << 17
    return x
}

// empreinte FNV-1a 32 bits d'une liste d'octets
def fnv1a(e: Empreinte, data: list[u8]) -> u32 {
    for b in data {
        e.h ^= b
        e.h *= 0x0100_0193
        e.octets += 1
    }
    return e.h
}

def melange(a: u8, b: u8) -> u8 {
    return a * 31 + b
}

def main() -> int {
    // calculs qui font le tour, comme en C et NumPy
    let a: u8 = 200
    let b: u8 = 100
    print(a + b)
    print(b - a)
    print(a * 3)
    let c: i8 = 127
    print(c + 1)
    print(-c - 2)
    let w: u16 = 0xFFFF
    print(w * w)
    let d: u32 = 4_000_000_000
    print(d + d)
    let m: u64 = 0xFFFF_FFFF_FFFF_FFFF
    print(m)
    print(m + 2)
    let s: i32 = -2147483648
    print(s - 1)
    print(s / 7)
    print(s % 7)
    print(a / 7)
    print(a % 7)

    // operations sur les bits
    print(a & 0x0F)
    print(a | 0b0000_0101)
    print(a ^ 0xFF)
    print(~a)
    print(~c)
    print(a << 1)
    print(a >> 3)
    print(1 << 40)
    print(-9 >> 1)
    let k: int = 70
    print(a << k)
    print(-1 >> k)
    print(0o755)
    print(0x7FFF_FFFF_FFFF_FFFF)
    print(1_000.5 + 0.25)
    // la priorite de Python : a & b == 64 veut dire (a & b) == 64
    print(a & b == 64)
    print(1 + 2 << 3)
    print(6 & 3 | 8 ^ 1)

    // conversions : on garde les bits du bas
    print(u8(300))
    print(i8(200))
    print(u8(-1))
    print(i16(70000))
    print(u32(-1))
    print(u16(3.9))
    print(int(m >> 1))
    print(i64(d))

    // elargir ne perd rien et se fait seul
    let grand: u64 = d
    let signe: int = d
    let moyen: i16 = a
    print(grand + d)
    print(signe + moyen)

    // abs, min, max
    print(abs(c))
    print(abs(i8(-128)))
    print(min(a, b))
    print(max(w, a))

    // fonctions, listes, classes
    print(popcount(m))
    print(popcount(0b1011_0000))
    print(rotl(0x8000_0001, 4))
    print(rotl(d, 0))
    let x: u64 = 88172645463325252
    for i in range(3) {
        x = xorshift(x)
        print(x)
    }
    let texte: list[u8] = [104, 101, 108, 108, 111]
    let e: Empreinte = Empreinte(0x811C9DC5, 0)
    print(fnv1a(e, texte))
    print(e.octets)
    print(melange(texte[0], texte[4]))
    texte[1] += 200
    print(texte[1])
    let somme: u16 = 0
    for i, v in enumerate(texte[1:4]) {
        somme += v * u8(i + 1)
    }
    print(somme)
    let img: matrix[u8] = matrix(2, 3)
    for i in range(2) {
        for j in range(3) {
            img[i][j] = u8(i * 100 + j * 90)
        }
    }
    print(img[1][2] + img[0][1])
    let pixels: matrix[i16] = matrix(1, 2)
    pixels[0][1] = -300
    print(pixels[0][1] * 200)

    // avec des nombres a virgule
    let f: float32 = 1.5
    print(f + a)
    print(f * d)
    print(float(m))
    return 0
}
