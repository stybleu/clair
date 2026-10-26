import pair

def est_impair(n: int) -> bool {
    if n == 0 {
        return false
    }
    return pair.est_pair(n - 1)
}
