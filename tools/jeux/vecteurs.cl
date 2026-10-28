// vecteurs : une balle lancee dans l'air (pesanteur et frottement), n pas de temps.
// Chaque pas fabrique six vecteurs de la classe Vec : depuis la V0.51 ce sont
// des valeurs, comme en C, au lieu d'objets gardes en memoire jusqu'a la fin.
// noyau: vole

Class Vec {
    x: float
    y: float
    z: float

    def plus(o: Vec) -> Vec {
        return Vec(self.x + o.x, self.y + o.y, self.z + o.z)
    }

    def fois(k: float) -> Vec {
        return Vec(self.x * k, self.y * k, self.z * k)
    }

    def norme() -> float {
        return sqrt(self.x * self.x + self.y * self.y + self.z * self.z)
    }
}

def vole(n: int, dt: float) -> Vec {
    let p: Vec = Vec(0.0, 0.0, 1.5)
    let v: Vec = Vec(30.0, 20.0, 25.0)
    let g: Vec = Vec(0.0, 0.0, -9.81)
    let chemin: float = 0.0
    for i in range(n) {
        let a: Vec = g.plus(v.fois(-0.02 * v.norme()))
        v = v.plus(a.fois(dt))
        let d: Vec = v.fois(dt)
        p = p.plus(d)
        chemin += d.norme()
    }
    print(format(chemin, ".9f"))
    return p
}

def main() -> int {
    let n: int = input_int()
    let p: Vec = vole(n, 4.0 / n)
    print(format(p.x, ".9f"))
    print(format(p.y, ".9f"))
    print(format(p.z, ".9f"))
    return 0
}
