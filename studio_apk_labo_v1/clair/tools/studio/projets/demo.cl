
Class Compteur {
    valeur: int

    def doubler() -> int {
        return self.valeur * 2
    }
}

def main() -> int {
    let compteur: Compteur = Compteur(21)

    print(compteur.doubler())

    return 0
}
