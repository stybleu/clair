// V0.75 : trier des objets demande une cle, key=lambda p: p.x
Class Point {
    x: float
    y: float
}

def main() {
    let ps = [Point(2.0, 1.0), Point(1.0, 3.0)]
    for p in sorted(ps) {
        print(p.x)
    }
}
