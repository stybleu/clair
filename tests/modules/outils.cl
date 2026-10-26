// V0.47 : deux modules qui s'importent l'un l'autre
import orbite

def compte(n: int) -> int {
    if n <= 0 {
        return 0
    }
    return 1 + compte(n - 1)
}

def annee_terre() -> float {
    return orbite.periode(1.32712440041279419e20, 149597870700.0) / 86400.0
}
