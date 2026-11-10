// V0.73 : les types devines, comme Python : let x = 5 donne un int, et une fonction
// sans -> type rend le type de ses return (recursive, mutuelle, methode...), ou rien.
Class Point {
    x: float
    y: float
    def norme2 {
        return self.x * self.x + self.y * self.y
    }
    def decale(d: float) {
        return Point(self.x + d, self.y + d)
    }
}

def double(x: int) {
    return x * 2
}

def fact(n: int) {
    if n < 2 {
        return 1
    }
    return n * fact(n - 1)
}

def fib(n: int) {
    if n > 1 {
        return fib(n - 1) + fib(n - 2)
    }
    return n
}

def moyenne(xs: list[float]) {
    let total = 0.0
    for x in xs {
        total += x
    }
    return total / float(len(xs))
}

def melange(x: int) {
    if x > 0 {
        return 1
    }
    return 2.5
}

def salut(nom: string) {
    return "bonjour " + nom
}

def premiers(n: int) {
    let res: list[int] = []
    for i in range(2, n) {
        let premier = true
        for d in range(2, i) {
            if i % d == 0 {
                premier = false
                break
            }
        }
        if premier {
            res.append(i)
        }
    }
    return res
}

def via_autre(x: int) {
    return double(x) + 1
}

def pair(n: int) {
    if n == 0 {
        return true
    }
    return impair(n - 1)
}

def impair(n: int) {
    if n == 0 {
        return false
    }
    return pair(n - 1)
}

def rien() {
    print("rien")
}

def main {
    let x = 5
    let f = 2.5
    let s = "été"
    let xs = [1.0, 2.0, 4.5]
    let d = {"a": 1, "b": 2}
    let p = Point(3.0, 4.0)
    let b = x > 3
    print(x + 1)
    print(f * 2.0)
    print(len(s))
    print(len(xs))
    print(d["b"])
    print(p.norme2())
    let q = p.decale(1.0)
    print(q.x)
    print(b)
    print(double(21))
    print(fact(10))
    print(fib(15))
    print(moyenne(xs))
    print(melange(1))
    print(salut("Léa"))
    print(len(premiers(30)))
    print(via_autre(4))
    print(pair(10))
    rien()
    let mots = "un deux".split()
    print(len(mots))
    let n = len(s)
    print(n)
}
