# La reference de tuples.cl : le meme programme en Python.
class Ville:
    def __init__(s, nom, pos, habitants): s.nom, s.pos, s.habitants = nom, pos, habitants
def show(v):
    if isinstance(v, bool): return "1" if v else "0"
    if isinstance(v, float) and v == int(v): return str(int(v))
    return str(v)
def p(v): print(show(v))
def divise(a, b): return a // b, a % b
def bornes(xs): return min(xs), max(xs)
def stats(xs):
    s = 0.0
    for x in xs: s += x
    return s, len(xs)
def fib(n):
    a, b = 0, 1
    for i in range(n): a, b = b, a + b
    return a
def chemins(i, j, memo):
    if i == 0 or j == 0: return 1
    if (i, j) in memo: return memo[(i, j)]
    n = chemins(i - 1, j, memo) + chemins(i, j - 1, memo)
    memo[(i, j)] = n
    return n
def voisins(pt):
    x, y = pt
    return [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
def largeur(depart, murs, n):
    dist = {depart: 0}
    file = [depart]
    i = 0
    while i < len(file):
        pt = file[i]
        i += 1
        for q in voisins(pt):
            x, y = q
            if x >= 0 and x < n and y >= 0 and y < n and q not in murs and q not in dist:
                dist[q] = dist[pt] + 1
                file.append(q)
    return dist

t = (3, "trois", 2.5, True)
for v in (t[0], t[1], t[2], t[3], t[-1], len(t)): p(v)
q, r = divise(17, 5); p(q * 10 + r)
lo, hi = bornes([4, 9, -2, 7]); p(lo); p(hi)
s, n = stats([1.5, 2.5, 4.25]); p(s); p(n)
p(fib(90))
_, milieu, _ = (1, 2, 3); p(milieu)
a, b = "gauche", "droite"
a, b = b, a
p(a + " " + b)
xs = [1, 2, 3, 4, 5]
xs[0], xs[4] = xs[4], xs[0]
p(" ".join(map(str, xs)))
gens = [("Léa", 30), ("Bo", 25), ("Ana", 30), ("Zoé", 25), ("Éric", 41)]
for nom, age in gens: p(nom + " " + str(age))
for nom, age in sorted(gens, key=lambda g: (-g[1], g[0])): p(nom)
for g in sorted(gens): p(g[0])
p(max(gens)[0])
p(min(gens, key=lambda g: (g[1], g[0]))[0])
p(("Bo", 25) in gens); p(("Bo", 26) in gens)
p(sum(map(lambda g: g[1], gens)))
p((1, 2) == (1, 2)); p((1, 2) != (1, 3)); p((1, 2) < (1, 3)); p((2, "a") > (1, "z"))
p((1, "b") <= (1, "a")); p(((1, 2), "x") == ((1, 2), "x"))
p("pomme" < "poire"); p("Zèbre" < "abricot"); p("été" > "eau")
noms = ["un", "deux", "trois"]
nombres = [1, 2, 3, 4]
for nom, k in zip(noms, nombres): p(nom + "=" + str(k))
p(len(list(zip(nombres, noms))))
compte = {}
for mot in "le chat et le chien et le rat".split(): compte[mot] = compte.get(mot, 0) + 1
for mot, c in compte.items(): p(mot + " " + str(c))
for mot, c in sorted(compte.items(), key=lambda kv: (-kv[1], kv[0])): p(mot)
p(" ".join(compte.keys())); p(sum(compte.values())); p(compte.get("loup", -1))
memo = {}
p(chemins(10, 10, memo)); p(len(memo))
murs = [(1, 0), (1, 1), (1, 2), (3, 1), (3, 2), (3, 3)]
dist = largeur((0, 0), murs, 5)
p(dist[(4, 4)]); p(len(dist))
loin = max(dist.items(), key=lambda kv: (kv[1], kv[0]))
p(str(loin[0][0]) + "," + str(loin[0][1]) + " " + str(loin[1]))
cases = sorted(dist)
p(str(cases[0][0]) + str(cases[0][1]) + " " + str(cases[len(cases) - 1][0]) + str(cases[len(cases) - 1][1]))
vu = {(1, True): "a"}
vu[(1, False)] = "b"
p(vu[(1, True)] + vu[(1, False)]); p(len(vu))
v = Ville("Lyon", (45.75, 4.85), 522000)
p(v.pos[0])
villes = [v, Ville("Nice", (43.7, 7.25), 342000)]
paire = (villes[1], villes)
paire[1].append(Ville("Lille", (50.63, 3.06), 236000))
p(len(villes)); p(paire[0].nom)
ops = [("double", lambda x: 2 * x), ("carre", lambda x: x * x)]
for nom, f in ops: p(nom + " " + str(f(7)))
profond = (1, (2, (3, "fond")))
p(profond[1][1][1])
cle, signe, valeur = "nom = Léa = moi".partition(" = ")
p(cle + "|" + signe + "|" + valeur)
chemin = "/maison/docs/notes.txt".rpartition("/")
p(chemin[0] + " " + chemin[2])
p("abc".partition("x")[0] + "." + "abc".rpartition("x")[2])
meilleur = ("", 0)
for i in range(3000):
    nom = "n" + str(i * 7 % 1000)
    if i * 7 % 1000 > meilleur[1]: meilleur = (nom, i * 7 % 1000)
    junk = "x" * 44 + str(i)
    if len(junk) < 3: p("faux")
p(meilleur[0])
