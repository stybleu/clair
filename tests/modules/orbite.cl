// V0.47 : un module. Depuis un programme : import orbite, puis orbite.periode(...)
import math as m
from astro import GM_soleil, UA

Class Astre {
    nom: string
    gm: float

    // une methode qui appelle une fonction du module, sans prefixe
    def vitesse(r: float) -> float {
        return vitesse_circulaire(self.gm, r)
    }
}

def vitesse_circulaire(gm: float, r: float) -> float {
    return m.sqrt(gm / r)
}

def periode(gm: float, a: float) -> float {
    return 2.0 * m.pi * m.sqrt(a * a * a / gm)
}

def vitesse_terre() -> float {
    return vitesse_circulaire(GM_soleil, UA)          // une fonction du module lui-meme
}

// lance seul (clair orbite.cl), le module se teste ; importe, ce main est laisse de cote
def main() -> int {
    print("orbite seul")
    print(vitesse_terre())
    return 0
}
