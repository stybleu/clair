// V0.66 : sans SDL3 (CLAIR_SDL3=none), gui.open dit quoi installer
import gui

def main() -> int {
    print(gui.available())
    let w: Window = gui.open("Essai", 100, 100)
    print("jamais")
    return w.width()
}
