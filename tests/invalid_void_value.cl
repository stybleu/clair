// V0.72 : une fonction sans -> type ne rend rien : elle ne sert pas de valeur
def rien() {
    pass
}

def main {
    let x: int = rien()
}
