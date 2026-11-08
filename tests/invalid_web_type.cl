import web

@web.route("/a/<x>")
def f(x: list[int]) -> string {
    return "a"
}

def main() -> int {
    return 0
}
