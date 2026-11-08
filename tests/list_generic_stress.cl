def main() -> int {
    let floats: list[float] = []
    let flags: list[bool] = []
    let words: list[string] = []

    for i in range(0, 100) {
        floats.append(i)
        flags.append(i < 50)
        words.append("a" + "b")
    }

    print(len(floats))
    print(floats[99])
    print(len(flags))
    print(flags[49])
    print(flags[50])
    print(len(words))
    print(words[99])
    return 0
}
