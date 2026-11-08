// V0.66 : le module nbody, sans fenetre : orbites, conservation, collisions
import nbody

def oui(b: bool) -> string {
    if b {
        return "oui"
    }
    return "NON"
}

def main() -> int {
    // une planete legere sur une orbite ronde : apres un tour, elle revient ; l'energie et le moment cinetique restent
    let s: Simulation = nbody.simulation(1.0)
    let soleil: int = s.add("Soleil", 1.0, 0.0, 0.0, 0.0, 0.0)
    let p: int = s.orbit("Planete", 0.001, soleil, 1.0, 0.0)
    s.center()
    let e0: float = s.energy()
    let l0: float = s.angular_momentum()
    s.dt = 0.0005
    let tour: float = 2.0 * 3.141592653589793 / sqrt(1.001)
    s.run(tour)
    print("un tour en " + format(s.t, ".4f"))
    print("revenue : " + oui(abs(s.x[p] - s.x[soleil] - 1.0) < 0.001 and abs(s.y[p] - s.y[soleil]) < 0.001))
    print("energie gardee : " + oui(abs((s.energy() - e0) / e0) < 0.000001))
    print("moment cinetique garde : " + oui(abs((s.angular_momentum() - l0) / l0) < 0.000000001))
    print("quantite de mouvement nulle : " + oui(abs(s.momentum(0)) < 0.000000000001 and abs(s.momentum(1)) < 0.000000000001))
    print("centre de masse immobile : " + oui(abs(s.center_of_mass(0)) < 0.000000001 and abs(s.center_of_mass(1)) < 0.000000001))
    print("vitesse : " + format(s.velocity(p), ".3f") + ", distance : " + format(s.distance(p, soleil), ".3f"))

    // une orbite allongee : l'ordre 4 (Yoshida) fait mieux que le saute-mouton pour le meme pas
    let ecarts: list[float] = []
    for ordre in [2, 4] {
        let e: Simulation = nbody.simulation(1.0)
        e.add("A", 1.0, 0.0, 0.0, 0.0, 0.0)
        e.add("B", 0.001, 1.0, 0.0, 0.0, 0.5)
        e.order = ordre
        e.dt = 0.01
        let en0: float = e.kinetic() + e.potential()
        e.run(20.0)
        ecarts.append(abs(e.energy() - en0))
    }
    print("ordre 4 plus precis : " + oui(ecarts[1] < ecarts[0] / 100.0))
    let z: Simulation = nbody.simulation(1.0)
    z.add("Astre", 1.0, 0.0, 0.0, 0.0, 0.0)
    let poussiere: int = z.add("Poussiere", 0.0, 2.0, 0.0, 0.0, 0.0)
    z.dt = 0.01
    z.run(1.0)
    print("un corps sans masse est attire : " + oui(z.vx[poussiere] < -0.2 and z.vx[0] == 0.0))

    // le huit de Chenciner et Montgomery : apres une periode, les trois corps sont revenus
    let h: Simulation = nbody.simulation(1.0)
    h.add("A", 1.0, 0.97000436, -0.24308753, 0.466203685, 0.43236573)
    h.add("B", 1.0, -0.97000436, 0.24308753, 0.466203685, 0.43236573)
    h.add("C", 1.0, 0.0, 0.0, -0.93240737, -0.86473146)
    h.order = 4
    h.dt = 0.001
    h.run(6.32591398)
    print("le huit revient : " + oui(abs(h.x[0] - 0.97000436) < 0.0001 and abs(h.y[0] + 0.24308753) < 0.0001 and abs(h.x[2]) < 0.0001))

    // deux corps qui se touchent n'en font plus qu'un : masse et quantite de mouvement gardees
    let c: Simulation = nbody.simulation(1.0)
    let g: int = c.add("Gros", 3.0, -1.0, 0.0, 1.0, 0.0)
    let q: int = c.add("Petit", 1.0, 1.0, 0.0, -1.0, 0.5)
    c.radius(g, 0.2)
    c.radius(q, 0.1)
    c.merge = true
    let px0: float = c.momentum(0)
    let py0: float = c.momentum(1)
    c.dt = 0.001
    let chocs: int = c.run(2.0)
    print("chocs : " + str(chocs) + ", corps restants : " + str(c.count()))
    print("le gros garde tout : " + format(c.m[g], ".1f") + " et le petit " + format(c.m[q], ".1f") + ", rayon " + format(c.r[q], ".0f"))
    print("quantite de mouvement gardee : " + oui(abs(c.momentum(0) - px0) < 0.000000001 and abs(c.momentum(1) - py0) < 0.000000001))
    print("rayon (volume garde) : " + format(c.r[g], ".4f"))

    // le pas choisi tout seul, les noms, la constante
    let a: Simulation = nbody.simulation(1.0)
    a.add("X", 1.0, 0.0, 0.0, 0.0, 0.0)
    a.add("Y", 1.0, 2.0, 0.0, 0.0, 0.0)
    a.step()
    print("pas choisi : " + format(a.dt * 500.0, ".4f") + " = sqrt(2^3 / 2)")
    print("Y est le numero " + str(a.find("Y")) + ", Z : " + str(a.find("Z")))
    print("G = " + format(nbody.G(), ".4e"))
    return 0
}
