// V0.74 : 'in' ne cherche pas encore un objet dans une liste d'objets (Python compare avec ==)
Class P {
    x: int
}

def main {
    let ps = [P(1)]
    print(P(1) in ps)
}
