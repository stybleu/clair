# La reference de tris.cl : le meme programme en Python (3.12 ou plus : sum() des flottants de Neumaier).
class Ville:
    def __init__(s, nom, habitants, surface): s.nom, s.habitants, s.surface = nom, habitants, surface
def show(v):
    if isinstance(v, bool): return "1" if v else "0"
    if isinstance(v, float) and v == int(v): return str(int(v))
    return str(v)
def p(v): print(show(v))
def montre(xs): p(" ".join(map(str, xs)))
def montre_f(xs): p(" ".join(map(str, xs)))
def alea(graine, n):
    xs = []
    x = graine
    for i in range(n):
        x = (x * 1103515245 + 12345) % 2147483648
        xs.append(x % 1000)
    return xs
def empreinte(xs):
    h = 0
    for i in range(len(xs)):
        h = (h * 31 + xs[i] * (i + 1)) % 1000000007
    return h

xs = [5, 3, 9, 1, 3, -2, 7]
montre(sorted(xs))
montre(sorted(xs, reverse=True))
montre(xs)
p(min(xs)); p(max(xs)); p(sum(xs))
montre(list(map(lambda x: x * x, xs)))
montre(list(filter(lambda x: x % 2 == 1, xs)))
seuil = 4
montre(list(filter(lambda x: x > seuil, xs)))
montre(sorted(xs, key=abs))
montre(sorted(xs, key=lambda x: -x))
xs.sort(); montre(xs)
xs.sort(reverse=True); montre(xs)
vide = []
p(len(sorted(vide))); p(sum(vide)); vide.sort()

mots = ["poire", "Banane", "été", "abricot", "kiwi", "Zèbre", "eau", "à"]
p(" ".join(sorted(mots)))
p(" ".join(sorted(mots, key=str.lower)))
p(" ".join(sorted(mots, key=len)))
p(" ".join(sorted(mots, key=len, reverse=True)))
p(min(mots)); p(max(mots)); p(max(mots, key=len)); p(min(mots, key=len))
p(" ".join(map(str.upper, mots)))
p(" ".join(filter(lambda m: "e" in m, mots)))
p(" ".join(map(lambda m: m[0], sorted(mots))))
montre(list(map(len, mots)))

villes = [Ville("Lyon", 522000, 47.87), Ville("Paris", 2103000, 105.4),
          Ville("Nice", 342000, 71.92), Ville("Lille", 236000, 34.51)]
for v in sorted(villes, key=lambda v: v.habitants): p(v.nom)
for v in sorted(villes, key=lambda v: float(v.habitants) / v.surface, reverse=True): p(v.nom)
p(max(villes, key=lambda v: v.surface).nom)
p(min(villes, key=lambda v: v.nom).nom)
p(sum(map(lambda v: v.habitants, villes)))
p(len(list(filter(lambda v: v.habitants > 300000, villes))))
villes.sort(key=lambda v: v.nom)
p(villes[0].nom + " " + villes[3].nom)

bs = [True, False, True, True]
p(sum(bs)); p(min(bs)); p(max(bs))
fs = [2.5, -1.25, 0.5, 10.75, -0.0]
montre_f(sorted(fs))
montre_f(sorted(fs, reverse=True))
p(sum(fs)); p(max(fs))
dixiemes = [0.1] * 10
p(sum(dixiemes) == 1.0)
p(sum([1.0e100, 1.0, -1.0e100]) == 1.0)
p(sum([0.1, 0.2, 0.3]) == 0.6)

grille = [[3, 1], [1, 2, 3], [2], [5, 5]]
for ligne in sorted(grille, key=len): montre(ligne)
montre(max(grille, key=sum))
montre(min(grille, key=len))
montre(list(map(sum, grille)))
montre(list(map(max, grille)))

stock = {"pommes": 12, "kiwis": 30, "poires": 7}
for k in sorted(stock): p(k + " " + str(stock[k]))
p(max(stock, key=lambda k: stock[k]))
p(min(stock))
cubes = {}
for i in range(-3, 4): cubes[i * i * i] = i
montre(sorted(cubes))
montre(sorted(cubes, reverse=True))
p(sum(cubes))

gs = alea(42, 3000)
ts = sorted(gs)
ok = True
for i in range(1, len(ts)):
    if ts[i - 1] > ts[i]: ok = False
p(ok); p(ts[0]); p(ts[1500]); p(ts[2999]); p(sum(ts)); p(empreinte(ts))
ids = [i for i in range(3000)]
p(empreinte(sorted(ids, key=lambda i: gs[i] % 10)))
p(empreinte(sorted(ids, key=lambda i: gs[i] % 10, reverse=True)))
copie = gs[0:3000]
copie.sort(key=lambda g: g % 7)
p(empreinte(copie))
p(empreinte(gs))
