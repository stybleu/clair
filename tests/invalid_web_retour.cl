import web

@web.route("/a")
def f() -> int {
    return 1
}

def main() -> int {
    return 0
}
