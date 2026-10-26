// V0.47 : pair et impair s importent l un l autre
import impair

def est_pair(n: int) -> bool {
    if n == 0 {
        return true
    }
    return impair.est_impair(n - 1)
}
