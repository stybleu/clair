import web

@web.route("/a/<x>", "GET POST")
def f(x: int) -> string {
    return "a"
}

@web.route("/a/<y>", "POST")
def g(y: int) -> string {
    return "b"
}

def main() -> int {
    return 0
}
