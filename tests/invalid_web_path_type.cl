import web

@web.route("/a/<path:x>")
def f(x: int) -> string {
    return "a"
}

def main() -> int {
    return 0
}
