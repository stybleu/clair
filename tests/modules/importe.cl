// V0.47 : import, from ... import, as
import math as m
import phys
from astro import GM_mars, GM_terre, an
import orbite
from orbite import periode as T, Astre
import outils
import pair

// une fonction du programme qui s'appelle comme une fonction de math ne la cache pas
def sqrt(x: float) -> float {
    return x * 0.0 - 1.0
}

def main() -> int {
    print(m.sqrt(2.0))
    print(sqrt(2.0))
    print(m.pi)
    print(m.e)
    print(m.tau)
    print(phys.c)
    print(phys.G)
    print(phys.hbar)
    print(GM_mars)
    print(orbite.vitesse_circulaire(GM_mars, 3789500.0))
    print(T(GM_terre, 6778137.0) / 60.0)
    print(orbite.vitesse_terre())
    print(outils.annee_terre())
    print(outils.compte(5))
    print(pair.est_pair(10))
    print(pair.est_pair(7))
    let mars: Astre = orbite.Astre("Mars", GM_mars)
    print(mars.nom)
    print(mars.vitesse(3789500.0))
    let terre: Astre = Astre("Terre", GM_terre)
    print(terre.gm / mars.gm)
    print(an)
    // les fonctions de math (V0.47)
    print(m.degrees(m.pi))
    print(m.radians(180.0))
    print(m.cbrt(27.0))
    print(m.asinh(1.0))
    print(m.acosh(2.0))
    print(m.atanh(0.5))
    print(m.erf(1.0))
    print(m.erfc(1.0))
    print(m.gamma(5.5))
    print(m.lgamma(10.0))
    print(m.expm1(1e-10))
    print(m.log1p(1e-10))
    print(m.exp2(10.0))
    print(m.trunc(-2.7))
    print(m.fmod(7.5, 2.0))
    print(m.copysign(3.0, -0.0))
    print(m.fabs(-4.0))
    print(m.isqrt(17))
    print(m.gcd(12, 18))
    print(m.inf)
    print(m.nan)
    print(m.pi(30))
    print(masque(4))
    return 0
}

// une variable du meme nom qu'un module le cache dans toute sa fonction, comme en Python
def masque(m: int) -> int {
    let phys: int = 3
    return phys * m
}
