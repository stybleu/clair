// V0.70 : nbody avec l'arbre de Barnes et Hut (s.gravity = "tree") sur tous les coeurs, et les spheres de
// Plummer. Les references : le calcul direct (toutes les paires) et la theorie de la sphere de Plummer.
import nbody

def oui(b: bool) -> string {
    if b {
        return "oui"
    }
    return "NON"
}

def amas(n: int, graine: int, arbre: bool, coeurs: int) -> Simulation {
    let s: Simulation = nbody.simulation(1.0)
    s.add_plummer(n, 1.0, 1.0, graine, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, "")
    s.soft = 0.02
    s.dt = 0.005
    s.threads = coeurs
    if arbre {
        s.gravity = "tree"
    }
    return s
}

def main() -> int {
    // la sphere de Plummer : en equilibre (2K / |W| = 1), la moitie de la masse a 1.305 rayon de Plummer
    let p: Simulation = amas(4000, 3, false, 0)
    print("etoiles : " + str(len(p.m)))
    print("equilibre (viriel a 5 % pres) : " + oui(abs(p.virial() - 1.0) < 0.05))
    print("rayon de demi-masse a 6 % pres de 1.305 : " + oui(abs(p.lagrange_radius(0.5) - 1.305) < 0.08))
    print("centre de masse au centre, immobile : " + oui(abs(p.center_of_mass(0)) < 1e-12 and abs(p.momentum(1)) < 1e-12))
    let q: Simulation = amas(4000, 3, false, 0)
    print("meme graine, meme amas : " + oui(q.x[17] == p.x[17] and q.vz[3999] == p.vz[3999]))

    // l'arbre contre le calcul direct, sur le meme amas
    let a: Simulation = amas(1500, 11, true, 1)
    let d: Simulation = amas(1500, 11, false, 1)
    let e0: float = d.energy()
    a.run(0.5)
    d.run(0.5)
    let ecart: float = 0.0
    for i in range(len(a.m)) {
        ecart = ecart + sqrt((a.x[i] - d.x[i]) * (a.x[i] - d.x[i]) + (a.y[i] - d.y[i]) * (a.y[i] - d.y[i]) + (a.z[i] - d.z[i]) * (a.z[i] - d.z[i]))
    }
    print("arbre comme le calcul direct (ecart moyen < 1e-4) : " + oui(ecart / 1500.0 < 1e-4))
    print("energie gardee avec l'arbre (1e-4) : " + oui(abs((a.energy() - e0) / e0) < 1e-4))

    // un coeur ou tous : les memes nombres, au dernier chiffre
    let b: Simulation = amas(1500, 11, true, 0)
    b.run(0.5)
    let pareil: bool = true
    for i in range(len(a.m)) {
        if a.x[i] != b.x[i] or a.vy[i] != b.vy[i] {
            pareil = false
        }
    }
    print("un coeur ou tous, memes nombres : " + oui(pareil))

    // Yoshida avec l'arbre
    let y: Simulation = amas(1000, 5, true, 0)
    y.integrator = "yoshida"
    let ey: float = y.energy()
    y.run(0.5)
    print("Yoshida avec l'arbre : " + oui(abs((y.energy() - ey) / ey) < 1e-4))

    // collisions : l'arbre trouve les memes chocs que toutes les paires (un pas tres court : les etoiles qui se touchent deja)
    let chocs: list[int] = []
    let mvt: list[float] = []
    for arbre in [false, true] {
        let c: Simulation = amas(800, 9, arbre, 0)
        c.merge = true
        for i in range(len(c.m)) {
            c.radius(i, 0.05)
        }
        c.dt = 1e-9
        chocs.append(c.run(1e-9))
        c.dt = 0.005
        let p0: float = c.momentum(0)
        c.run(1.0)
        mvt.append(abs(c.momentum(0) - p0))
    }
    print("collisions : l'arbre trouve les memes : " + oui(chocs[0] > 10 and chocs[0] == chocs[1]))
    // les forces de l'arbre ne sont pas exactement opposees deux a deux : la quantite de mouvement bouge un peu
    print("quantite de mouvement : directe a 1e-12, arbre a 1e-5 : " + oui(mvt[0] < 1e-12 and mvt[1] < 1e-5))

    // deux amas qui se rencontrent, sauves et relus avec leurs reglages
    let g: Simulation = nbody.simulation(1.0)
    g.add_plummer(500, 1.0, 0.5, 1, -2.0, 0.0, 0.0, 0.3, 0.0, 0.0, "bleu")
    g.add_plummer(500, 1.0, 0.5, 2, 2.0, 0.0, 0.0, -0.3, 0.0, 0.0, "rouge")
    g.gravity = "tree"
    g.theta = 0.6
    g.soft = 0.02
    print("deux amas : " + str(len(g.m)) + " etoiles, centre de masse au milieu : " + oui(abs(g.center_of_mass(0)) < 1e-9) + ", couleurs " + oui(g.colors[0] != g.colors[999]))
    g.save("build/nbody_amas.txt")
    let h: Simulation = nbody.load("build/nbody_amas.txt")
    print("relu : " + oui(h.gravity == "tree" and h.theta == 0.6 and h.x[999] == g.x[999]))
    print("pas choisi seul : " + oui(g.time_step() > 0.0001 and g.time_step() < 0.1))
    return 0
}
