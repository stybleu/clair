def abs(x: int) -> int {
    if x < 0 {
        return -x
    }
    return x
}

def exit(code: int) -> int {
    return code + 1
}

def free(n: int) -> int {
    return n * 2
}

def printf(s: string) -> string {
    return s + "!"
}

Class Box {
    static: int
    long: int

    def switch(default: int) -> int {
        return self.static + self.long + default
    }
}

def strlen(char: int, double: int) -> int {
    let NULL: int = char + double
    return NULL
}

def main() -> int {
    let case: int = abs(-5)
    print(case)
    print(exit(1))
    print(free(21))
    print(printf("hi"))

    let b: Box = Box(1, 2)
    print(b.switch(3))
    b.static = 10
    print(b.static)

    let int64_t: int = strlen(4, 5)
    print(int64_t)
    print(len("abc"))
    let size_t: string = "xyz"
    print(len(size_t))
    return 0
}
