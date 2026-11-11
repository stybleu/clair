// V0.74 : les dictionnaires a cles entieres (dict[int, ...]), et ceux qui gardent des objets,
// des listes ou d'autres dictionnaires ; dicts_listes.expected est la sortie de Python.
Class P {
    x: int
}

Class Ville {
    nom: string
    habitants: int
}

def fib(n: int, memo: dict[int, int]) -> int {
    if n < 2 {
        return n
    }
    if n in memo {
        return memo[n]
    }
    let r = fib(n - 1, memo) + fib(n - 2, memo)
    memo[n] = r
    return r
}

def main {
    let carres: dict[int, int] = {}
    for i in range(-3, 5) {
        carres[i] = i * i
    }
    print(len(carres))
    print(carres[-3])
    print(carres[4])
    let total = 0
    for k in carres {
        total += k
    }
    print(total)
    print(2 in carres)
    print(9 in carres)
    let noms = {1: "un", 2: "deux", 10: "dix"}
    print(noms[10])
    let memo: dict[int, int] = {}
    print(fib(80, memo))
    let villes: dict[string, Ville] = {}
    villes["p"] = Ville("Paris", 2100000)
    villes["l"] = Ville("Lyon", 520000)
    print(villes["l"].nom)
    villes["p"].habitants += 1
    print(villes["p"].habitants)
    let groupes: dict[string, list[int]] = {}
    groupes["pairs"] = []
    groupes["impairs"] = []
    for i in range(10) {
        if i % 2 == 0 {
            groupes["pairs"].append(i)
        } else {
            groupes["impairs"].append(i)
        }
    }
    print(len(groupes["pairs"]))
    print(groupes["impairs"][4])
    let index: dict[int, list[string]] = {3: ["abc", "def"], 2: ["ab"]}
    index[3].append("ghi")
    print(len(index[3]))
    let grand = 9223372036854775807
    let extremes: dict[int, string] = {}
    extremes[grand] = "max"
    extremes[-grand - 1] = "min"
    for k in extremes {
        print(k)
    }
    let notes: dict[string, dict[string, int]] = {}
    notes["léa"] = {}
    notes["léa"]["maths"] = 15
    notes["léa"]["maths"] += 2
    print(notes["léa"]["maths"])
    let ld: list[dict[string, int]] = [{"a": 1}, {}]
    ld[1]["b"] = 2
    print(ld[1]["b"] + ld[0]["a"])
    let ps = [P(1), P(2)]
    for p in ps {
        if p.x < 3 {
            ps.append(P(p.x + 10))
        }
    }
    print(len(ps))
    print(ps[3].x)
    let g = [[1], [2]]
    for ligne in g {
        ligne.append(0)
    }
    print(len(g[1]))
}
