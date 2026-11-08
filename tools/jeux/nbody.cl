// The Computer Language Benchmarks Game : n-body
// Mouvement du Soleil et des 4 planetes geantes ; energie avant et apres n pas.
// noyau: avance

def avance(x: list[float], y: list[float], z: list[float], vx: list[float], vy: list[float],
           vz: list[float], m: list[float], pas: int, dt: float) -> int {
    let nb: int = len(m)
    for p in range(pas) {
        for i in range(nb) {
            for j in range(i + 1, nb) {
                let dx: float = x[i] - x[j]
                let dy: float = y[i] - y[j]
                let dz: float = z[i] - z[j]
                let d2: float = dx * dx + dy * dy + dz * dz
                let mag: float = dt / (d2 * sqrt(d2))
                let mi: float = m[i] * mag
                let mj: float = m[j] * mag
                vx[i] -= dx * mj
                vy[i] -= dy * mj
                vz[i] -= dz * mj
                vx[j] += dx * mi
                vy[j] += dy * mi
                vz[j] += dz * mi
            }
        }
        for i in range(nb) {
            x[i] += dt * vx[i]
            y[i] += dt * vy[i]
            z[i] += dt * vz[i]
        }
    }
    return 0
}

def energie(x: list[float], y: list[float], z: list[float], vx: list[float], vy: list[float],
            vz: list[float], m: list[float]) -> float {
    let e: float = 0.0
    let nb: int = len(m)
    for i in range(nb) {
        e += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i])
        for j in range(i + 1, nb) {
            let dx: float = x[i] - x[j]
            let dy: float = y[i] - y[j]
            let dz: float = z[i] - z[j]
            e -= m[i] * m[j] / sqrt(dx * dx + dy * dy + dz * dz)
        }
    }
    return e
}

def main() -> int {
    let n: int = input_int()
    let pi: float = 3.141592653589793
    let masse_soleil: float = 4.0 * pi * pi
    let jours: float = 365.24
    let x: list[float] = [0.0, 4.84143144246472090e+00, 8.34336671824457987e+00, 1.28943695621391310e+01, 1.53796971148509165e+01]
    let y: list[float] = [0.0, -1.16032004402742839e+00, 4.12479856412430479e+00, -1.51111514016986312e+01, -2.59193146099879641e+01]
    let z: list[float] = [0.0, -1.03622044471123109e-01, -4.03523417114321381e-01, -2.23307578892655734e-01, 1.79258772950371181e-01]
    let vx: list[float] = [0.0, 1.66007664274403694e-03 * jours, -2.76742510726862411e-03 * jours, 2.96460137564761618e-03 * jours, 2.68067772490389322e-03 * jours]
    let vy: list[float] = [0.0, 7.69901118419740425e-03 * jours, 4.99852801234917238e-03 * jours, 2.37847173959480950e-03 * jours, 1.62824170038242295e-03 * jours]
    let vz: list[float] = [0.0, -6.90460016972063023e-05 * jours, 2.30417297573763929e-05 * jours, -2.96589568540237556e-05 * jours, -9.51592254519715870e-05 * jours]
    let m: list[float] = [masse_soleil, 9.54791938424326609e-04 * masse_soleil, 2.85885980666130812e-04 * masse_soleil, 4.36624404335156298e-05 * masse_soleil, 5.15138902046611451e-05 * masse_soleil]

    // le Soleil compense la quantite de mouvement des planetes
    let px: float = 0.0
    let py: float = 0.0
    let pz: float = 0.0
    for i in range(len(m)) {
        px += vx[i] * m[i]
        py += vy[i] * m[i]
        pz += vz[i] * m[i]
    }
    vx[0] = 0.0 - px / masse_soleil
    vy[0] = 0.0 - py / masse_soleil
    vz[0] = 0.0 - pz / masse_soleil

    print(format(energie(x, y, z, vx, vy, vz, m), ".9f"))
    avance(x, y, z, vx, vy, vz, m, n, 0.01)
    print(format(energie(x, y, z, vx, vy, vz, m), ".9f"))
    return 0
}
