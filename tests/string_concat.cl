def join(a: string, b: string) -> string {
    return a + b
}

def main() -> int {
    let hello: string = "Hello"
    let space: string = " "
    let world: string = "Clair"

    let message: string = hello + space + world
    print(message)
    print(len(message))

    let from_function: string = join("A", "B")
    print(from_function)

    if ("foo" + "bar") == "foobar" {
        print(1)
    }

    let nested: string = join("x", join("y", "z"))
    print(nested)

    let repeated: string = ""
    for i in range(0, 40) {
        repeated = repeated + "x"
    }
    print(len(repeated))

    return 0
}
