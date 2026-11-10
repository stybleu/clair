#!/usr/bin/env python3
"""V0.72 : les tables Unicode des methodes de texte de Clair (lower, upper, title, isalpha...).

    python3 tools/clair_text_tables.py > /tmp/tables.c

Ecrit le morceau de src/clair_text_rt.c compris entre les lignes
    /* --- tables : tools/clair_text_tables.py (Unicode X.Y) --- */
    /* --- fin des tables --- */
Les tables viennent de Python lui-meme : Clair fait comme le Python qui les a faites.
Avant d'ecrire, le script relit les tables comme le fera le C et verifie, pour chacun des
1 114 112 caracteres, qu'elles redonnent exactement str.lower, str.upper, str.title,
isupper, islower, istitle, isalpha, isdecimal, isdigit, isnumeric et isspace de Python.

Format : chaque table est une suite de nombres, ecrits en base 32 avec 64 lettres sures
dans un texte C (A-Z a-z 0-9 + -) ; une lettre porte 5 bits, et 32 de plus quand le
nombre continue (les 5 bits suivants dans la lettre d'apres).
"""
import sys
import unicodedata

N = 0x110000
ALPHA = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-"


def enc(v):
    assert v >= 0
    out = ""
    while True:
        d = v & 31
        v >>= 5
        if v:
            out += ALPHA[d | 32]
        else:
            out += ALPHA[d]
            return out


def zig(d):
    return 2 * d if d >= 0 else -2 * d - 1


def ranges(pred):
    r, start = [], None
    for c in range(N):
        if pred(c):
            if start is None:
                start = c
        elif start is not None:
            r.append((start, c - 1))
            start = None
    if start is not None:
        r.append((start, N - 1))
    return r


def map_runs(pairs):
    """(c, m) tries par c -> suites (debut, fin, pas, ecart) : m = c + ecart pour debut, debut + pas, ... fin"""
    out = []
    for c, m in pairs:
        d = m - c
        if out:
            s, e, st, dd = out[-1]
            if dd == d and st == 0 and c - e in (1, 2):
                out[-1] = (s, c, c - e, d)
                continue
            if dd == d and st and c - e == st:
                out[-1] = (s, c, st, d)
                continue
        out.append((c, c, 0, d))
    return [(s, e, st or 1, d) for s, e, st, d in out]


def encode_ranges(rs):
    text, prev = enc(len(rs)), 0
    for s, e in rs:
        text += enc(s - prev) + enc(e - s)
        prev = e
    return text


def encode_runs(runs):
    text, prev = enc(len(runs)), 0
    for s, e, st, d in runs:
        text += enc(s - prev) + enc(e - s) + enc(st) + enc(zig(d))
        prev = e
    return text


def encode_special(sp):
    text, prev = enc(len(sp)), 0
    for c, cps in sp:
        text += enc(c - prev) + enc(len(cps)) + "".join(enc(x) for x in cps)
        prev = c
    return text


