// V0.61 : le module plot. Sans Clair Studio, plot.show() ecrit plot_1.svg, plot_2.svg...
import plot

def main() -> int {
    // 1. une courbe, des points, une legende, un titre a echapper (comparee a tests/plot_module.expected.svg)
    plot.size(480, 300)
    plot.title("<essai & \"test\">")
    plot.xlabel("temps (s)")
    plot.ylabel("hauteur (m)")
    plot.line([0.0, 1.0, 2.0, 3.0, 4.0], [0.0, 1.0, 4.0, 9.0, 16.0])
    plot.name("x au carre")
    plot.points([0.0, 2.0, 4.0], [1.0, 5.0, 3.0])
    plot.name("mesures")
    plot.color("rouge")
    plot.width(3.0)
    plot.show()

    // 2. un dessin sans axes : polygone rempli, cercle, segment, fleche, texte
    plot.equal()
    plot.hide_axes()
    plot.polygon([0.0, 1.0, 0.5], [0.0, 0.0, 0.8])
    plot.fill("jaune")
    plot.circle(0.5, 0.3, 0.6)
    plot.color("#0f766e")
    plot.segment(0.0, 0.0, 1.0, 0.8)
    plot.arrow(0.0, 1.0, 0.5, -0.2)
    plot.text(0.5, -0.4, "triangle")
    plot.save("garde.svg")
    plot.show()

    // 3. barres et histogramme
    plot.bars(["lun", "mar", "mer"], [3.0, 5.5, 2.0])
    plot.name("pluie")
    plot.show()
    let v: list[float] = []
    for i in range(0, 100) {
        v.append(float(i % 10))
    }
    plot.histogram(v, 10)
    plot.show()

    // 4. un champ : carte en couleurs, isolignes, fleches
    let nx: int = 20
    let ny: int = 10
    let p: list[float] = []
    let u: list[float] = []
    let w: list[float] = []
    for j in range(0, ny) {
        for i in range(0, nx) {
            let x: float = float(i - 10)
            let y: float = float(j - 5)
            p.append(x * x + 2.0 * y * y)
            u.append(-y)
            w.append(x)
        }
    }
    plot.extent(-10.0, 10.0, -5.0, 5.0)
    plot.colormap("bleu-rouge")
    plot.heatmap(p, nx, ny)
    plot.contour(p, nx, ny, 5)
    plot.arrows(u, w, nx, ny)
    plot.show()

    // 5. une animation de 5 images
    plot.xrange(0.0, 4.0)
    plot.yrange(0.0, 4.0)
    for t in range(0, 5) {
        plot.points([float(t)], [float(t)])
        plot.frame()
    }
    plot.animate(5.0)
    print("fini")
    return 0
}
