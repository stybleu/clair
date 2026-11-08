
Class Moteur {
    puissance: int

    def doubler() -> int {
        return self.puissance * 2
    }
}

Class Machine {
    moteur: Moteur
}

def main() -> int {
    let moteur: Moteur = Moteur(21)
    let machine: Machine = Machine(moteur)
    self
    

    return 0
}
