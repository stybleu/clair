// V0.55: the module web (import web), written in Clair and built into the compiler.
// The network, the files, PHP and the templates are done in C (clair_web_rt.c),
// through the __web_... functions, which only this file can call.

// A request: what the browser asked.
Class Request {
    method: string
    path: string
    args: dict[string, string]
    form: dict[string, string]
    headers: dict[string, string]
    cookies: dict[string, string]
    files: dict[string, string]
    session: dict[string, string]
    body: string
    ip: string

    // ?nom=Ada : req.arg("nom", "")
    def arg(name: string, otherwise: string) -> string {
        if name in self.args {
            return self.args[name]
        }
        return otherwise
    }

    // a field of a form sent with POST
    def field(name: string, otherwise: string) -> string {
        if name in self.form {
            return self.form[name]
        }
        return otherwise
    }

    // a header, its name in any case: req.header("User-Agent")
    def header(name: string) -> string {
        let low: string = __web_lower(name)
        if low in self.headers {
            return self.headers[low]
        }
        return ""
    }

    // a value kept in the session (signed: the browser cannot change it)
    def session_value(name: string, otherwise: string) -> string {
        if name in self.session {
            return self.session[name]
        }
        return otherwise
    }

    def cookie(name: string, otherwise: string) -> string {
        if name in self.cookies {
            return self.cookies[name]
        }
        return otherwise
    }
}

// An answer: its status (200, 404...), its type, its body and more headers (one per line).
Class Response {
    status: int
    type: string
    body: string
    headers: string
}

// @web.route("/path/<name>") and @web.route("/path", "GET POST") are read by the compiler (web.c).

// serves on this phone only (http://127.0.0.1:port/)
def run(port: int) -> int {
    return __web_run(port, "127.0.0.1")
}

// serves on the network too (the other devices of the same Wi-Fi)
def run_public(port: int) -> int {
    return __web_run(port, "0.0.0.0")
}

def html(body: string) -> Response {
    return Response(200, "text/html; charset=utf-8", body, "")
}

def text(body: string) -> Response {
    return Response(200, "text/plain; charset=utf-8", body, "")
}

def json(body: string) -> Response {
    return Response(200, "application/json", body, "")
}

def redirect(url: string) -> Response {
    return Response(302, "text/plain; charset=utf-8", "", "Location: " + url)
}

def error(status: int, message: string) -> Response {
    let title: string = str(status)
    let page: string = "<!doctype html>\n<meta charset=\"utf-8\">\n<meta name=\"viewport\" content=\"width=device-width\">\n<title>" + title + "</title>\n<h1>" + title + "</h1>\n<p>" + __web_escape(message) + "</p>\n"
    return Response(status, "text/html; charset=utf-8", page, "")
}

// a file sent as it is (its type comes from its extension)
def file(path: string) -> Response {
    return Response(200, "@file", path, "")
}

def with_status(r: Response, status: int) -> Response {
    return Response(status, r.type, r.body, r.headers)
}

def with_header(r: Response, name: string, value: string) -> Response {
    return Response(r.status, r.type, r.body, r.headers + "\n" + name + ": " + value)
}

def with_cookie(r: Response, name: string, value: string) -> Response {
    return with_header(r, "Set-Cookie", name + "=" + __web_quote(value) + "; Path=/; SameSite=Lax")
}

// V0.57: the session, kept by the browser in a signed cookie (HttpOnly, 31 days): it cannot change it
def with_session(r: Response, values: dict[string, string]) -> Response {
    return with_header(r, "Set-Cookie", __web_session_cookie(values))
}

def end_session(r: Response) -> Response {
    return with_header(r, "Set-Cookie", "clair_session=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax")
}

def forget_cookie(r: Response, name: string) -> Response {
    return with_header(r, "Set-Cookie", name + "=; Path=/; Max-Age=0")
}

// a password, salted and hashed 600000 times (PBKDF2-SHA256, the format of Werkzeug): keep this, never the password
def hash_password(password: string) -> string {
    return __web_hash_password(password)
}

def check_password(hashed: string, password: string) -> bool {
    return __web_check_password(hashed, password)
}

// n random bytes in hexadecimal: a name nobody can guess
def random_token(n: int) -> string {
    return __web_random(n)
}

// templates/name with the values: {{ x }}, {% if x %}, {% for l in x %}, {% extends "base.html" %}...
def page(name: string, values: dict[string, string]) -> string {
    return __web_render(name, values)
}

// < > & " ' written for HTML
def escape(s: string) -> string {
    return __web_escape(s)
}

// written for a URL: "a b" is "a%20b"
def quote(s: string) -> string {
    return __web_quote(s)
}

// written for JSON, with its quotes: "a\"b"
def json_string(s: string) -> string {
    return __web_json(s)
}

// data/name: a text kept between two runs of the program (load, save, add_line, lines)
def load(name: string, otherwise: string) -> string {
    return __web_data(0, name, otherwise)
}

def save(name: string, value: string) -> int {
    __web_data(1, name, value)
    return 0
}

def add_line(name: string, line: string) -> int {
    __web_data(2, name, line)
    return 0
}

def lines(name: string) -> list[string] {
    let out: list[string] = []
    for l in split(__web_data(0, name, ""), "\n") {
        if l != "" {
            out.append(l)
        }
    }
    return out
}

// a file sent by a form (req.files["photo"]) kept as path: 1, or 0 when none was sent
def keep(sent: string, path: string) -> int {
    return __web_keep(sent, path)
}

// what the routes the compiler writes use
def __request() -> Request {
    return Request(__web_text(0), __web_text(1), __web_pairs(0), __web_pairs(1), __web_pairs(2), __web_pairs(3), __web_pairs(4), __web_pairs(5), __web_text(2), __web_text(3))
}

def __parts(path: string) -> list[string] {
    let out: list[string] = []
    for s in split(path, "/") {
        if s != "" {
            out.append(s)
        }
    }
    return out
}

// V0.62: <path:name>: the parts from k on, joined by /
def __rest(parts: list[string], k: int) -> string {
    let out: string = parts[k]
    for i in range(k + 1, len(parts)) {
        out = out + "/" + parts[i]
    }
    return out
}

def __allow(methods: string, more: string) -> string {
    if methods == "" {
        return more
    }
    return methods + ", " + more
}

def __number(s: string, kind: int) -> bool {
    return __web_is_number(s, kind)
}

def __page(body: string) -> int {
    return __web_reply(200, "text/html; charset=utf-8", body, "")
}

def __reply(r: Response) -> int {
    return __web_reply(r.status, r.type, r.body, r.headers)
}

def __static(path: string) -> int {
    return __web_static(path)
}
