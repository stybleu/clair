// V0.67: the module forms (import forms), written in Clair and built into the compiler.
// The applications drawn with the designer of Clair Studio, like Visual Basic: a page in the
// browser, its controls (buttons, texts, boxes to type in, lists...) and Clair functions that
// answer when something happens: bouton1_click(f), saisie1_change(f)...
//
// The designer writes the page (Form1.html) and the start of the program (Form1.cl): a class with
// one Control per control, and the web routes. Each time something happens, the page sends the
// state of every control; the program builds the form from it, calls the function written for
// this event, and sends back the new state, which the page shows. So the program keeps nothing
// between two events (Clair has no global variables): everything is in the controls.
//
//     def bouton1_click(f: Form1) -> int {
//         f.etiquette1.text = "Bonjour " + f.saisie1.text
//         return 0
//     }

import web
import os

// A control of the page, or the window itself (f.fenetre).
// kind: "fenetre", "bouton", "etiquette", "saisie", "case", "curseur", "liste", "choix", "image", "minuteur".
Class Control {
    name: string
    kind: string
    text: string
    value: float
    minimum: float
    maximum: float
    step: float
    checked: bool
    items: list[string]
    selected: int
    visible: bool
    enabled: bool
    x: float
    y: float
    width: float
    height: float
    color: string
    background: string
    size: float
    interval: float
    source: string
    message: string

    // the item chosen in a list ("" when none)
    def choice() -> string {
        if self.selected >= 0 and self.selected < len(self.items) {
            return self.items[self.selected]
        }
        return ""
    }

    // adds an item to a list; gives its number
    def add(item: string) -> int {
        self.items.append(item)
        return len(self.items) - 1
    }

    // takes the item i out of a list
    def remove(i: int) -> int {
        let rest: list[string] = []
        for k in range(len(self.items)) {
            if k != i {
                rest.append(self.items[k])
            }
        }
        self.items = rest
        if self.selected >= len(self.items) {
            self.selected = len(self.items) - 1
        }
        return len(self.items)
    }

    // empties a list
    def clear() -> int {
        self.items = []
        self.selected = -1
        return 0
    }

    def show() -> int {
        self.visible = true
        return 0
    }

    def hide() -> int {
        self.visible = false
        return 0
    }

    def move(x: float, y: float) -> int {
        self.x = x
        self.y = y
        return 0
    }

    // true when the text is a number ("12", "-3,5", "1.5e3")
    def is_number() -> bool {
        return is_number(self.text)
    }

    // the text as a number (a comma as well as a point), 0 when it is not one
    def number() -> float {
        return number(self.text)
    }
}

// is s a number? "12", " -3,5 ", "1.5e3" (a comma as well as a point)
def is_number(s: string) -> bool {
    let t: string = decimal(s)
    let n: int = len(t)
    let i: int = 0
    if i < n and (t[i:i + 1] == "-" or t[i:i + 1] == "+") {
        i += 1
    }
    let digits: int = 0
    while i < n and is_digit(t, i) {
        i += 1
        digits += 1
    }
    if i < n and t[i:i + 1] == "." {
        i += 1
        while i < n and is_digit(t, i) {
            i += 1
            digits += 1
        }
    }
    if digits == 0 {
        return false
    }
    if i < n and (t[i:i + 1] == "e" or t[i:i + 1] == "E") {
        i += 1
        if i < n and (t[i:i + 1] == "-" or t[i:i + 1] == "+") {
            i += 1
        }
        let e: int = 0
        while i < n and is_digit(t, i) {
            i += 1
            e += 1
        }
        if e == 0 {
            return false
        }
    }
    return i == n
}

// the character i of t is a digit
def is_digit(t: string, i: int) -> bool {
    let c: int = ord(t[i:i + 1])
    return c >= 48 and c <= 57
}

// s as a number (0 when it is not one)
def number(s: string) -> float {
    if not is_number(s) {
        return 0.0
    }
    return float(decimal(s))
}

// without the spaces around, and a point instead of a comma
def decimal(s: string) -> string {
    let t: string = strip(s)
    let out: string = ""
    for i in range(len(t)) {
        let c: string = t[i:i + 1]
        if c == "," {
            out = out + "."
        } else {
            out = out + c
        }
    }
    return out
}

// a number as the page writes it: 3 rather than 3.0
def text_of(v: float) -> string {
    if v == float(int(v)) and v < 1000000000000000.0 and v > -1000000000000000.0 {
        return str(int(v))
    }
    return str(v)
}

// the control name, as the page sent it
def read(req: Request, name: string) -> Control {
    let p: string = name + "."
    let items: list[string] = []
    let all: string = req.field(p + "items", "")
    if all != "" {
        items = split(all, "\n")
    }
    return Control(name, req.field(p + "kind", ""), req.field(p + "text", ""), number(req.field(p + "value", "0")), number(req.field(p + "minimum", "0")), number(req.field(p + "maximum", "100")), number(req.field(p + "step", "1")), req.field(p + "checked", "0") == "1", items, int(number(req.field(p + "selected", "-1"))), req.field(p + "visible", "1") == "1", req.field(p + "enabled", "1") == "1", number(req.field(p + "x", "0")), number(req.field(p + "y", "0")), number(req.field(p + "width", "0")), number(req.field(p + "height", "0")), req.field(p + "color", ""), req.field(p + "background", ""), number(req.field(p + "size", "16")), number(req.field(p + "interval", "1000")), req.field(p + "source", ""), "")
}

def yes(b: bool) -> string {
    if b {
        return "true"
    }
    return "false"
}

// the control for the page: "name":{...}
def json(c: Control) -> string {
    let items: string = ""
    for i in range(len(c.items)) {
        if i > 0 {
            items = items + ","
        }
        items = items + web.json_string(c.items[i])
    }
    return web.json_string(c.name) + ":{\"name\":" + web.json_string(c.name) + ",\"kind\":" + web.json_string(c.kind) + ",\"text\":" + web.json_string(c.text) + ",\"value\":" + text_of(c.value) + ",\"minimum\":" + text_of(c.minimum) + ",\"maximum\":" + text_of(c.maximum) + ",\"step\":" + text_of(c.step) + ",\"checked\":" + yes(c.checked) + ",\"items\":[" + items + "],\"selected\":" + str(c.selected) + ",\"visible\":" + yes(c.visible) + ",\"enabled\":" + yes(c.enabled) + ",\"x\":" + text_of(c.x) + ",\"y\":" + text_of(c.y) + ",\"width\":" + text_of(c.width) + ",\"height\":" + text_of(c.height) + ",\"color\":" + web.json_string(c.color) + ",\"background\":" + web.json_string(c.background) + ",\"size\":" + text_of(c.size) + ",\"interval\":" + text_of(c.interval) + ",\"source\":" + web.json_string(c.source) + ",\"message\":" + web.json_string(c.message) + "}"
}

// the page drawn by the designer (Form1.html, next to the program)
def page(file: string) -> Response {
    return web.file(file)
}

// serves the application on this phone; in Clair Studio, the console shows a button to open it
// (CLAIR_FORMS_PORT=n: on the port n instead, 0: any free port)
def run(port: int) -> int {
    let p: int = port
    let autre: string = os.env("CLAIR_FORMS_PORT", "")
    if is_number(autre) {
        p = int(number(autre))
    }
    if p > 0 {
        let adresse: string = "http://127.0.0.1:" + str(p) + "/"
        print(chr(27) + "[clair:app " + adresse + "]")
        print("Application : " + adresse)
    }
    return web.run(p)
}
