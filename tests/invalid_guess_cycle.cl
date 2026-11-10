// V0.73 : deux fonctions qui ne rendent que l'une l'autre : rien a deviner
def a(n: int) {
    return b(n)
}

def b(n: int) {
    return a(n)
}

def main {
    print(a(1))
}
