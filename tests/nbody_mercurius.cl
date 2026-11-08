// V0.71 : MERCURIUS, l'integrateur hybride de REBOUND : WHFast tant que les planetes sont loin les unes
// des autres, IAS15 pendant leurs rencontres proches. Les valeurs de reference viennent de REBOUND 5.2.2
// (MERCURIUS, memes conditions de depart) et d'IAS15.
import nbody

def oui(b: bool) -> string {
    if b {
        return "oui"
    }
    return "NON"
}

// une comete qui passe a 0.027 UA de Jupiter (unites : UA, an, masse du Soleil)
def comete(integrateur: string, dt: float) -> Simulation {
    let s: Simulation = nbody.simulation_solar()
    s.integrator = integrateur
    s.add("Soleil", 1.0, 0.0, 0.0, 0.0, 0.0)
    s.add_orbit("Jupiter", 0.000954791915, 0, 5.2, 0.0, 0.0, 0.0, 0.0, 175.0)
    s.add_orbit("comete", 0.0, 0, 3.5, 0.5, 0.0, 0.0, 0.0, 180.0)
    if dt > 0.0 {
        s.dt = dt
    }
    return s
}

def ecart(a: Simulation, b: Simulation, i: int) -> float {
    let dx: float = a.x[i] - b.x[i]
    let dy: float = a.y[i] - b.y[i]
    return sqrt(dx * dx + dy * dy)
}

// trois planetes a 0.1 UA l'une de l'autre (moins d'un rayon de Hill) : des rencontres a chaque tour
def serrees(k: int) -> Simulation {
    let s: Simulation = nbody.simulation(1.0)
    s.integrator = "mercurius"
    s.add("etoile", 1.0, 0.0, 0.0, 0.0, 0.0)
    for i in range(3) {
        let a: float = 1.0 + 0.1 * float(i) + 0.01 * float(k)
        s.add("p", 0.002, a, 0.0, 0.0, sqrt(1.0 / a))
    }
    s.center()
    s.dt = 0.0012 * 2.0 * 3.141592653589793
    return s
}

