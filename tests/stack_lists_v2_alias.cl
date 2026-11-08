def consume(values: list[int]) -> int {
    return values[0]
}

def make_alias_return() -> list[int] {
    let source: list[int] = [70, 80]
    let alias: list[int] = source
    return alias
}

def main() -> int {
    let values: list[int] = [10, 20, 30]
    let alias: list[int] = values

    print(alias[1])

    alias[1] = 25
    print(values[1])

    let alias2: list[int] = alias
    print(alias2[2])

    let grows_source: list[int] = [1, 2]
    let grows_alias: list[int] = grows_source
    grows_alias.append(3)
    print(len(grows_source))

    let passed_source: list[int] = [7, 8]
    let passed_alias: list[int] = passed_source
    print(consume(passed_alias))

    let returned: list[int] = make_alias_return()
    print(returned[1])

    let reassigned_source: list[int] = [4, 5]
    let reassigned_alias: list[int] = reassigned_source
    reassigned_alias = [6, 7]
    print(reassigned_source[0])

    return 0
}
