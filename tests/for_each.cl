Class Bag {
    items: list[int]
    spare: list[int]

    def total() -> int {
        let s: int = 0
        for v in self.items {
            s = s + v
            self.items = self.spare
        }
        return s
    }
}

def make(n: int) -> list[int] {
    let out: list[int] = []
    for i in range(n) {
        out.append(i * i)
    }
    return out
}

def main() -> int {
    let xs: list[int] = [3, 1, 4, 1, 5, 9, 2, 6]
    let s: int = 0
    for v in xs {
        s = s + v
    }
    print(s)

    let ys: list[float] = [0.5, 1.5, 2.0]
    let p: float = 1.0
    for y in ys {
        p = p * y
    }
    print(p)

    let words: list[string] = ["Clair", "est", "rapide"]
    for char in words {
        print(char)
    }

    let flags: list[bool] = [true, false, true]
    let count: int = 0
    for flag in flags {
        if flag {
            count = count + 1
        }
    }
    print(count)

    let seen: int = 0
    for v in xs {
        if v == 1 {
            continue
        }
        if v == 9 {
            break
        }
        seen = seen + v
    }
    print(seen)

    let pairs: int = 0
    for a in [1, 2, 3] {
        for b in [10, 20] {
            pairs = pairs + a * b
        }
    }
    print(pairs)

    let grid: int = 0
    for v in xs {
        for i in range(len(xs)) {
            grid = grid + v * xs[i]
        }
    }
    print(grid)

    let doubled: list[int] = [1, 2, 3, 4]
    let k: int = 0
    for v in doubled {
        if k + 1 < len(doubled) {
            doubled[k + 1] = doubled[k + 1] + v
        }
        k = k + 1
    }
    print(doubled[3])

    let grow: list[int] = [1, 2]
    let steps: int = 0
    for v in grow {
        if v < 20 {
            grow.append(v * 3)
        }
        steps = steps + 1
    }
    print(steps)
    print(len(grow))

    let first: list[int] = [7, 8]
    let other: list[int] = [100]
    let r: int = 0
    for v in first {
        r = r + v
        first = other
    }
    print(r)
    print(first[0])

    let squares: int = 0
    for v in make(5) {
        squares = squares + v
    }
    print(squares)

    let x: list[int] = [4, 5]
    for x in x {
        print(x)
    }
    print(len(x))

    let empty: list[int] = []
    let none: int = 0
    for v in empty {
        none = none + 1
    }
    print(none)

    let copy: list[int] = [1, 2, 3]
    for v in copy {
        v = v * 100
    }
    print(copy[2])

    let bag: Bag = Bag([1, 2, 3], [50])
    print(bag.total())
    print(bag.items[0])
    return 0
}
