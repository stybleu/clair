// V0.72 : return seul ne suffit pas a une fonction qui rend un int
def f() -> int {
    return
}

def main {
    print(f())
}
