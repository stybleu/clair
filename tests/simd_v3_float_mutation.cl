def identity(values: list[float]) -> list[float] {
    return values
}

def grow(values: list[float]) -> int {
    for i in range(0, 4096) {
        values.append(i)
    }
    return len(values)
}

def update(values: list[float]) -> int {
    let i: int = 0
    while i < len(values) {
        values[i] = values[i] + 0.5
        i = i + 1
    }
    return i
}

def main() -> int {
    let values: list[float] = []
    let alias: list[float] = identity(values)

    print(grow(alias))
    print(update(values))
    print(alias[0])
    print(values[1024])
    print(alias[4095])

    alias.append(4096)
    values[4096] = values[4096] + 0.5

    print(alias[4096])
    print(len(values))
    return 0
}
