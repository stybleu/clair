// V0.64 : un grand dictionnaire (200 000 cles) : chaque cle se trouve d'un coup (table de hachage), dans l'ordre ou elle est venue
def main() -> int {
    let d: dict[string, int] = {}
    for i in range(0, 200000) {
        d["cle" + str(i)] = i
    }
    let s: int = 0
    for i in range(0, 200000) {
        s += d["cle" + str(i)]
    }
    let n: int = 0
    for k in d {
        n += 1
    }
    print(s)
    print(n)
    print("cle199999" in d)
    print("absente" in d)
    let petit: dict[string, string] = {"a": "1", "b": "2"}
    petit["c"] = "3"
    for k in petit {
        print(k + "=" + petit[k])
    }
    return 0
}
