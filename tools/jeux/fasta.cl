// The Computer Language Benchmarks Game : fasta
// Trois sequences d'ADN : une repetee (ALU), deux tirees au hasard (generateur congruentiel
// lineaire du jeu), ecrites en lignes de 60 lettres.
// noyau: au_hasard

def repete(seq: list[u8], n: int, ligne: list[u8]) -> int {
    let taille: int = len(seq)
    let pos: int = 0
    let reste: int = n
    while reste > 0 {
        let m: int = min(reste, 60)
        for i in range(m) {
            ligne[i] = seq[pos]
            pos += 1
            if pos == taille {
                pos = 0
            }
        }
        ligne[m] = 10
        write(ligne, m + 1)
        reste -= m
    }
    return 0
}

// les lettres tirees au hasard selon leurs probabilites ; renvoie la graine suivante
def au_hasard(lettres: list[u8], cumul: list[float], n: int, graine: int, ligne: list[u8]) -> int {
    let dernier: int = len(cumul) - 1
    let g: int = graine
    let reste: int = n
    while reste > 0 {
        let m: int = min(reste, 60)
        for i in range(m) {
            g = (g * 3877 + 29573) % 139968
            let r: float = float(g) / 139968.0
            let k: int = 0
            while k < dernier and r >= cumul[k] {
                k += 1
            }
            ligne[i] = lettres[k]
        }
        ligne[m] = 10
        write(ligne, m + 1)
        reste -= m
    }
    return g
}

def cumule(p: list[float]) -> list[float] {
    let c: list[float] = []
    let s: float = 0.0
    for x in p {
        s += x
        c.append(s)
    }
    return c
}

def main() -> int {
    let n: int = input_int()
    let alu: list[u8] = encode("GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA")
    let ligne: list[u8] = [0] * 61
    let iub: list[u8] = encode("acgtBDHKMNRSVWY")
    let p_iub: list[float] = cumule([0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02])
    let homo: list[u8] = encode("acgt")
    let p_homo: list[float] = cumule([0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008])

    print(">ONE Homo sapiens alu")
    repete(alu, n * 2, ligne)
    print(">TWO IUB ambiguity codes")
    let g: int = au_hasard(iub, p_iub, n * 3, 42, ligne)
    print(">THREE Homo sapiens frequency")
    au_hasard(homo, p_homo, n * 5, g, ligne)
    return 0
}
