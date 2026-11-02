// V0.66 : le module gui, sans ecran (SDL3 et son pilote offscreen) ; les clics et les touches viennent de
// CLAIR_GUI_EVENTS (tests/run_tests.sh), la fenetre se ferme seule apres CLAIR_GUI_FRAMES images.
import gui

def couleur(c: Color) -> string {
    return str(c.r) + "," + str(c.g) + "," + str(c.b)
}

def main() -> int {
    if not gui.available() {
        print("sans SDL3")
        return 0
    }
    let w: Window = gui.open("Essai", 200, 120)
    print("taille " + str(w.width()) + " x " + str(w.height()))
    let images: int = 0
    let clics: int = 0
    let espaces: int = 0
    let blanc: int = 0
    while w.running() {
        for i in range(w.event_count()) {
            let e: Event = w.event(i)
            if e.kind == "mouse_down" {
                print("appui " + str(int(e.x)) + " " + str(int(e.y)) + " bouton " + str(e.button))
            }
        }
        w.clear(gui.color("noir"))
        w.rect(10.0, 10.0, 30.0, 20.0, gui.color("rouge"))
        w.circle(100.0, 60.0, 20.0, gui.hex("#ffcc00"))
        w.ring(100.0, 60.0, 26.0, gui.rgb(0, 200, 0))
        w.lines([120.0, 140.0, 160.0], [100.0, 90.0, 100.0], gui.color("ciel"))
        w.polygon([150.0, 190.0, 190.0], [40.0, 40.0, 70.0], gui.color("violet"))
        w.text("É", 150.0, 10.0, 16.0, gui.color("blanc"))
        if w.button("OK", 10.0, 80.0, 60.0, 30.0) {
            clics += 1
        }
        if w.key_pressed("espace") {
            espaces += 1
        }
        if images == 3 {
            print("rectangle " + couleur(w.pixel(20, 20)))
            print("disque " + couleur(w.pixel(100, 60)))
            print("anneau " + couleur(w.pixel(126, 60)))
            print("polygone " + couleur(w.pixel(185, 45)))
            print("fond " + couleur(w.pixel(5, 5)))
            print("copie BMP " + str(w.screenshot("gui_test.bmp")) + ", PNG " + str(w.screenshot("gui_test.png")))
            for y in range(10, 26) {
                for x in range(150, 166) {
                    if w.pixel(x, y).r == 255 {
                        blanc += 1
                    }
                }
            }
        }
        images += 1
        w.present()
    }
    print("images " + str(images))
    print("clics sur OK " + str(clics))
    print("espace " + str(espaces))
    print("lettre E accent : " + str(blanc > 40 and blanc < 140))
    print("largeur du texte " + format(w.text_width("été", 16.0), ".0f"))
    let img: Image = gui.image("gui_test.bmp")
    print("image " + str(img.width) + " x " + str(img.height))
    w.clear(gui.color("noir"))
    w.draw(img, 0.0, 0.0)
    print("image dessinee " + couleur(w.pixel(20, 20)))
    w.clear(gui.color("noir"))
    w.draw_rotated(img, 0.0, 0.0, 200.0, 120.0, 180.0)
    print("image tournee " + couleur(w.pixel(180, 100)))
    let png: Image = gui.image("gui_test.png")
    print("image PNG " + str(png.width) + " x " + str(png.height))
    w.close()
    let w2: Window = gui.open("Encore", 64, 48)
    print("une autre fenetre " + str(w2.width()) + " x " + str(w2.height()))
    w2.close()
    return 0
}
