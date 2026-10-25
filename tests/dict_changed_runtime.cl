def main() -> int {
    let d: dict[string,int] = {"a": 1}
    for k in d {
        print(k)
        d["b"] = 2
    }
    return 0
}
