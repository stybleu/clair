import web

@web.route("/a/<path:x>/b")
def f(x: string) -> string {
    return x
}

def main() -> int {
    return 0
}
