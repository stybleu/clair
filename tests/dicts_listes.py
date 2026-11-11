class Ville:
    def __init__(s, nom, habitants): s.nom, s.habitants = nom, habitants
class P:
    def __init__(s, x): s.x = x
def fib(n, memo):
    if n < 2: return n
    if n in memo: return memo[n]
    r = fib(n-1, memo) + fib(n-2, memo); memo[n] = r; return r
def g(v):
    if isinstance(v, bool): return "1" if v else "0"
    return str(v)
out = []; p = lambda v: out.append(g(v))
carres = {}
for i in range(-3, 5): carres[i] = i*i
p(len(carres)); p(carres[-3]); p(carres[4]); p(sum(carres)); p(2 in carres); p(9 in carres)
noms = {1: "un", 2: "deux", 10: "dix"}; p(noms[10]); p(fib(80, {}))
villes = {"p": Ville("Paris", 2100000), "l": Ville("Lyon", 520000)}; p(villes["l"].nom); villes["p"].habitants += 1; p(villes["p"].habitants)
groupes = {"pairs": [], "impairs": []}
for i in range(10): (groupes["pairs"] if i % 2 == 0 else groupes["impairs"]).append(i)
p(len(groupes["pairs"])); p(groupes["impairs"][4])
index = {3: ["abc", "def"], 2: ["ab"]}; index[3].append("ghi"); p(len(index[3]))
grand = 9223372036854775807; extremes = {grand: "max", -grand - 1: "min"}
for k in extremes: p(k)
notes = {"léa": {}}; notes["léa"]["maths"] = 15; notes["léa"]["maths"] += 2; p(notes["léa"]["maths"])
ld = [{"a": 1}, {}]; ld[1]["b"] = 2; p(ld[1]["b"] + ld[0]["a"])
ps = [P(1), P(2)]
for q in ps:
    if q.x < 3: ps.append(P(q.x + 10))
p(len(ps)); p(ps[3].x)
gg = [[1], [2]]
for ligne in gg: ligne.append(0)
p(len(gg[1]))
print("\n".join(out))
