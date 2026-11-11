# La reference de regions_listes.cl : le meme programme en Python.
class Point:
    def __init__(s, x, nom): s.x, s.nom = x, nom
class Boite:
    def __init__(s, items): s.items = items
def fabrique(i): return [Point(float(i), "f" + str(i))]
best = []; noms = []; d = {}; par_cle = {}; boite = Boite([])
regle = lambda x: "rien"
for i in range(3):
    ps = [Point(float(i), "p" + str(i)), Point(1.0, "q" + str(i))]
    best = ps
    ms = ["a" + str(i), "b" + str(i)]
    noms = [ms]
    tmp = {}
    tmp["k" + str(i)] = Point(2.0, "z" + str(i))
    d = tmp
    par_cle[i] = ["v" + str(i)]
    boite.items = fabrique(i)
    suffixe = "#" + str(i)
    regle = lambda x, suffixe=suffixe: str(x) + suffixe
for i in range(2000):
    junk = "x" * 48 + str(i)
    o = Point(9.0, junk)
    if len(o.nom) != len(junk): print("faux")
print(best[0].nom + " " + best[1].nom)
print(noms[0][1])
for k in d: print(k + " " + d[k].nom)
for k in par_cle: print(str(k) + " " + par_cle[k][0])
print(boite.items[0].nom)
print(regle(7))
