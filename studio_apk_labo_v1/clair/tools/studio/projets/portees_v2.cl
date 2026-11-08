def calculer(nombre: int) -> int {
    let resultat: int = nombre + 1

    if true {
        let temporaire: int = resultat
        resultat = temporaire
    }

    // Le commentaire ne declare aucune variable.
    return resultat
}

def main() -> int {
    let resultat: int = calculer(41)
    print(resultat)
    return 0
}
