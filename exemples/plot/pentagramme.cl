// V0.61 : un pentagramme : les 5 sommets d'un pentagone, relies un sur deux.
import plot
import math

def main() -> int {
    let xs: list[float] = []
    let ys: list[float] = []
    for k in range(0, 6) {
        let angle: float = math.pi / 2.0 + float(2 * k) * 2.0 * math.pi / 5.0
        xs.append(math.cos(angle))
        ys.append(math.sin(angle))
    }
    plot.title("Pentagramme")
    plot.equal()
    plot.hide_axes()
    plot.circle(0.0, 0.0, 1.0)
    plot.color("gris")
    plot.polygon(xs, ys)
    plot.color("violet")
    plot.fill("jaune")
    plot.width(2.5)
    for k in range(0, 5) {
        plot.text(1.15 * xs[k], 1.15 * ys[k], str(k + 1))
    }
    plot.show()
    return 0
}