def main() -> int {
    // le systeme solaire, mille ans : aucune rencontre, MERCURIUS fait comme WHFast (autour du Soleil)
    let ss: Simulation = nbody.solar_system()
    ss.integrator = "mercurius"
    ss.dt = 0.005
    let e0: float = ss.energy()
    ss.run(1000.0)
    let de: float = abs((ss.energy() - e0) / e0)
    print("systeme solaire, mille ans : aucune rencontre " + oui(ss.steps_done(3) == 0) + ", energie comme REBOUND (6.15e-10) " + oui(abs(de - 6.15e-10) < 3e-12))
    print("systeme solaire, places comme REBOUND a 1e-7 UA (Mercure, Neptune) : " + oui(abs(ss.x[1] + 0.121766222484721) < 1e-7 and abs(ss.y[1] + 0.450856769894247) < 1e-7 and abs(ss.x[8] - 26.3524046927049) < 1e-7 and abs(ss.y[8] + 14.3910877240593) < 1e-7))

    // la comete frole Jupiter : MERCURIUS suit la rencontre comme IAS15 ; WHFast se trompe
    let ref: Simulation = comete("ias15", 0.0)
    ref.run(3.0)
    let m: Simulation = comete("mercurius", 0.02)
    m.run(3.0)
    let rencontre: bool = m.steps_done(3) > 0 and m.steps_done(4) > m.steps_done(3)   // juste apres s.run()
    let m2: Simulation = comete("mercurius", 0.01)
    m2.run(3.0)
    let w: Simulation = comete("whfast", 0.02)
    w.run(3.0)
    print("comete : deviee par Jupiter, de a = 3.5 a a = " + format(ref.semi_major_axis(2, 0), ".2f") + " UA")
    print("comete, MERCURIUS comme REBOUND a 1e-10 UA : " + oui(abs(m.x[2] + 2.57316512202445) < 1e-10 and abs(m.y[2] + 5.27062897408258) < 1e-10))
    print("comete, MERCURIUS a moins de 2e-5 UA d'IAS15 : " + oui(ecart(m, ref, 2) < 2e-5) + " ; WHFast a plus de 0.1 UA : " + oui(ecart(w, ref, 2) > 0.1))
    let q: float = ecart(m, ref, 2) / ecart(m2, ref, 2)
    print("comete, ordre 2 (pas deux fois plus court, ecart quatre fois plus petit) : " + oui(q > 3.5 and q < 4.5))
    print("comete, la rencontre faite avec IAS15 : " + oui(rencontre))

    // trois planetes serrees : comme REBOUND pendant les 100 premiers pas (ensuite le chaos ecarte deux
    // calculs qui different au dernier chiffre, REBOUND contre lui-meme aussi)
    let p: Simulation = serrees(0)
    let ep: float = p.energy()
    p.run(100.0 * p.dt)
    print("trois planetes serrees, 100 pas : comme REBOUND a 1e-10 " + oui(abs(p.x[1] - 0.8046539919226113) < 1e-10 and abs(p.y[3] - 0.6534071306856961) < 1e-10) + ", rencontres " + oui(p.steps_done(3) > 0) + ", energie a 1e-6 " + oui(abs((p.energy() - ep) / ep) < 1e-6))

    // sauver et reprendre : la suite est la meme au dernier chiffre (les distances critiques sont sauvees)
    p.save("mercurius_etat.txt")
    let r: Simulation = nbody.load("mercurius_etat.txt")
    let autre: Simulation = serrees(1)
    autre.run(50.0 * autre.dt)
    p.run(200.0 * p.dt)
    r.run(200.0 * r.dt)
    print("sauve et relu : " + r.integrator + ", suite identique " + oui(p.x[1] == r.x[1] and p.vy[3] == r.vy[3]))

    // deux simulations menees ensemble : chacune garde ses distances critiques
    let a: Simulation = serrees(0)
    let b: Simulation = serrees(1)
    let c: Simulation = serrees(0)
    for i in range(100) {
        a.run(a.dt)
        b.run(b.dt)
    }
    for i in range(100) {
        c.run(c.dt)
    }
    print("deux simulations ensemble, comme seules : " + oui(a.x[1] == c.x[1] and a.vy[2] == c.vy[2]))

    // collisions : deux planetes qui se touchent n'en font qu'une ; une comete tombe dans l'etoile
    let k: Simulation = nbody.simulation(1.0)
    k.integrator = "mercurius"
    k.merge = true
    let e: int = k.add("etoile", 1.0, 0.0, 0.0, 0.0, 0.0)
    let u: int = k.add("u", 0.001, 1.0, 0.0, 0.0, 1.0)
    let v: int = k.add("v", 0.002, 1.0, 0.05, 0.0, 0.8)
    k.radius(e, 0.005)
    k.radius(u, 0.01)
    k.radius(v, 0.01)
    k.center()
    k.dt = 0.01
    let px: float = k.momentum(0)
    let py: float = k.momentum(1)
    let chocs: int = k.run(1.0)
    print("deux planetes : " + str(chocs) + " collision, " + str(k.count()) + " corps, masse " + format(k.m[v], ".3f") + ", quantite de mouvement gardee " + oui(abs(k.momentum(0) - px) < 1e-15 and abs(k.momentum(1) - py) < 1e-15))
    let t: Simulation = nbody.simulation(1.0)
    t.integrator = "mercurius"
    t.merge = true
    let et: int = t.add("etoile", 1.0, 0.0, 0.0, 0.0, 0.0)
    t.radius(et, 0.005)
    t.add("planete", 0.001, 2.0, 0.0, 0.0, sqrt(0.5))
    let cm: int = t.add("comete", 0.0001, 1.0, 0.0, 0.0, 0.002)
    t.radius(cm, 0.0001)
    t.center()
    t.dt = 0.01
    let p0: float = t.momentum(1)
    let chute: int = t.run(2.0)
    print("comete dans l'etoile : " + str(chute) + " collision, " + str(t.count()) + " corps, masse de l'etoile " + format(t.m[0], ".4f") + ", quantite de mouvement gardee " + oui(abs(t.momentum(1) - p0) < 1e-15))
    return 0
}
