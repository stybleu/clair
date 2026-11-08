// V0.61 : 5000 tirages d'une loi normale, et des barres.
import plot

def main() -> int {
    let tirages: list[float] = []
    for i in range(0, 5000) {
        tirages.append(gauss(20.0, 3.0))
    }
    plot.title("Temperatures tirees au hasard")
    plot.xlabel("degres")
    plot.ylabel("nombre")
    plot.histogram(tirages, 40)
    plot.color("orange")
    plot.show()

    plot.title("Pluie de la semaine (mm)")
    plot.bars(["lun", "mar", "mer", "jeu", "ven", "sam", "dim"], [0.0, 2.5, 11.0, 4.5, 0.5, 0.0, 7.0])
    plot.color("bleu")
    plot.show()
    return 0
}
