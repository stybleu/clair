// V0.63 : le modele de Lorenz (1963), tire d'un modele de convection de l'atmosphere.
// Trois equations, et pourtant deux departs presque identiques finissent tout a fait differents :
// c'est "l'effet papillon", la raison pour laquelle on ne prevoit pas le temps a plus de quelques jours.
//   dx/dt = sigma (y - x)      dy/dt = x (rho - z) - y      dz/dt = x y - beta z
// clair --test lorenz.cl -o t && ./t : verifie le calcul.
import plot
import math

def derivee(e: list[float], sigma: float, rho: float, beta: float) -> list[float] {
    return [sigma * (e[1] - e[0]), e[0] * (rho - e[2]) - e[1], e[0] * e[1] - beta * e[2]]
}

// un pas de temps dt, par la methode de Runge-Kutta d'ordre 4
def pas(e: list[float], dt: float, sigma: float, rho: float, beta: float) -> list[float] {
    let k1: list[float] = derivee(e, sigma, rho, beta)
    let k2: list[float] = derivee([e[0] + dt / 2.0 * k1[0], e[1] + dt / 2.0 * k1[1], e[2] + dt / 2.0 * k1[2]], sigma, rho, beta)
    let k3: list[float] = derivee([e[0] + dt / 2.0 * k2[0], e[1] + dt / 2.0 * k2[1], e[2] + dt / 2.0 * k2[2]], sigma, rho, beta)
    let k4: list[float] = derivee([e[0] + dt * k3[0], e[1] + dt * k3[1], e[2] + dt * k3[2]], sigma, rho, beta)
    let r: list[float] = []
    for i in range(0, 3) {
        r.append(e[i] + dt / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]))
    }
    return r
}

def main() -> int {
    let sigma: float = 10.0
    let rho: float = 28.0
    let beta: float = 8.0 / 3.0
    let dt: float = 0.01
    let n: int = 4000
    // deux departs qui ne different que d'un millionieme
    let a: list[float] = [1.0, 1.0, 1.0]
    let b: list[float] = [1.000001, 1.0, 1.0]
    let ts: list[float] = []
    let ax: list[float] = []
    let az: list[float] = []
    let bx: list[float] = []
    let ecart_a: float = -1.0
    for k in range(0, n) {
        ts.append(float(k) * dt)
        ax.append(a[0])
        az.append(a[2])
        bx.append(b[0])
        if ecart_a < 0.0 and abs(a[0] - b[0]) > 1.0 {
            ecart_a = float(k) * dt
        }
        a = pas(a, dt, sigma, rho, beta)
        b = pas(b, dt, sigma, rho, beta)
    }
    print("Les deux départs diffèrent d'un millionième.")
    print("Ils s'écartent de plus de 1 après t = " + format(ecart_a, ".1f") + " : au-delà, on ne peut plus prévoir.")

    plot.title("L'attracteur de Lorenz (le papillon)")
    plot.xlabel("x")
    plot.ylabel("z")
    plot.line(ax, az)
    plot.width(1.0)
    plot.show()

    plot.title("Deux départs presque identiques")
    plot.xlabel("temps")
    plot.ylabel("x")
    plot.line(ts, ax)
    plot.name("départ x = 1")
    plot.width(1.5)
    plot.line(ts, bx)
    plot.name("départ x = 1.000001")
    plot.width(1.5)
    plot.segment(ecart_a, -22.0, ecart_a, 22.0)
    plot.color("gris")
    plot.show()
    return 0
}

test "les points fixes ne bougent pas" {
    let beta: float = 8.0 / 3.0
    let c: float = math.sqrt(beta * 27.0)
    let d: list[float] = derivee([c, c, 27.0], 10.0, 28.0, beta)
    assert(abs(d[0]) < 1e-9 and abs(d[1]) < 1e-9 and abs(d[2]) < 1e-9, "C+ = (sqrt(beta (rho - 1)), sqrt(beta (rho - 1)), rho - 1)")
    let o: list[float] = derivee([0.0, 0.0, 0.0], 10.0, 28.0, beta)
    assert_equal(o[0] + o[1] + o[2], 0.0)
}

test "Runge-Kutta : deux pas de temps donnent le meme resultat" {
    // jusqu'a t = 1, avec dt = 0.01 et dt = 0.005 : les deux calculs restent a moins de 0.001 l'un de l'autre
    let a: list[float] = [1.0, 1.0, 1.0]
    let b: list[float] = [1.0, 1.0, 1.0]
    for k in range(0, 100) {
        a = pas(a, 0.01, 10.0, 28.0, 8.0 / 3.0)
    }
    for k in range(0, 200) {
        b = pas(b, 0.005, 10.0, 28.0, 8.0 / 3.0)
    }
    assert(abs(a[0] - b[0]) < 0.001 and abs(a[2] - b[2]) < 0.001, "ecart " + str(abs(a[0] - b[0])))
}

test "sans assez de chaleur (rho < 1), l'air se calme" {
    let e: list[float] = [1.0, 1.0, 1.0]
    for k in range(0, 3000) {
        e = pas(e, 0.01, 10.0, 0.5, 8.0 / 3.0)
    }
    assert(abs(e[0]) < 1e-3 and abs(e[2]) < 1e-3)
}
