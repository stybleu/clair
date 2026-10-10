def empty_strings() -> list[string] {
    return []
}

def sum_first(values: list[float]) -> float {
    return values[0] + values[1]
}

def join_first(values: list[string]) -> string {
    return values[0] + values[1]
}

def main() -> int {
    let floats: list[float] = [1, 2.5]
    print(floats[0])
    floats.append(3)
    floats[1] = 4
    print(floats[1])
    print(floats[2])
    print(len(floats))
    print(sum_first([5, 0.5]))

    let flags: list[bool] = [true, false]
    print(flags[0])
    flags.append(true)
    flags[1] = true
    print(flags[1])
    print(len(flags))

    let words: list[string] = ["A", "B"]
    words.append("C" + "D")
    words[0] = "Hello "
    print(words[0] + words[2])
    print(len(words))

    let alias: list[string] = words
    alias[1] = "X"
    print(words[1])

    let empty_flags: list[bool] = []
    empty_flags.append(false)
    print(empty_flags[0])

    let returned: list[string] = empty_strings()
    print(len(returned))
    print(join_first(["Cl", "air"]))

    print([true, false][1])
    print([1.5, 2][0])
    print(["left", "right"][1])

    return 0
}
