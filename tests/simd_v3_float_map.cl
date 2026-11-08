def map_float(values: list[float]) -> int {
    let i: int = 0

    while i < len(values) {
        values[i] = values[i] + 1.0
        i = i + 1
    }

    return i
}

def main() -> int {
    let values: list[float] = []
    let alias: list[float] = values

    for i in range(0, 1024) {
        alias.append(i)
    }

    print(map_float(values))
    print(values[0])
    print(values[1023])

    return 0
}
