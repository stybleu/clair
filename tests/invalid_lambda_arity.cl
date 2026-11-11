// V0.75 : une lambda a deux parametres la ou une fonction a un parametre est attendue
def applique(f: def(float) -> float, x: float) -> float {
    return f(x)
}

def main() {
    print(applique(lambda x, y: x + y, 1.0))
}
