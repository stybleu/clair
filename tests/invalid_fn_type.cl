// V0.75 : une fonction qui prend un texte ne va pas la ou il faut une fonction de float
def crie(s: string) -> string {
    return s.upper()
}

def applique(f: def(float) -> float, x: float) -> float {
    return f(x)
}

def main() {
    print(applique(crie, 1.0))
}
