// V0.54 : un million de tours qui fabriquent des textes et des objets ; la memoire ne grandit pas

Class Fiche {
    nom: string
    age: int

    def ligne() -> string {
        return self.nom + " (" + str(self.age) + " ans)"
    }
}

def nom(i: int) -> string {
    return "objet " + str(i)
}

def main() -> int {
    let compte: dict[string, int] = {}
    let total: int = 0
    for i in range(1000000) {
        let s: string = nom(i)
        total += len(s)
        let f: Fiche = Fiche("Ada " + str(i % 100), i % 90)
        total += len(f.ligne())
        for mot in split("le chat " + str(i % 5) + " dort", " ") {
            if mot in compte {
                compte[mot] += 1
            } else {
                compte[mot] = 1
            }
        }
    }
    print(total)
    print(compte["le"])
    print(compte["3"])
    print(len(compte))
    return 0
}
