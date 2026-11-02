// V0.59 : un module avec ses propres tests : ceux qui l'importent ne les lancent pas
def double(x: int) -> int {
    return 2 * x
}

test "le test du module ne tourne pas chez ceux qui l'importent" {
    assert_equal(double(2), 5)
}
