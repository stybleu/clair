# La reference de fonctions_valeurs.cl : le meme programme en Python.
# Une lambda de Clair garde la valeur des variables quand elle est faite : ici, lambda x, m=m: ...
class Compteur:
    def __init__(s, n): s.n = n
class Bouton:
    def __init__(s, nom, action): s.nom, s.action = nom, action
class Vec:
    def __init__(s, x, y): s.x, s.y = x, y
    def norme2(s): return s.x * s.x + s.y * s.y
    def plus(s, o): return Vec(s.x + o.x, s.y + o.y)
def carre(x): return x * x
def double(n): return 2 * n
def integrer(f, a, b, n):
    h = (b - a) / float(n)
    s = 0.0
    for i in range(n):
        s += f(a + (float(i) + 0.5) * h)
    return s * h
def ajouteur(n): return lambda x: x + n
def compose(f, g): return lambda x: f(g(x))
def applique_n(f, n, x):
    v = x
    for i in range(n): v = f(v)
    return v
def lecteur(c): return lambda pas: c.n + pas
def note(acc, x): acc.append(x * 10)
def pour_chaque(xs, f):
    for x in xs: f(x)
def derivee(f):
    h = 0.5
    return lambda x: (f(x + h) - f(x - h)) / (2.0 * h)
def appelle(f): return f() + "!"
def show(v):
    if isinstance(v, bool): return "1" if v else "0"
    if isinstance(v, float) and v == int(v): return str(int(v))
    return str(v)
def p(v): print(show(v))

p(int(integrer(carre, 0.0, 3.0, 1000) * 1000.0))
k = 3.0
p(int(integrer(lambda x: k * x, 0.0, 2.0, 8)))
f = carre
p(f(1.5))
f = lambda x: x + 0.25
p(f(1.5))
g = double
p(g(21))
plus3 = ajouteur(3)
p(plus3(4))
p(ajouteur(10)(5))
h = compose(plus3, double)
p(h(5))
p(compose(double, plus3)(5))
p(applique_n(lambda v: v * 3, 4, 1))
p(applique_n(plus3, 5, 0))
d = derivee(carre)
p(d(3.0))
p(appelle(lambda: "salut"))
m = 2
fois_m = lambda x, m=m: x * m
m = 100
p(fois_m(7))
fs = []
for i in range(3):
    fs.append(lambda x, i=i: x + i)
for fi in fs: p(fi(10))
c = Compteur(1)
lire = lecteur(c)
p(lire(0))
c.n = 41
p(lire(1))
acc = []
pour_chaque([1, 2, 3], lambda x: note(acc, x))
p(len(acc))
p(acc[2])
ops = [double, plus3, lambda x: x * x]
for op in ops: p(op(7))
p(ops[2](9))
p(len(ops))
table = {}
table["double"] = double
table["carre"] = lambda x: x * x
table["plus3"] = plus3
for nom in table: p(nom + " " + str(table[nom](6)))
b = Bouton("ok", lambda x: x - 1)
p(b.action(10))
b.action = double
p(b.nom + " " + str(b.action(10)))
boutons = [Bouton("a", plus3), Bouton("b", h)]
for bt in boutons: p(bt.action(1))
vs = [Vec(3.0, 4.0), Vec(1.0, 1.0), Vec(0.0, 2.0)]
for v in sorted(vs, key=Vec.norme2): p(v.x)
somme = Vec.plus
p(somme(vs[0], vs[1]).y)
p(" ".join(map(str.upper, ["ab", "cd"])))
p(" ".join(map(str, [1, 22, 333])))
longueur = len
p(longueur("bonjour"))
p(" ".join(map(str, sorted([3, -1, 2], key=carre))))
en_float = carre
p(float(en_float(4)))
