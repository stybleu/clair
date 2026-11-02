// Clair Studio (V0.67) : le designer des fenetres, comme Visual Basic.
//
// Une fenetre se dessine dans Form1.ui (du JSON) : on pose des controles (bouton, texte, saisie...),
// on les deplace et on change leur taille avec le doigt, on regle leurs proprietes. Toucher deux fois
// un controle ecrit la fonction qui lui repond (bouton1_click) dans Form1.cl et y emmene.
//
// A chaque changement, le designer ecrit :
//   Form1.cl   le debut, entre deux lignes de marque : la classe Form1 (un Control par controle),
//              la lecture de l'etat envoye par la page, les routes du module web et l'appel des
//              fonctions ecrites pour chaque evenement ; ce qui suit la marque de fin est a l'auteur ;
//   Form1.html la page : les controles, et un petit programme qui envoie chaque evenement avec
//              l'etat de tous les controles, puis montre l'etat renvoye par le programme Clair.
// Les fonctions de generation n'ont pas besoin d'un navigateur (node les emploie pour les tests).
"use strict";
(function (global) {
  // ---- les sortes de controles ----
  const SORTES = {
    bouton:    { titre: "Bouton", l: 120, h: 44, texte: "Bouton", evenements: ["click"] },
    etiquette: { titre: "Texte", l: 160, h: 28, texte: "Texte", evenements: ["click"] },
    saisie:    { titre: "Saisie", l: 200, h: 40, texte: "", evenements: ["change", "enter"] },
    case:      { titre: "Case", l: 160, h: 32, texte: "Case", evenements: ["change"] },
    curseur:   { titre: "Curseur", l: 200, h: 32, texte: "", evenements: ["change"] },
    liste:     { titre: "Liste", l: 200, h: 120, texte: "", evenements: ["select"] },
    choix:     { titre: "Choix", l: 200, h: 40, texte: "", evenements: ["select"] },
    image:     { titre: "Image", l: 120, h: 120, texte: "", evenements: ["click"] },
    minuteur:  { titre: "Minuteur", l: 120, h: 30, texte: "", evenements: ["tick"] },
  };
  const EVENEMENTS_FENETRE = ["load"];
  const QUAND = {
    click: "quand on touche ", change: "quand change ", enter: "quand on appuie sur Entrée dans ",
    select: "quand on choisit dans ", tick: "à chaque tic de ", load: "quand la fenêtre s'ouvre",
  };
  const MARQUE_DEBUT = "// ==== Clair Studio : écrit par le designer à partir de ";
  const MARQUE_FIN = "// ==== fin de ce qu'écrit le designer ====";

  function controle(sorte, nom, x, y) {
    const s = SORTES[sorte];
    return {
      name: nom, kind: sorte, text: s ? s.texte : "", value: sorte === "curseur" ? 50 : 0, minimum: 0, maximum: 100, step: 1,
      checked: false, items: sorte === "liste" || sorte === "choix" ? ["Premier", "Deuxième"] : [], selected: -1,
      visible: true, enabled: true, x: x, y: y, width: s ? s.l : 360, height: s ? s.h : 640, color: "", background: "",
      size: 16, interval: 1000, source: "", message: "",
    };
  }

  function nouvelleFenetre(titre) {
    const f = controle("fenetre", "fenetre", 0, 0);
    f.text = titre;
    f.background = "#ffffff";
    f.color = "#111827";
    return { version: 1, fenetre: f, controles: [] };
  }

  // Form1.ui -> Form1 (la classe) et form1 (le debut des noms de fonctions)
  function base(chemin) { return chemin.replace(/^.*\//, "").replace(/\.(ui|cl|html)$/, ""); }
  function nomClasse(b) {
    const c = b.replace(/[^A-Za-z0-9_]/g, "_");
    return (/^[0-9]/.test(c) ? "F" : "") + c.charAt(0).toUpperCase() + c.slice(1);
  }
  function prefixe(b) { return nomClasse(b).toLowerCase(); }
  const nomValide = (n) => /^[a-z_][a-z0-9_]*$/.test(n) && n.length <= 40 && !/__/.test(n);

  function nouveauNom(ui, sorte) {
    const pris = new Set(ui.controles.map((c) => c.name));
    for (let i = 1; ; i++) if (!pris.has(sorte + i)) return sorte + i;
  }

  // ---- le programme Clair ----
  // ce que l'auteur a ecrit : ce qui suit la marque de fin
  function partieAuteur(code) {
    const i = code.indexOf(MARQUE_FIN);
    if (i < 0) return null;
    const j = code.indexOf("\n", i);
    return j < 0 ? "" : code.slice(j + 1);
  }

  function estDuDesigner(code) { return code.startsWith(MARQUE_DEBUT) && code.includes(MARQUE_FIN); }

  // les fonctions ecrites pour un evenement : "bouton1.click", "fenetre.load"
  function gestionnaires(ui, auteur, b) {
    const classe = nomClasse(b), noms = new Set(["fenetre", ...ui.controles.map((c) => c.name)]);
    const trouves = new Set();
    const re = new RegExp("^def ([a-z_][a-z0-9_]*)_(click|change|enter|select|tick|load)\\(\\s*\\w+\\s*:\\s*" + classe + "\\s*\\)", "gm");
    let m;
    while ((m = re.exec(auteur || ""))) {
      const nom = m[1], ev = m[2];
      const sorte = nom === "fenetre" ? null : (ui.controles.find((c) => c.name === nom) || {}).kind;
      const permis = nom === "fenetre" ? EVENEMENTS_FENETRE : sorte ? SORTES[sorte].evenements : [];
      if (noms.has(nom) && permis.includes(ev)) trouves.add(nom + "." + ev);
    }
    return trouves;
  }

  const AUTEUR_DEPART = "\n// Ton code : les fonctions qui répondent aux événements.\n" +
    "// Dans le dessin, touche deux fois un contrôle pour écrire la sienne.\n";

  function genererCode(ui, b, ancien) {
    const classe = nomClasse(b), p = prefixe(b);
    const auteur = ancien !== undefined && ancien !== null && partieAuteur(ancien) !== null ? partieAuteur(ancien) : AUTEUR_DEPART;
    const evs = gestionnaires(ui, auteur, b);
    const noms = ["fenetre", ...ui.controles.map((c) => c.name)];
    const l = [];
    l.push(MARQUE_DEBUT + b + ".ui. Ne pas changer jusqu'à la ligne de fin.");
    l.push("import web", "import forms", "");
    l.push("Class " + classe + " {");
    for (const n of noms) l.push("    " + n + ": Control");
    l.push("}", "");
    l.push("// la fenêtre telle que la page l'envoie");
    l.push("def " + p + "_lire(req: Request) -> " + classe + " {");
    l.push("    return " + classe + "(" + noms.map((n) => 'forms.read(req, "' + n + '")').join(", ") + ")");
    l.push("}", "");
    l.push("// la fenêtre pour la page");
    l.push("def " + p + "_json(f: " + classe + ") -> string {");
    l.push('    return "{" + ' + noms.map((n) => "forms.json(f." + n + ")").join(' + "," + ') + ' + "}"');
    l.push("}", "");
    l.push('@web.route("/")');
    l.push("def " + p + "_page() -> Response {");
    l.push('    return forms.page("' + b + '.html")');
    l.push("}", "");
    l.push('@web.route("/evenement", "POST")');
    l.push("def " + p + "_evenement(req: Request) -> Response {");
    l.push("    let f: " + classe + " = " + p + "_lire(req)");
    if (evs.size) {
      l.push('    let e: string = req.field("_evenement", "")');
      for (const ev of evs) {
        const [nom, quoi] = ev.split(".");
        l.push('    if e == "' + ev + '" {');
        l.push("        " + nom + "_" + quoi + "(f)");
        l.push("    }");
      }
    }
    l.push("    return web.json(" + p + "_json(f))");
    l.push("}", "");
    l.push("def main() -> int {");
    l.push("    return forms.run(8100)");
    l.push("}");
    l.push(MARQUE_FIN);
    return l.join("\n") + "\n" + auteur;
  }

  // la fonction d'un evenement : ecrite a la fin si elle n'y est pas ; donne le code et la ligne ou ecrire
  function ajouterGestionnaire(ui, b, ancien, nom, ev) {
    const classe = nomClasse(b);
    let code = genererCode(ui, b, ancien);
    const tete = "def " + nom + "_" + ev + "(f: " + classe + ") -> int {";
    let i = code.indexOf(tete);
    if (i < 0) {
      const quoi = nom === "fenetre" ? QUAND.load : QUAND[ev] + nom;
      code = code.replace(/\n*$/, "\n") + "\n// " + quoi + "\n" + tete + "\n    \n    return 0\n}\n";
      code = genererCode(ui, b, code);
      i = code.indexOf(tete);
    }
    const ligne = code.slice(0, i).split("\n").length + 1;     // la ligne vide sous def
    return { code, ligne };
  }

  // ---- les controles dans la page (et dans le dessin) ----
  function creerControle(doc, c) {
    let el;
    switch (c.kind) {
      case "bouton": el = doc.createElement("button"); el.type = "button"; break;
      case "saisie": el = doc.createElement(c.height >= 80 ? "textarea" : "input"); if (el.tagName === "INPUT") el.type = "text"; break;
      case "case": {
        el = doc.createElement("label");
        const b = doc.createElement("input");
        b.type = "checkbox";
        el.append(b, doc.createElement("span"));
        break;
      }
      case "curseur": el = doc.createElement("input"); el.type = "range"; break;
      case "liste": el = doc.createElement("select"); el.size = 4; break;
      case "choix": el = doc.createElement("select"); break;
      case "image": el = doc.createElement("img"); el.alt = ""; break;
      default: el = doc.createElement("div");
    }
    el.className = "cf cf-" + c.kind;
    el.dataset.nom = c.name;
    return el;
  }

  function appliquerControle(el, c, dessin) {
    const s = el.style;
    s.left = c.x + "px";
    s.top = c.y + "px";
    s.width = c.width + "px";
    s.height = c.height + "px";
    s.fontSize = c.size + "px";
    s.color = c.color || "";
    s.background = c.background || "";
    s.display = c.visible || dessin ? "" : "none";
    s.opacity = !c.visible && dessin ? "0.4" : "";
    const actif = c.enabled || !!dessin;
    switch (c.kind) {
      case "bouton": case "etiquette": el.textContent = c.text; if (c.kind === "bouton") el.disabled = !actif; break;
      case "minuteur": el.textContent = "⏱ " + c.name; break;
      case "saisie": if (el.value !== c.text) el.value = c.text; el.disabled = !actif; break;
      case "case": el.firstChild.checked = c.checked; el.firstChild.disabled = !actif; el.lastChild.textContent = c.text; break;
      case "curseur": el.min = c.minimum; el.max = c.maximum; el.step = c.step > 0 ? c.step : "any"; el.value = c.value; el.disabled = !actif; break;
      case "liste": case "choix": {
        const cle = JSON.stringify(c.items);
        if (el.dataset.items !== cle) {
          el.textContent = "";
          for (const t of c.items) { const o = el.ownerDocument.createElement("option"); o.textContent = t; el.appendChild(o); }
          el.dataset.items = cle;
        }
        el.selectedIndex = c.selected;
        el.disabled = !actif;
        if (c.kind === "liste") el.size = Math.max(2, Math.floor(c.height / (c.size * 1.45)));
        break;
      }
      case "image": if (c.source && el.getAttribute("src") !== c.source) el.setAttribute("src", c.source); if (!c.source) el.removeAttribute("src"); break;
      default: break;
    }
  }

  const CSS_CONTROLES =
    ".cf{position:absolute;box-sizing:border-box;margin:0;font-family:system-ui,sans-serif;line-height:1.25}" +
    ".cf-bouton{border:0;border-radius:8px;background:#2563eb;color:#fff;font-weight:600;padding:0 8px}" +
    ".cf-bouton:disabled{opacity:.5}" +
    ".cf-etiquette{display:flex;align-items:center;white-space:pre-wrap;overflow:hidden}" +
    ".cf-saisie{border:1px solid #9ca3af;border-radius:6px;padding:4px 8px;background:#fff;color:#111827}" +
    "textarea.cf-saisie{resize:none}" +
    ".cf-case{display:flex;align-items:center;gap:8px}.cf-case input{width:1.2em;height:1.2em;margin:0}" +
    ".cf-curseur{padding:0;accent-color:#2563eb}" +
    ".cf-liste,.cf-choix{border:1px solid #9ca3af;border-radius:6px;background:#fff;color:#111827;padding:2px 4px}" +
    ".cf-image{object-fit:contain}" +
    ".cf-minuteur{display:flex;align-items:center;justify-content:center;border:1px dashed #9ca3af;border-radius:6px;font-size:13px!important;color:#6b7280!important}";

  // ---- la page de l'application ----
  function pageApplication() {
    /* le programme de la page : ETAT, ORDRE et EVENEMENTS sont ecrits avant lui */
    const fenetre = document.getElementById("fenetre");
    const elements = {};
    const minuteurs = {};
    const attente = {};
    let file = Promise.resolve();

    function corps(ev) {
      const p = new URLSearchParams();
      p.set("_evenement", ev);
      for (const nom of Object.keys(ETAT)) {
        const c = ETAT[nom];
        for (const k of Object.keys(c)) {
          const v = c[k];
          p.set(nom + "." + k, Array.isArray(v) ? v.join("\n") : typeof v === "boolean" ? (v ? "1" : "0") : String(v));
        }
      }
      return p;
    }

    function erreur(texte) {
      let b = document.getElementById("erreur");
      if (!b) {
        b = document.createElement("pre");
        b.id = "erreur";
        b.addEventListener("click", () => b.remove());
        document.body.appendChild(b);
      }
      b.textContent = texte;
    }

    function envoyer(ev) {
      if (!EVENEMENTS.includes(ev)) return;
      file = file.then(async () => {
        let r;
        try {
          r = await fetch("evenement", { method: "POST", body: corps(ev) });
        } catch (e) {
          erreur("Le programme ne répond plus (il est arrêté ?).");
          return;
        }
        const t = await r.text();
        if (!r.ok) {
          erreur(t.replace(/<[^>]*>/g, " ").replace(/\s+/g, " ").trim());
          return;
        }
        const nouveau = JSON.parse(t);
        for (const nom of Object.keys(nouveau)) if (ETAT[nom]) ETAT[nom] = nouveau[nom];
        montrer();
        const m = ETAT.fenetre.message;
        if (m) {
          ETAT.fenetre.message = "";
          alert(m);
        }
      });
    }

    function plusTard(ev, ms) {
      clearTimeout(attente[ev]);
      attente[ev] = setTimeout(() => envoyer(ev), ms);
    }

    function montrer() {
      const f = ETAT.fenetre;
      document.title = f.text || "Application";
      fenetre.style.width = f.width + "px";
      fenetre.style.height = f.height + "px";
      fenetre.style.background = f.background || "";
      fenetre.style.color = f.color || "";
      fenetre.style.fontSize = f.size + "px";
      document.body.style.background = f.background || "";
      for (const nom of ORDRE) appliquerControle(elements[nom], ETAT[nom], false);
      for (const nom of ORDRE) {
        const c = ETAT[nom];
        if (c.kind !== "minuteur") continue;
        const ev = nom + ".tick", voulu = c.enabled && c.interval > 0 && EVENEMENTS.includes(ev) ? c.interval : 0;
        if (minuteurs[nom] && minuteurs[nom].ms === voulu) continue;
        if (minuteurs[nom]) clearInterval(minuteurs[nom].id);
        minuteurs[nom] = { ms: voulu, id: voulu ? setInterval(() => envoyer(ev), Math.max(20, voulu)) : 0 };
      }
      echelle();
    }

    function echelle() {
      const f = ETAT.fenetre, k = Math.min(window.innerWidth / f.width, 2);
      fenetre.style.transform = "scale(" + k + ")";
      document.body.style.height = f.height * k + "px";
      document.body.style.width = f.width * k + "px";
    }

    for (const nom of ORDRE) {
      const c = ETAT[nom], el = creerControle(document, c);
      elements[nom] = el;
      fenetre.appendChild(el);
      switch (c.kind) {
        case "bouton": case "etiquette": case "image":
          el.addEventListener("click", () => envoyer(nom + ".click"));
          break;
        case "saisie":
          el.addEventListener("input", () => { ETAT[nom].text = el.value; plusTard(nom + ".change", 300); });
          el.addEventListener("keydown", (e) => {
            if (e.key === "Enter" && el.tagName === "INPUT") { e.preventDefault(); ETAT[nom].text = el.value; envoyer(nom + ".enter"); }
          });
          break;
        case "case":
          el.firstChild.addEventListener("change", () => { ETAT[nom].checked = el.firstChild.checked; envoyer(nom + ".change"); });
          break;
        case "curseur":
          el.addEventListener("input", () => { ETAT[nom].value = +el.value; plusTard(nom + ".change", 120); });
          break;
        case "liste": case "choix":
          el.addEventListener("change", () => { ETAT[nom].selected = el.selectedIndex; envoyer(nom + ".select"); });
          break;
        default:
          break;
      }
    }
    window.addEventListener("resize", echelle);
    montrer();
    envoyer("fenetre.load");
  }

  const echapper = (s) => String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");

  function genererPage(ui, b, evs) {
    const etat = { fenetre: ui.fenetre };
    for (const c of ui.controles) etat[c.name] = c;
    const donnees = JSON.stringify(etat).replace(/</g, "\\u003c");
    return "<!doctype html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"utf-8\">\n" +
      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n" +
      "<title>" + echapper(ui.fenetre.text || b) + "</title>\n" +
      "<!-- écrit par le designer de Clair Studio à partir de " + echapper(b) + ".ui -->\n" +
      "<style>\nhtml,body{margin:0;padding:0}body{overflow-x:hidden;background:#e5e7eb}\n" +
      "#fenetre{position:relative;transform-origin:0 0;overflow:hidden;font-family:system-ui,sans-serif}\n" +
      ".cf-minuteur{display:none!important}\n" +
      "#erreur{position:fixed;left:0;right:0;bottom:0;margin:0;padding:10px;background:#7f1d1d;color:#fff;font:13px monospace;white-space:pre-wrap;max-height:40vh;overflow:auto}\n" +
      CSS_CONTROLES + "\n</style>\n</head>\n<body>\n<div id=\"fenetre\"></div>\n<script>\n\"use strict\";\n" +
      "const ETAT = " + donnees + ";\n" +
      "const ORDRE = " + JSON.stringify(ui.controles.map((c) => c.name)) + ";\n" +
      "const EVENEMENTS = " + JSON.stringify([...evs]) + ";\n" +
      creerControle.toString() + "\n" + appliquerControle.toString() + "\n(" + pageApplication.toString() + ")();\n" +
      "</script>\n</body>\n</html>\n";
  }

  const outils = { SORTES, MARQUE_DEBUT, MARQUE_FIN, controle, nouvelleFenetre, base, nomClasse, prefixe, nomValide, nouveauNom,
    partieAuteur, estDuDesigner, gestionnaires, genererCode, ajouterGestionnaire, genererPage, creerControle, appliquerControle, CSS_CONTROLES };
  if (typeof module !== "undefined" && module.exports) { module.exports = outils; return; }

  // ===================================================================================
  // ---- le designer dans Clair Studio ----
  const $ = (id) => document.getElementById(id);
  const S = () => global.ClairStudio;
  let chemin = null;            // Form1.ui (avec son dossier)
  let ui = null;
  let choisi = "fenetre";
  let minuterie = 0, enCours = Promise.resolve(), aSauver = false;
  let vue = null;               // les elements du designer
  const elements = {};          // nom -> element dessine
  const portes = {};            // nom -> la zone qu'on touche
  let k = 1;                    // l'echelle du dessin

  const voisin = (ext) => chemin.replace(/\.ui$/, ext);
  const tous = () => [ui.fenetre, ...ui.controles];
  const trouver = (nom) => tous().find((c) => c.name === nom);

  function construire() {
    if (vue) return;
    const d = document.createElement("section");
    d.id = "designer";
    d.hidden = true;
    d.innerHTML =
      '<div class="d-palette" id="d-palette"></div>' +
      '<div class="d-scene" id="d-scene"><div class="d-cadre" id="d-cadre"><div class="d-feuille" id="d-feuille"></div></div></div>' +
      '<div class="d-props" id="d-props"></div>';
    const editeur = $("editeur");
    editeur.parentNode.insertBefore(d, editeur.nextSibling);
    const pal = d.querySelector("#d-palette");
    for (const [sorte, s] of Object.entries(SORTES)) {
      const b = document.createElement("button");
      b.type = "button";
      b.textContent = "+ " + s.titre;
      b.addEventListener("click", () => ajouter(sorte));
      pal.appendChild(b);
    }
    const st = document.createElement("style");
    st.textContent = CSS_CONTROLES.replace(/(^|\})([^{}]+)\{/g, (m, a, sel) => a + sel.split(",").map((x) => "#d-feuille " + x).join(",") + "{");
    document.head.appendChild(st);
    vue = { d, feuille: d.querySelector("#d-feuille"), cadre: d.querySelector("#d-cadre"), scene: d.querySelector("#d-scene"), props: d.querySelector("#d-props") };
    // le bouton de l'en-tete : passer du dessin au code et retour
    const b = document.createElement("button");
    b.id = "b-dessin";
    b.className = "icone";
    b.hidden = true;
    b.addEventListener("click", basculer);
    $("b-git").parentNode.insertBefore(b, $("b-git"));
    window.addEventListener("resize", () => { if (!vue.d.hidden) dessinerTout(); });
    gestes();
  }

  // ---- montrer, cacher ----
  async function prendre(nom, texte) {
    construire();
    if (!/\.ui$/.test(nom)) { cacher(); return false; }
    try { ui = JSON.parse(texte); } catch (e) { ui = null; }
    if (!ui || !ui.fenetre || !Array.isArray(ui.controles)) {
      alert(nom + " n'est pas une fenêtre lisible.");
      return false;
    }
    chemin = nom;
    choisi = "fenetre";
    $("app").classList.add("mode-dessin");
    vue.d.hidden = false;
    const b = $("b-dessin");
    b.hidden = false;
    b.textContent = "{ }";
    b.title = "Le code de cette fenêtre (" + base(nom) + ".cl)";
    dessinerTout();
    return true;
  }

  function cacher() {
    if (!vue) return;
    $("app").classList.remove("mode-dessin");
    vue.d.hidden = true;
  }

  // dans l'editeur : un .cl ecrit par le designer a son bouton « Dessin »
  function apresOuverture(nom, texte) {
    construire();
    const b = $("b-dessin");
    if (/\.cl$/.test(nom) && estDuDesigner(texte || "")) {
      b.hidden = false;
      b.textContent = "▣";
      b.title = "Le dessin de cette fenêtre (" + base(nom) + ".ui)";
    } else if (!/\.ui$/.test(nom)) {
      b.hidden = true;
    }
  }

  async function basculer() {
    const f = S().fichier();
    if (/\.ui$/.test(f)) {
      await sauver();
      await S().ouvrir(f.replace(/\.ui$/, ".cl"));
    } else {
      await S().ouvrir(f.replace(/\.cl$/, ".ui"));
    }
  }

  // ---- dessiner ----
  function dessinerTout() {
    const f = ui.fenetre, feuille = vue.feuille;
    const largeur = vue.scene.clientWidth - 24;
    k = Math.max(0.3, Math.min(largeur / f.width, 1.6));
    feuille.style.width = f.width + "px";
    feuille.style.height = f.height + "px";
    feuille.style.background = f.background || "#fff";
    feuille.style.color = f.color || "";
    feuille.style.fontSize = f.size + "px";
    feuille.style.transform = "scale(" + k + ")";
    vue.cadre.style.width = f.width * k + "px";
    vue.cadre.style.height = f.height * k + "px";
    feuille.textContent = "";
    for (const n of Object.keys(elements)) { delete elements[n]; delete portes[n]; }
    for (const c of ui.controles) {
      const el = creerControle(document, c);
      el.tabIndex = -1;
      elements[c.name] = el;
      feuille.appendChild(el);
      appliquerControle(el, c, true);
      const p = document.createElement("div");
      p.className = "d-porte";
      p.dataset.nom = c.name;
      portes[c.name] = p;
      feuille.appendChild(p);
      placerPorte(c);
    }
    const sel = document.createElement("div");
    sel.className = "d-selection";
    sel.id = "d-selection";
    const poignee = document.createElement("div");
    poignee.className = "d-poignee";
    poignee.id = "d-poignee";
    sel.appendChild(poignee);
    feuille.appendChild(sel);
    marquer();
    proprietes();
  }

  function placerPorte(c) {
    const p = portes[c.name];
    if (!p) return;
    p.style.left = c.x + "px";
    p.style.top = c.y + "px";
    p.style.width = c.width + "px";
    p.style.height = c.height + "px";
  }

  function redessiner(c) {
    if (c.kind === "fenetre") { dessinerTout(); return; }
    const vieux = elements[c.name];
    if (vieux && c.kind === "saisie" && (vieux.tagName === "TEXTAREA") !== (c.height >= 80)) {
      const el = creerControle(document, c);       // une saisie haute devient une zone de texte
      el.tabIndex = -1;
      vieux.replaceWith(el);
      elements[c.name] = el;
    }
    appliquerControle(elements[c.name], c, true);
    placerPorte(c);
    marquer();
  }

  function marquer() {
    const sel = $("d-selection"), c = trouver(choisi);
    if (!sel) return;
    if (!c || c.kind === "fenetre") { sel.hidden = true; return; }
    sel.hidden = false;
    sel.style.left = c.x + "px";
    sel.style.top = c.y + "px";
    sel.style.width = c.width + "px";
    sel.style.height = c.height + "px";
    const t = Math.round(26 / k);
    const poignee = $("d-poignee");
    poignee.style.width = poignee.style.height = t + "px";
    poignee.style.right = poignee.style.bottom = -Math.round(t / 2) + "px";
    sel.style.borderWidth = Math.max(1, Math.round(2 / k)) + "px";
  }

  // ---- les gestes : toucher, glisser, changer la taille, toucher deux fois ----
  function gestes() {
    const feuille = vue.feuille;
    let geste = null, dernier = { nom: null, t: 0 };
    const point = (e) => {
      const r = feuille.getBoundingClientRect();
      return { x: (e.clientX - r.left) / k, y: (e.clientY - r.top) / k };
    };
    feuille.addEventListener("pointerdown", (e) => {
      const p = point(e);
      if (e.target.id === "d-poignee") {
        const c = trouver(choisi);
        geste = { sorte: "taille", c, depart: p, l: c.width, h: c.height, bouge: false };
      } else if (e.target.classList.contains("d-porte")) {
        const c = trouver(e.target.dataset.nom);
        choisir(c.name);
        geste = { sorte: "place", c, depart: p, x: c.x, y: c.y, bouge: false };
      } else {
        geste = { sorte: "rien", c: ui.fenetre, depart: p, bouge: false };
      }
      feuille.setPointerCapture(e.pointerId);
    });
    feuille.addEventListener("pointermove", (e) => {
      if (!geste || geste.sorte === "rien") return;
      const p = point(e), dx = p.x - geste.depart.x, dy = p.y - geste.depart.y;
      if (!geste.bouge && Math.hypot(dx, dy) * k < 6) return;
      geste.bouge = true;
      const c = geste.c, f = ui.fenetre, pas = (v) => Math.round(v / 4) * 4;
      if (geste.sorte === "place") {
        c.x = Math.max(0, Math.min(f.width - 8, pas(geste.x + dx)));
        c.y = Math.max(0, Math.min(f.height - 8, pas(geste.y + dy)));
      } else {
        c.width = Math.max(16, pas(geste.l + dx));
        c.height = Math.max(16, pas(geste.h + dy));
      }
      appliquerControle(elements[c.name], c, true);
      placerPorte(c);
      marquer();
    });
    const fin = (e) => {
      if (!geste) return;
      const g = geste;
      geste = null;
      if (g.bouge) {
        if (g.sorte === "taille" && g.c.kind === "saisie") redessiner(g.c);
        proprietes();
        plusTard();
        return;
      }
      if (e.type === "pointercancel") return;
      const nom = g.c.name, maintenant = Date.now();
      if (g.sorte === "rien") choisir("fenetre");
      if (dernier.nom === nom && maintenant - dernier.t < 450) {
        dernier = { nom: null, t: 0 };
        const evs = nom === "fenetre" ? EVENEMENTS_FENETRE : SORTES[g.c.kind].evenements;
        allerAuCode(nom, evs[0]);
        return;
      }
      dernier = { nom, t: maintenant };
    };
    feuille.addEventListener("pointerup", fin);
    feuille.addEventListener("pointercancel", fin);
  }

  function choisir(nom) {
    if (choisi === nom) return;
    choisi = nom;
    marquer();
    proprietes();
  }

  function ajouter(sorte) {
    const f = ui.fenetre, s = SORTES[sorte];
    const haut = vue.scene.scrollTop / k;
    const n = ui.controles.length;
    const c = controle(sorte, nouveauNom(ui, sorte), Math.max(0, Math.round((f.width - s.l) / 2 / 4) * 4),
      Math.min(f.height - s.h, Math.round((haut + 40 + (n % 6) * 12) / 4) * 4));
    if (sorte === "bouton") c.text = "Bouton " + c.name.replace(/\D/g, "");
    ui.controles.push(c);
    choisi = c.name;
    dessinerTout();
    plusTard();
  }

  function supprimer(nom) {
    if (!confirm("Enlever " + nom + " de la fenêtre ? (ses fonctions restent dans le code)")) return;
    ui.controles = ui.controles.filter((c) => c.name !== nom);
    choisi = "fenetre";
    dessinerTout();
    plusTard();
  }

  // ---- le panneau des proprietes ----
  const CHAMPS = {
    commun: [["text", "Texte", "texte"], ["x", "x", "nombre"], ["y", "y", "nombre"], ["width", "Largeur", "nombre"], ["height", "Hauteur", "nombre"],
      ["size", "Taille du texte", "nombre"], ["color", "Couleur", "couleur"], ["background", "Fond", "couleur"],
      ["visible", "Visible", "oui"], ["enabled", "Actif", "oui"]],
    fenetre: [["text", "Titre", "texte"], ["width", "Largeur", "nombre"], ["height", "Hauteur", "nombre"], ["size", "Taille du texte", "nombre"],
      ["color", "Couleur", "couleur"], ["background", "Fond", "couleur"]],
    curseur: [["minimum", "Minimum", "nombre"], ["maximum", "Maximum", "nombre"], ["step", "Pas", "nombre"], ["value", "Valeur", "nombre"]],
    case: [["checked", "Cochée", "oui"]],
    liste: [["items", "Éléments (un par ligne)", "lignes"], ["selected", "Choisi (-1 : aucun)", "nombre"]],
    choix: [["items", "Éléments (un par ligne)", "lignes"], ["selected", "Choisi (-1 : aucun)", "nombre"]],
    image: [["source", "Image (static/photo.png)", "texte"]],
    minuteur: [["interval", "Intervalle (ms)", "nombre"], ["enabled", "Actif", "oui"]],
  };

  function champs(c) {
    if (c.kind === "fenetre") return CHAMPS.fenetre;
    let l = CHAMPS.commun.slice();
    if (c.kind === "curseur" || c.kind === "image") l = l.filter((d) => d[0] !== "text");
    if (c.kind === "minuteur") l = [["x", "x", "nombre"], ["y", "y", "nombre"]];
    return l.concat(CHAMPS[c.kind] || []);
  }

  function proprietes() {
    const c = trouver(choisi) || ui.fenetre, p = vue.props;
    p.textContent = "";
    const tete = document.createElement("div");
    tete.className = "d-tete";
    if (c.kind === "fenetre") {
      const t = document.createElement("strong");
      t.textContent = "La fenêtre";
      tete.appendChild(t);
    } else {
      const nom = document.createElement("input");
      nom.value = c.name;
      nom.className = "d-nom";
      nom.setAttribute("aria-label", "Nom");
      nom.autocapitalize = "off";
      nom.spellcheck = false;
      nom.addEventListener("change", () => {
        const v = nom.value.trim();
        if (v === c.name) return;
        if (!nomValide(v) || v === "fenetre" || trouver(v)) {
          nom.classList.add("faux");
          nom.title = "Un nom en minuscules, chiffres et _, qui n'est pas déjà pris";
          return;
        }
        nom.classList.remove("faux");
        delete elements[c.name];
        c.name = v;
        choisi = v;
        dessinerTout();
        plusTard();
      });
      const sorte = document.createElement("span");
      sorte.className = "discret";
      sorte.textContent = SORTES[c.kind].titre;
      const sup = document.createElement("button");
      sup.type = "button";
      sup.className = "danger";
      sup.textContent = "Enlever";
      sup.addEventListener("click", () => supprimer(c.name));
      tete.append(nom, sorte, sup);
    }
    p.appendChild(tete);
    // les evenements : vers la fonction qui repond
    const evs = document.createElement("div");
    evs.className = "d-evenements";
    for (const ev of c.kind === "fenetre" ? EVENEMENTS_FENETRE : SORTES[c.kind].evenements) {
      const b = document.createElement("button");
      b.type = "button";
      b.textContent = c.name + "_" + ev + " →";
      b.title = "Écrire ou retrouver la fonction : " + (c.kind === "fenetre" ? QUAND.load : QUAND[ev] + c.name);
      b.addEventListener("click", () => allerAuCode(c.name, ev));
      evs.appendChild(b);
    }
    p.appendChild(evs);
    const grille = document.createElement("div");
    grille.className = "d-grille";
    for (const [cle, titre, sorte] of champs(c)) {
      const l = document.createElement("label");
      const t = document.createElement("span");
      t.textContent = titre;
      let entree;
      if (sorte === "lignes") {
        entree = document.createElement("textarea");
        entree.rows = 3;
        entree.value = c[cle].join("\n");
      } else {
        entree = document.createElement("input");
        if (sorte === "nombre") { entree.type = "number"; entree.inputMode = "decimal"; entree.value = c[cle]; }
        else if (sorte === "oui") { entree.type = "checkbox"; entree.checked = !!c[cle]; }
        else if (sorte === "couleur") { entree.type = "text"; entree.placeholder = "#2563eb, red…"; entree.value = c[cle]; }
        else { entree.type = "text"; entree.value = c[cle]; }
      }
      entree.autocapitalize = "off";
      entree.addEventListener(sorte === "oui" ? "change" : "input", () => {
        if (sorte === "nombre") { const v = parseFloat(entree.value); if (!isFinite(v)) return; c[cle] = cle === "selected" ? Math.round(v) : v; }
        else if (sorte === "oui") c[cle] = entree.checked;
        else if (sorte === "lignes") c[cle] = entree.value.split("\n").map((s) => s.trim()).filter((s) => s !== "");
        else c[cle] = entree.value;
        redessiner(c);
        plusTard();
      });
      l.append(t, entree);
      if (sorte === "oui") l.classList.add("d-oui");
      grille.appendChild(l);
    }
    p.appendChild(grille);
  }

  // ---- enregistrer : Form1.ui, puis Form1.cl et Form1.html ----
  function plusTard() {
    aSauver = true;
    clearTimeout(minuterie);
    minuterie = setTimeout(sauver, 500);
  }

  async function lire(nom) {
    const r = await S().api("/api/lire/" + S().enc(nom));
    return r.ok ? r.text() : null;
  }

  async function ecrire(nom, texte) { await S().api("/api/ecrire/" + S().enc(nom), texte); }

  // ecrit le code et la page a partir du dessin (le code de l'auteur est garde) ; donne le code
  async function generer(code) {
    const b = base(chemin), cl = voisin(".cl");
    const ancien = code !== undefined ? code : await lire(cl);
    if (ancien !== null && ancien !== undefined && ancien.trim() !== "" && !estDuDesigner(ancien) && code === undefined) {
      throw new Error(cl + " existe déjà et n'a pas été écrit par le designer : renomme-le d'abord.");
    }
    const nouveau = genererCode(ui, b, ancien);
    if (nouveau !== ancien) await ecrire(cl, nouveau);
    await ecrire(voisin(".html"), genererPage(ui, b, gestionnaires(ui, partieAuteur(nouveau), b)));
    return nouveau;
  }

  function sauver() {
    clearTimeout(minuterie);
    if (!aSauver || !ui) return enCours;
    aSauver = false;
    const texte = JSON.stringify(ui, null, 1) + "\n", nom = chemin;
    enCours = enCours.then(async () => {
      $("etat-sauve").textContent = "enregistrement…";
      await ecrire(nom, texte);
      try {
        await generer();
        $("etat-sauve").textContent = "enregistré";
      } catch (e) {
        $("etat-sauve").textContent = "";
        alert(e.message);
      }
    });
    return enCours;
  }

  // toucher deux fois : la fonction de l'evenement, ecrite si besoin, puis le code a cet endroit
  async function allerAuCode(nom, ev) {
    await sauver();
    const b = base(chemin), cl = voisin(".cl");
    const ancien = await lire(cl);
    const r = ajouterGestionnaire(ui, b, ancien, nom, ev);
    if (r.code !== ancien) await ecrire(cl, r.code);
    await generer(r.code);
    await S().ouvrir(cl);
    S().aLaLigne(r.ligne);
  }

  // ▶ : le dessin et le code sont remis d'accord, puis c'est Form1.cl qui est lance
  async function avantLancement(fichier) {
    if (/\.ui$/.test(fichier) && chemin === fichier) {
      aSauver = true;
      await sauver();
      return fichier.replace(/\.ui$/, ".cl");
    }
    if (/\.cl$/.test(fichier)) {
      const code = S().texte();
      if (!estDuDesigner(code)) return fichier;
      const texteUi = await lire(fichier.replace(/\.cl$/, ".ui"));
      if (texteUi === null) return fichier;
      const garde = { chemin, ui };
      try {
        chemin = fichier.replace(/\.cl$/, ".ui");
        ui = JSON.parse(texteUi);
        const nouveau = await generer(code);
        if (nouveau !== code) S().remplacer(nouveau);
      } finally {
        chemin = garde.chemin;
        ui = garde.ui;
      }
    }
    return fichier;
  }

  // + Fenêtre : Form1.ui, Form1.cl et Form1.html, puis le dessin
  async function nouvelle(dossier) {
    let n = prompt("Nom de la nouvelle fenêtre (une application) :", "Fenetre1");
    if (!n) return null;
    n = n.trim().replace(/\.(ui|cl)$/, "");
    if (!/^[A-Za-z][A-Za-z0-9_]{0,40}$/.test(n)) { alert("Un nom en lettres, chiffres et _ (sans espace ni accent)."); return null; }
    const ch = S().joindre(dossier || "", n + ".ui");
    if ((await lire(ch)) !== null || (await lire(ch.replace(/\.ui$/, ".cl"))) !== null) { alert(n + " existe déjà."); return null; }
    ui = nouvelleFenetre(n);
    const titre = controle("etiquette", "etiquette1", 24, 24);
    titre.text = "Ma première application";
    titre.width = 312;
    titre.height = 40;
    titre.size = 22;
    ui.controles.push(titre);
    chemin = ch;
    await ecrire(ch, JSON.stringify(ui, null, 1) + "\n");
    await generer(null);
    return ch;
  }

  global.ClairDesigner = { prendre, cacher, apresOuverture, avantLancement, nouvelle, sauver, outils };
})(typeof window !== "undefined" ? window : globalThis);
