// The Computer Language Benchmarks Game : reverse-complement
// Lit des sequences d'ADN (la sortie de fasta) et ecrit chacune a l'envers, chaque lettre
// remplacee par sa complementaire (A <-> T, C <-> G...), en lignes de 60 lettres.
// noyau: inverse

def inverse(seq: list[u8], n: int, comp: list[u8], ligne: list[u8]) -> int {
    let j: int = n - 1
    while j >= 0 {
        let m: int = min(j + 1, 60)
        for i in range(m) {
            ligne[i] = comp[seq[j - i]]
        }
        ligne[m] = 10
        write(ligne, m + 1)
        j -= m
    }
    return 0
}

def main() -> int {
    let data: list[u8] = read_bytes()
    let comp: list[u8] = []
    for i in range(256) {
        comp.append(u8(i))
    }
    let de: list[u8] = encode("ACBDGHKMNSRUTWVYacbdghkmnsrutwvy")
    let vers: list[u8] = encode("TGVHCDMKNSYAAWBRTGVHCDMKNSYAAWBR")
    for i in range(len(de)) {
        comp[de[i]] = vers[i]
    }
    let ligne: list[u8] = [0] * 61
    let n: int = len(data)
    let seq: list[u8] = [0] * n
    let i: int = 0
    while i < n {
        // la ligne de titre (">ONE ..."), recopiee telle quelle
        let debut: int = i
        while i < n and data[i] != 10 {
            i += 1
        }
        write(data[debut:i])
        write("\n")
        i += 1
        // la sequence jusqu'au titre suivant : chaque ligne recopiee sans sa fin de ligne
        let k: int = 0
        while i < n and data[i] != 62 {
            let fin: int = i
            while fin < n {
                if data[fin] == 10 {
                    break
                }
                fin += 1
            }
            for t in range(i, fin) {
                seq[k + t - i] = data[t]
            }
            k += fin - i
            i = fin + 1
        }
        inverse(seq, k, comp, ligne)
    }
    return 0
}
