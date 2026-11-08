// V0.59 : des tests dans le programme : test "nom" { ... }, assert, assert_equal
// clair test_blocs.cl -o p : main seul.  clair --test test_blocs.cl -o t : les tests, chacun a part.
import test_blocs_outil

Class Point {
    x: float
    y: float
}

def carre(x: int) -> int {
    return x * x
}

def salut(nom: string) -> string {
    return "Bonjour " + nom
}

def main() -> int {
    print(carre(4))
    print(test_blocs_outil.double(5))
    return 0
}

test "entiers" {
    assert_equal(carre(3), 9)
    assert_equal(carre(-3), 9)
    let petit: u8 = 200
    assert_equal(petit, 200)
    let grand: u64 = 18446744073709551615
    assert_equal(grand, grand)
    assert(carre(2) == 4)
    assert(test_blocs_outil.double(4) == 8, "double")
}

test "chaines, booleens et flottants" {
    print("ce qu'ecrit un test reussi n'est pas montre")
    assert_equal(salut("Ada"), "Bonjour Ada")
    assert_equal(carre(2) > 3, true)
    assert_equal(0.5 + 0.25, 0.75)
    let p: Point = Point(1.5, 2.0)
    assert_equal(p.x + p.y, 3.5)
}

test "assert avec un message" {
    print("avant")
    assert(carre(-2) < 0, "un carre est negatif ?")
    print("jamais")
}

test "assert sans message" {
    let x: int = carre(3)
    assert(x ==
        10)
}

test "chaine differente" {
    assert_equal(salut("ada"), "Bonjour Ada\n")
}

test "flottant" {
    assert_equal(0.1 + 0.2, 0.3)
}

test "erreur pendant le test" {
    let xs: list[int] = [1, 2, 3]
    print(xs[5])
}

test "booleen" {
    assert_equal(carre(1) == 1, false)
}

test "boucle sans fin" {
    let i: int = 0
    while true {
        i += 1
        if i < 0 {
            print(i)
        }
    }
}

test "input lit la fin de l'entree au lieu d'attendre" {
    print(input())
}
