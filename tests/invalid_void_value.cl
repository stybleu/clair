// V0.72 : une fonction qui n'a aucun return avec une valeur ne rend rien : elle ne sert pas de valeur
def rien() {
    pass
}

def main {
    let x: int = rien()
}
