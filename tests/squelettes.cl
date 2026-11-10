// V0.72 : le squelette d'un programme se compile, comme en Python : pass, def sans
// parentheses ni -> type, return seul, else if ; une fonction typee qui finit sans
// return rend 0, 0.0, false, "" ou une liste vide (avec un avertissement).

Class Moteur {
    pass
}

Class Compteur {
    n: int
    def ajoute {
        self.n = self.n + 1
    }
    def montre() {
        print(self.n)
        return
    }
}

def plus_tard() {
    pass
}

def calcul(x: float) -> float {
    pass
}

def nom_vide() -> string {
    pass
}

def vide() -> list[int] {
    pass
}

def signe(x: int) -> string {
    if x > 0 {
        return "positif"
    } else if x < 0 {
        return "negatif"
    } else {
        return "nul"
    }
}

def bonjour(nom: string) {
    if nom == "" {
        return
    }
    print("bonjour " + nom)
}

def premier_pair(xs: list[int]) -> int {
    for x in xs {
        if x % 2 == 0 {
            return x
        }
    }
}

def jusqu_a_trois() -> int {
    let i: int = 0
    while true {
        i += 1
        if i == 3 {
            return i
        }
    }
}

def main {
    let pass: int = 2
    pass += 1
    print(pass)
    plus_tard()
    print(calcul(2.5) == 0.0)
    print(len(nom_vide()))
    print(len(vide()))
    print(signe(5) + " " + signe(-5) + " " + signe(0))
    bonjour("")
    bonjour("Clair")
    print(premier_pair([1, 3, 4, 6]))
    print(premier_pair([1, 3]))
    print(jusqu_a_trois())
    let c: Compteur = Compteur(0)
    c.ajoute()
    c.ajoute()
    c.montre()
    for i in range(3) {
        pass
    }
    while false {
        pass
    }
    if len("x") == 1 {
        pass
    } else {
        print("jamais")
    }
    return
}
