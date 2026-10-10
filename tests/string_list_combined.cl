def main() -> int {
    let message: string = "Hello " + "Clair"
    let values: list[int] = [1]
    values.append(2)
    values[0] = 9
    print(message)
    print(values[0])
    print(values[1])
    return 0
}
