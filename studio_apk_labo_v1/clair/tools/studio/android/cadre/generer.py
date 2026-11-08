#!/usr/bin/env python3
"""Clair Studio, l'appli Android (V0.71.8) : ecrit cadre/res/values/cadre.xml, le petit morceau d'Android
dont aapt2 a besoin pour lier l'appli.

aapt2 lie les ressources de l'appli a celles d'Android (-I) : il y trouve le numero de chaque attribut
android:... et de chaque style @android:style/... . Le telephone a bien ces ressources
(/system/framework/framework-res.apk), mais l'aapt2 de Termux ne sait pas les lire sur Android 16
("failed to load include path"). fabriquer.sh lie donc l'appli a un petit cadre, construit sur le
telephone a partir de cadre.xml : seulement les attributs et les styles dont l'appli se sert, avec leurs
vrais numeros publics (ils ne changent jamais d'une version d'Android a l'autre).

A relancer quand un fichier XML de l'appli se sert d'un nouvel attribut android:..., sur une machine qui a
aapt2 et un android.jar complet (avec resources.arsc), par exemple celui du SDK d'Android :
    python3 generer.py aapt2 android.jar
"""
import re
import subprocess
import sys
from pathlib import Path

ICI = Path(__file__).resolve().parent
APPLI = ICI.parent


def besoins():
    """Les attributs android:... et les styles @android:style/... des fichiers XML de l'appli."""
    attrs, styles = set(), set()
    fichiers = [APPLI / "AndroidManifest.xml"] + sorted((APPLI / "res").rglob("*.xml"))
    for f in fichiers:
        t = f.read_text(encoding="utf-8")
        t = re.sub(r"<!--.*?-->", "", t, flags=re.S)
        attrs.update(a for a in re.findall(r"\bandroid:([A-Za-z]+)\s*=", t))
        attrs.update(re.findall(r'<item\s+name="android:([A-Za-z]+)"', t))
        styles.update(re.findall(r"@android:style/([A-Za-z0-9_.]+)", t))
    return sorted(attrs), sorted(styles)


def lire(dump):
    """Les ressources publiques d'android, lues dans 'aapt2 dump resources'."""
    res, cle = {}, None
    for ligne in dump.splitlines():
        m = re.match(r"\s+resource (0x01[0-9a-f]{6}) (attr|style)/(\S+) PUBLIC$", ligne)
        if m:
            cle = (m.group(2), m.group(3))
            res[cle] = {"id": m.group(1), "type": None, "valeurs": []}
            continue
        if re.match(r"\s+resource ", ligne) or ligne.startswith("Package") or re.match(r"  type ", ligne):
            cle = None
            continue
        if cle is None:
            continue
        m = re.match(r"\s+\(\) \(attr\) type=(\S+)", ligne)
        if m and res[cle]["type"] is None:
            res[cle]["type"] = m.group(1)
            continue
        m = re.match(r"\s+([A-Za-z0-9_]+)\(0x[0-9a-f]+\)=(0x[0-9a-f]+)$", ligne)
        if m and cle[0] == "attr":
            res[cle]["valeurs"].append((m.group(1), m.group(2)))
    return res


def main():
    aapt2, jar = sys.argv[1], sys.argv[2]
    dump = subprocess.run([aapt2, "dump", "resources", jar], capture_output=True, text=True, check=True).stdout
    res = lire(dump)
    attrs, styles = besoins()
    out = ['<?xml version="1.0" encoding="utf-8"?>',
           "<!-- Ecrit par generer.py (ne pas modifier a la main) : les attributs et les styles d'Android dont",
           "     l'appli se sert, avec leurs numeros publics. fabriquer.sh en fait le cadre auquel aapt2 lie l'appli. -->",
           "<resources>"]
    publics = []
    for nom in attrs:
        r = res.get(("attr", nom))
        if r is None:
            sys.exit(f"android:{nom} n'est pas un attribut public d'Android")
        formats = [f for f in r["type"].split("|") if f not in ("enum", "flags")]
        sorte = "enum" if "enum" in r["type"].split("|") else "flag" if "flags" in r["type"].split("|") else None
        fmt = f' format="{"|".join(formats)}"' if formats else ""
        if sorte and r["valeurs"]:
            out.append(f'    <attr name="{nom}"{fmt}>')
            # comme Android : les enum en decimal, les flag en hexadecimal (le type de la valeur ecrite en depend)
            for v, n in sorted(r["valeurs"], key=lambda x: (int(x[1], 16), x[0])):
                n = str(int(n, 16)) if sorte == "enum" else n
                out.append(f'        <{sorte} name="{v}" value="{n}" />')
            out.append("    </attr>")
        else:
            out.append(f'    <attr name="{nom}"{fmt} />')
        publics.append(f'    <public type="attr" name="{nom}" id="{r["id"]}" />')
    for nom in styles:
        r = res.get(("style", nom))
        if r is None:
            sys.exit(f"@android:style/{nom} n'est pas un style public d'Android")
        out.append(f'    <style name="{nom}" parent="" />')
        publics.append(f'    <public type="style" name="{nom}" id="{r["id"]}" />')
    out += publics + ["</resources>", ""]
    sortie = ICI / "res" / "values" / "cadre.xml"
    sortie.parent.mkdir(parents=True, exist_ok=True)
    sortie.write_text("\n".join(out), encoding="utf-8")
    print(f"{sortie} : {len(attrs)} attributs, {len(styles)} styles")


if __name__ == "__main__":
    main()
