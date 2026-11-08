// V0.42 : let x = x + 1 dans un bloc interieur lit le x du dessus,
// comme let x = x le faisait deja (avant : valeur au hasard en C)
def double(v: int) -> int {
    return v * 2
}

def main() -> int {
    let x: int = 5
    let f: float = 1.5
    if x > 0 {
        let x: int = x + 1
        print(x)
        let f: float = f * double(x)
        print(f)
        for i in range(2) {
            let x: int = double(x) + i
            print(x)
        }
    }
    print(x)
    print(f)
    return 0
}
