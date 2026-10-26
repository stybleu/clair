// V0.46 : lire l'entree, ecrire sans aller a la ligne, les octets, [x] * n
def main() -> int {
    let nom: string = input("Nom : ")
    print("Bonjour " + nom)
    let n: int = int(input())
    let suite: list[string] = read_lines()          // le reste de l'entree
    print(len(suite))
    for l in suite {
        write("[" + l + "] ")
    }
    write("\n")

    // texte et octets
    let b: list[u8] = encode("héllo")
    print(len(b))
    write(b)
    write("\n")
    write(b, 3)
    write("|\n")
    print(decode(b))
    print(ord("A"))
    print(ord("é"))
    print(ord("€"))
    print(chr(65) + chr(233) + chr(8364) + chr(128512))

    // des listes toutes faites
    let zeros: list[u8] = [0] * n
    let motif: list[int] = [1, 2] * 3
    let rien: list[int] = [7] * -2
    let moities: list[float] = 2 * [0.5]
    print(len(zeros))
    print(len(motif))
    print(motif[5])
    print(len(rien))
    print(moities[1])

    // fichiers
    write_file("build/es_texte.txt", "un\r\ndeux\n")
    let t: string = read_text("build/es_texte.txt")  // \r\n devient \n, comme en Python
    print(len(t))
    write(t)
    let o: list[u8] = read_bytes("build/es_texte.txt")
    print(len(o))
    print(o[2])
    write_file("build/es_copie.bin", o)
    print(len(read_bytes("build/es_copie.bin")))

    // une table lue avec des octets : le brin complementaire, a l'envers
    let comp: list[u8] = [0] * 256
    for i in range(256) {
        comp[i] = u8(i)
    }
    let de: list[u8] = encode("ACGT")
    let vers: list[u8] = encode("TGCA")
    for i in range(4) {
        comp[de[i]] = vers[i]
    }
    let adn: list[u8] = encode("GATTACA")
    let brin: list[u8] = [0] * len(adn)
    for i in range(len(adn)) {
        brin[i] = comp[adn[len(adn) - 1 - i]]
    }
    print(decode(brin))
    return 0
}
