Class Moteur {
    puissance: int

    def doubler() -> int {
        return self.puissance * 2
    }
}

def main() -> int {
    let moteur: Moteur = Moteur(21)
    print(moteur.doubler())
    return 0
}