def main():
    ch = [chr(c) for c in range(N)]
    low = [s.lower() for s in ch]
    up = [s.upper() for s in ch]
    tit = [s.title() for s in ch]

    lower_single = [(c, ord(low[c])) for c in range(N) if len(low[c]) == 1 and low[c] != ch[c]]
    lower_special = [(c, [ord(x) for x in low[c]]) for c in range(N) if len(low[c]) > 1]
    upper_single = [(c, ord(up[c])) for c in range(N) if len(up[c]) == 1 and up[c] != ch[c]]
    upper_special = [(c, [ord(x) for x in up[c]]) for c in range(N) if len(up[c]) > 1]
    title_single = [(c, ord(tit[c])) for c in range(N) if len(tit[c]) == 1 and tit[c] != up[c]]
    title_special = [(c, [ord(x) for x in tit[c]]) for c in range(N) if len(tit[c]) > 1 and tit[c] != up[c]]

    is_lower = [s.islower() for s in ch]
    is_upper = [s.isupper() for s in ch]
    is_title = [s.istitle() and not s.isupper() for s in ch]
    has_upper = [up[c] != ch[c] for c in range(N)]
    has_lower = [low[c] != ch[c] for c in range(N)]
    lower_extra = ranges(lambda c: is_lower[c] and not has_upper[c])
    upper_extra = ranges(lambda c: is_upper[c] and not has_lower[c])
    title_chars = ranges(lambda c: is_title[c])
    cased = [is_lower[c] or is_upper[c] or is_title[c] for c in range(N)]

    # Case_Ignorable, que Python ne donne pas : on le lit dans son lower() (le sigma final)
    def ignorable(c):
        if c == 0x3A3:
            return False
        if cased[c]:
            return ("1" + ch[c] + "Σ").lower()[-1] == "σ"
        return ("AΣ" + ch[c] + "B").lower()[1] == "σ"

    ign = ranges(ignorable)

    props = {
        "alpha": ranges(lambda c: ch[c].isalpha()),
        "decimal": ranges(lambda c: ch[c].isdecimal()),
        "digit": ranges(lambda c: ch[c].isdigit()),
        "numeric": ranges(lambda c: ch[c].isnumeric()),
    }
    # isspace, strip() et split() : la liste ecrite dans clair_tx_space (src/clair_text_rt.c)
    space = [(9, 13), (0x1C, 0x20), (0x85, 0x85), (0xA0, 0xA0), (0x1680, 0x1680), (0x2000, 0x200A),
             (0x2028, 0x2029), (0x202F, 0x202F), (0x205F, 0x205F), (0x3000, 0x3000)]
    assert ranges(lambda c: ch[c].isspace()) == space, "clair_tx_space n'est plus la liste de Python"

    # relire les tables comme le C, et comparer a Python
    def run_map(runs):
        table = {}
        for s, e, st, d in runs:
            for c in range(s, e + 1, st):
                table[c] = c + d
        return table

    lmap, umap, tmap = run_map(map_runs(lower_single)), run_map(map_runs(upper_single)), run_map(map_runs(title_single))
    lsp, usp, tsp = dict(lower_special), dict(upper_special), dict(title_special)

    def full(c, which):
        if which == 0:
            return lsp.get(c) or [lmap.get(c, c)]
        if which == 1:
            return usp.get(c) or [umap.get(c, c)]
        return tsp.get(c) or ([tmap[c]] if c in tmap else full(c, 1))

    def in_ranges(rs):
        s = set()
        for a, b in rs:
            s.update(range(a, b + 1))
        return s

    lx, ux, tc = in_ranges(lower_extra), in_ranges(upper_extra), in_ranges(title_chars)
    sets = {k: in_ranges(v) for k, v in props.items()}
    for c in range(N):
        assert "".join(map(chr, full(c, 0))) == low[c], hex(c)
        assert "".join(map(chr, full(c, 1))) == up[c], hex(c)
        assert "".join(map(chr, full(c, 2))) == tit[c], hex(c)
        hu, hl = full(c, 1) != [c], full(c, 0) != [c]
        assert ((hu and c not in tc) or c in lx) == is_lower[c], hex(c)
        assert ((hl and c not in tc) or c in ux) == is_upper[c], hex(c)
        assert (c in tc) == is_title[c], hex(c)
        assert (c in sets["alpha"]) == ch[c].isalpha()
        assert (c in sets["decimal"]) == ch[c].isdecimal()
        assert (c in sets["digit"]) == ch[c].isdigit()
        assert (c in sets["numeric"]) == ch[c].isnumeric()
        if ch[c].isdecimal():
            assert ch[c].isdigit() and ch[c].isnumeric()
        if ch[c].isdigit():
            assert ch[c].isnumeric()

    tables = [
        ("clair_tx_t_lower", encode_runs(map_runs(lower_single)), "lower(c) = c + ecart, par suites"),
        ("clair_tx_t_upper", encode_runs(map_runs(upper_single)), "upper(c)"),
        ("clair_tx_t_title", encode_runs(map_runs(title_single)), "title(c), quand il n'est pas upper(c)"),
        ("clair_tx_t_lower_sp", encode_special(lower_special), "lower(c) en plusieurs caracteres"),
        ("clair_tx_t_upper_sp", encode_special(upper_special), "upper(c) en plusieurs caracteres (\\u00df : SS)"),
        ("clair_tx_t_title_sp", encode_special(title_special), "title(c) en plusieurs caracteres"),
        ("clair_tx_t_lower_extra", encode_ranges(lower_extra), "minuscules sans majuscule"),
        ("clair_tx_t_upper_extra", encode_ranges(upper_extra), "majuscules sans minuscule"),
        ("clair_tx_t_titlecase", encode_ranges(title_chars), "les lettres de titre (Lt)"),
        ("clair_tx_t_ignorable", encode_ranges(ign), "Case_Ignorable (le sigma final)"),
        ("clair_tx_t_alpha", encode_ranges(props["alpha"]), "isalpha"),
        ("clair_tx_t_decimal", encode_ranges(props["decimal"]), "isdecimal"),
        ("clair_tx_t_digit", encode_ranges(props["digit"]), "isdigit"),
        ("clair_tx_t_numeric", encode_ranges(props["numeric"]), "isnumeric"),
    ]
    out = sys.stdout
    out.write("/* --- tables : tools/clair_text_tables.py (Unicode %s, Python %d.%d) --- */\n"
              % (unicodedata.unidata_version, sys.version_info[0], sys.version_info[1]))
    for name, text, what in tables:
        out.write("/* %s */\nstatic const char %s[] =\n" % (what, name))
        for i in range(0, len(text), 96):
            out.write('    "%s"%s\n' % (text[i:i + 96], ";" if i + 96 >= len(text) else ""))
        if not text:
            out.write('    "";\n')
    out.write("/* --- fin des tables --- */\n")


if __name__ == "__main__":
    main()
