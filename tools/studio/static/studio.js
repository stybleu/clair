// Clair Studio (V0.58, V0.59) : l'editeur. Le serveur est tools/studio/studio.cl.
"use strict";
(function () {
  const $ = (id) => document.getElementById(id);
  const code = $("code"), couleurs = $("couleurs"), numeros = $("numeros"), sortie = $("sortie");
  let fichier = null;            // le nom du fichier ouvert
  let sauve = true, minuterie = 0;
  let enCours = null;            // le numero du programme qui tourne
  let enTests = false, texteTests = "";
  let lignesErr = new Set();     // les lignes d'une erreur au dernier lancement
  let lignesVerif = new Map();   // V0.59 : les lignes des erreurs trouvees en tapant -> leur message
  let debut = 0;
  const enc = encodeURIComponent;

  // ---- les couleurs du code ----
  const MOTS = new Set(["def", "let", "return", "if", "else", "while", "for", "in", "Class", "import", "from", "as",
    "and", "or", "not", "true", "false", "break", "continue", "print", "absolue", "range", "test", "assert", "assert_equal"]);
  const TYPES = new Set(["int", "float", "string", "bool", "list", "dict", "measure", "bigint", "bigfloat", "certfloat",
    "u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64", "float32", "matrix", "ptr", "uint1024",
    "Request", "Response", "RunResult", "Process"]);
  const esc = (s) => s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");

  function colorier(ligne) {
    let out = "", i = 0;
    const n = ligne.length;
    while (i < n) {
      const c = ligne[i];
      if (c === "/" && ligne[i + 1] === "/") { out += '<span class="c">' + esc(ligne.slice(i)) + "</span>"; break; }
      if (c === '"') {
        let j = i + 1;
        while (j < n && ligne[j] !== '"') j += ligne[j] === "\\" ? 2 : 1;
        j = Math.min(j + 1, n);
        out += '<span class="s">' + esc(ligne.slice(i, j)) + "</span>";
        i = j;
        continue;
      }
      if (/[0-9]/.test(c) && !/[A-Za-z_0-9]/.test(ligne[i - 1] || "")) {
        const m = /^(0[xXbBoO][0-9a-fA-F_]+|[0-9][0-9_]*(\.[0-9_]+)?([eE][+-]?[0-9]+)?)/.exec(ligne.slice(i));
        out += '<span class="n">' + esc(m[0]) + "</span>";
        i += m[0].length;
        continue;
      }
      if (/[A-Za-z_]/.test(c)) {
        const m = /^[A-Za-z_][A-Za-z0-9_]*/.exec(ligne.slice(i))[0];
        const apres = ligne.slice(i + m.length);
        let cls = MOTS.has(m) ? "k" : TYPES.has(m) ? "t" : /^\s*\(/.test(apres) ? "f" : "";
        out += cls ? '<span class="' + cls + '">' + esc(m) + "</span>" : esc(m);
        i += m.length;
        continue;
      }
      if (c === "@" || c === "±") { out += '<span class="d">' + esc(c) + "</span>"; i++; continue; }
      out += esc(c);
      i++;
    }
    return out;
  }

  // V0.59 : chaque ligne n'est coloriee qu'une fois, et seules les lignes visibles sont dessinees :
  // un fichier de 10 000 lignes reste aussi rapide qu'un fichier de 10
  const dejaColories = new Map();
  function couleur(ligne) {
    let h = dejaColories.get(ligne);
    if (h === undefined) {
      if (dejaColories.size > 5000) dejaColories.clear();
      h = colorier(ligne);
      dejaColories.set(ligne, h);
    }
    return h;
  }

  let hauteur = 21, lignes = [""], plusLongue = 0, texteDessine = null;
  function mesurer() { hauteur = parseFloat(getComputedStyle(code).lineHeight) || 21; }

  function dessiner() {
    if (texteDessine !== code.value) {
      texteDessine = code.value;
      lignes = texteDessine.split("\n");
      plusLongue = 0;
      for (const l of lignes) if (l.length > plusLongue) plusLongue = l.length;
    }
    const total = lignes.length;
    const haut = code.scrollTop, vue = code.clientHeight || 800;
    const premier = Math.max(0, Math.floor(haut / hauteur) - 20);
    const dernier = Math.min(total, Math.ceil((haut + vue) / hauteur) + 20);
    let html = '<span class="largeur" style="width:' + plusLongue + 'ch"></span>';
    if (premier) html += '<span class="vide" style="height:' + premier * hauteur + 'px"></span>';
    let nums = "";
    for (let k = premier; k < dernier; k++) {
      const n = k + 1;
      const cls = lignesErr.has(n) ? "err" : lignesVerif.has(n) ? "verif" : "";
      html += '<span class="l' + (cls ? " " + cls : "") + '">' + (couleur(lignes[k]) || " ") + "</span>";
      nums += cls ? '<span class="' + cls + '">' + n + "</span>\n" : n + "\n";
    }
    if (total > dernier) html += '<span class="vide" style="height:' + (total - dernier) * hauteur + 'px"></span>';
    couleurs.innerHTML = html + '<span class="l"> </span>';
    numeros.style.paddingTop = 8 + premier * hauteur + "px";
    numeros.innerHTML = nums;
    suivreDefilement();
  }

  function suivreDefilement() {
    couleurs.scrollTop = code.scrollTop;
    couleurs.scrollLeft = code.scrollLeft;
    numeros.style.transform = "translateY(" + -code.scrollTop + "px)";
  }

  let image = 0;
  function redessiner() {
    if (image) return;
    image = requestAnimationFrame(() => { image = 0; dessiner(); });
  }

  // ---- ecrire dans le texte en gardant l'annulation du navigateur ----
  function inserer(t) {
    code.focus();
    if (!document.execCommand("insertText", false, t)) code.setRangeText(t, code.selectionStart, code.selectionEnd, "end");
    modifie();
  }

  function debutLigne() {
    const avant = code.value.slice(0, code.selectionStart);
    return avant.slice(avant.lastIndexOf("\n") + 1);
  }

  code.addEventListener("beforeinput", (e) => {
    if (e.isComposing) return;
    if (e.inputType === "insertLineBreak" || e.inputType === "insertParagraph") {
      // la nouvelle ligne garde le retrait, un de plus apres {
      e.preventDefault();
      const ligne = debutLigne();
      let retrait = /^\s*/.exec(ligne)[0];
      if (/\{\s*$/.test(ligne)) retrait += "    ";
      inserer("\n" + retrait);
    } else if (e.inputType === "insertText" && e.data === "}") {
      const ligne = debutLigne();
      if (/^ {4,}$/.test(ligne) && code.selectionStart === code.selectionEnd) {
        e.preventDefault();
        code.setSelectionRange(code.selectionStart - 4, code.selectionStart);
        inserer("}");
      }
    }
  });
  code.addEventListener("keydown", (e) => {
    if (e.key === "Tab") {
      e.preventDefault();
      if (propositions.length) accepter(propositions[0]);
      else inserer("    ");
    }
    if (e.key === "Escape" && !$("aide").hidden) { aideFermee = true; cacherAide(); }
    if ((e.ctrlKey || e.metaKey) && e.key === "s") { e.preventDefault(); enregistrer(); }
    if ((e.ctrlKey || e.metaKey) && e.key === "Enter") { e.preventDefault(); lancer(); }
  });
  code.addEventListener("input", modifie);
  code.addEventListener("scroll", () => { suivreDefilement(); redessiner(); });
  for (const ev of ["click", "keyup", "select"]) code.addEventListener(ev, (e) => { if (e.key !== "Escape") planifierAide(); });
  window.addEventListener("resize", () => { mesurer(); redessiner(); });
  if (window.ResizeObserver) new ResizeObserver(redessiner).observe(code);   // la console s'ouvre, le clavier aussi

  for (const b of document.querySelectorAll("#touches button")) {
    b.addEventListener("pointerdown", (e) => e.preventDefault());      // le clavier reste ouvert
    b.addEventListener("click", () => {
      if (b.dataset.cmd) { code.focus(); document.execCommand(b.dataset.cmd); modifie(); return; }
      inserer(b.dataset.t);
    });
  }

  function modifie() {
    if (lignesErr.size) lignesErr = new Set();
    sauve = false;
    aideFermee = false;
    $("etat-sauve").textContent = "modifié";
    clearTimeout(minuterie);
    minuterie = setTimeout(enregistrer, 400);
    redessiner();
    planifierAide();
  }

  // ---- le serveur ----
  async function api(chemin, corps) {
    const r = await fetch(chemin, corps === undefined ? {} : { method: "POST", body: corps });
    if (r.status === 403) { alert("Ouvre l'adresse affichée dans Termux (avec ?t=...)."); throw new Error("403"); }
    return r;
  }

  async function enregistrer() {
    clearTimeout(minuterie);
    if (!fichier || sauve) return;
    const texte = code.value;
    await api("/api/ecrire/" + enc(fichier), texte);
    if (code.value === texte) {
      sauve = true;
      $("etat-sauve").textContent = "enregistré";
      verifier();
    }
  }

  async function ouvrir(nom) {
    await enregistrer();
    const r = await api("/api/lire/" + enc(nom));
    if (!r.ok) return false;
    fichier = nom;
    code.value = await r.text();
    sauve = true;
    lignesErr = new Set();
    lignesVerif = new Map();
    montrerDiag([]);
    cacherAide();
    $("nom-fichier").textContent = nom;
    $("etat-sauve").textContent = "";
    document.title = nom + " · Clair Studio";
    try { localStorage.setItem("clair-studio-fichier", nom); } catch (e) { /* rien */ }
    code.scrollTop = 0;
    dessiner();
    verifier();
    return true;
  }

  async function listerFichiers() {
    const r = await api("/api/fichiers");
    return r.json();
  }

  // ---- V0.59 : les erreurs pendant qu'on tape (clair --check, sans compiler) ----
  // "semantic error: ... (line 3)", "(orbite.cl, line 3)", "parse error at 3:5: ..."
  function lireErreurs(texte) {
    const liste = [];
    for (const l of texte.split("\n")) {
      const dessus = /the error above is in (\S+)/.exec(l);
      if (dessus) { if (liste.length) liste[liste.length - 1].fichier = dessus[1]; continue; }
      const m = /\((?:in )?(?:([^()\n,]+), )?line (\d+)\)/.exec(l) || /error at (\d+):\d+/.exec(l);
      if (!m) continue;
      const avecFichier = m.length === 3;
      let message = l.replace(/^[a-z ]*error( at \d+:\d+)?: /, "").replace(m[0], "").trim();
      liste.push({ fichier: avecFichier ? m[1] || null : null, ligne: +(avecFichier ? m[2] : m[1]), message });
    }
    for (const e of liste) if (e.fichier === fichier) e.fichier = null;
    return liste;
  }

  let numVerif = 0;
  async function verifier() {
    if (!fichier || !fichier.endsWith(".cl")) return;
    const n = ++numVerif, texte = code.value;
    let r;
    try { r = await (await api("/api/verifier/" + enc(fichier), "")).json(); } catch (e) { return; }
    if (n !== numVerif || code.value !== texte) return;   // une autre verification suit
    const erreurs = r.ok ? [] : lireErreurs(r.erreurs);
    if (!r.ok && !erreurs.length) erreurs.push({ fichier: null, ligne: 0, message: r.erreurs.trim().split("\n")[0] });
    lignesVerif = new Map();
    for (const e of erreurs) if (!e.fichier && e.ligne && !lignesVerif.has(e.ligne)) lignesVerif.set(e.ligne, e.message);
    montrerDiag(erreurs);
    dessiner();
  }

  let diagCible = null;
  function montrerDiag(erreurs) {
    const d = $("diag");
    const e = erreurs[0];
    diagCible = e || null;
    if (!e) { d.hidden = true; return; }
    d.textContent = "⚠ " + (e.fichier ? e.fichier + ", " : "") + (e.ligne ? "ligne " + e.ligne + " : " : "") + e.message;
    d.title = d.textContent;
    d.hidden = false;
  }
  $("diag").addEventListener("click", () => { if (diagCible) allerAFichier(diagCible.fichier, diagCible.ligne); });

  async function allerAFichier(nom, n) {
    if (nom && nom !== fichier && !(await ouvrir(nom))) return;
    if (n) allerA(n);
  }

  // ---- lancer ----
  function ecrireSortie(texte, classe, cible) {
    cible = cible || sortie;
    const bas = cible.scrollTop + cible.clientHeight >= cible.scrollHeight - 4;
    const morceaux = texte.split(/(\((?:in )?(?:[^()\n,]+, )?line \d+\)|error at \d+:\d+)/);
    for (const m of morceaux) {
      if (!m) continue;
      const ici = /^\((?:in )?(?:([^()\n,]+), )?line (\d+)\)$/.exec(m), at = /^error at (\d+):/.exec(m);
      if (ici || at) {
        const a = document.createElement("a");
        a.textContent = m;
        a.dataset.ligne = ici ? ici[2] : at[1];
        const autre = ici && ici[1] && ici[1] !== fichier ? ici[1] : "";
        if (autre) a.dataset.fichier = autre;
        else lignesErr.add(+a.dataset.ligne);
        cible.appendChild(a);
      } else {
        for (const morceau of m.split(/(?<=\n)/)) {     // ligne par ligne : seules celles d'une erreur en rouge
          const s = document.createElement("span");
          if (classe) s.className = classe;
          else if (/error/.test(morceau)) s.className = "err";
          s.textContent = morceau;
          cible.appendChild(s);
        }
      }
    }
    if (bas) cible.scrollTop = cible.scrollHeight;
  }

  sortie.addEventListener("click", (e) => {
    const n = e.target.dataset && e.target.dataset.ligne;
    if (n) allerAFichier(e.target.dataset.fichier || null, +n);
  });

  function allerA(n) {
    const tout = code.value.split("\n");
    let pos = 0;
    for (let k = 0; k < n - 1 && k < tout.length; k++) pos += tout[k].length + 1;
    code.focus();
    code.setSelectionRange(pos, pos + (tout[n - 1] || "").length);
    code.scrollTop = Math.max(0, (n - 3) * hauteur);
    dessiner();
  }

  function etat(texte) { $("etat").textContent = texte; }

  function boutons(tourne) {
    $("b-lancer").disabled = tourne;
    $("b-tester").disabled = tourne;
    $("b-arreter").hidden = !tourne;
    $("f-entree").hidden = !tourne || enTests;
  }

  // compile (le serveur ne recompile pas un programme qui n'a pas change), puis suit le programme
  async function demarrer(chemin, quoi) {
    if (!fichier || enCours) return;
    await enregistrer();
    sortie.textContent = "";
    lignesErr = new Set();
    $("console").hidden = false;
    etat("Compilation…");
    $("b-lancer").disabled = true;
    $("b-tester").disabled = true;
    let r;
    try {
      r = await (await api(chemin + enc(fichier), "")).json();
    } catch (e) {
      etat("Le serveur ne répond pas");
      boutons(false);
      return;
    }
    if (!r.ok) {
      ecrireSortie(r.erreurs, "err");
      if (/has no test/.test(r.erreurs))
        ecrireSortie("\nCe fichier n'a pas encore de test. Par exemple :\n\ntest \"addition\" {\n    assert_equal(1 + 1, 2)\n}\n", "info");
      etat("Erreur à la compilation (" + r.duree + " s)");
      enTests = false;
      boutons(false);
      dessiner();
      if (lignesErr.size) allerA(Math.min(...lignesErr));
      return;
    }
    enCours = r.id;
    debut = performance.now();
    etat((quoi === "tests" ? "Tests en cours…" : "En cours…") + (r.cache ? " (déjà compilé)" : " (compilé en " + r.duree + " s)"));
    boutons(true);
    suivre();
  }

  function lancer() { enTests = false; demarrer("/api/lancer/", "programme"); }
  function tester() { enTests = true; texteTests = ""; demarrer("/api/tester/", "tests"); }

  async function suivre() {
    if (!enCours) return;
    let r;
    try {
      r = await (await api("/api/suivre/" + enCours)).json();
    } catch (e) {
      setTimeout(suivre, 500);
      return;
    }
    if (r.sortie) {
      if (enTests) texteTests += r.sortie;
      ecrireSortie(r.sortie);
    }
    if (r.fini) {
      const duree = ((performance.now() - debut) / 1000).toFixed(2);
      enCours = null;
      if (enTests) afficherTests(texteTests, r.code, duree);
      else etat((r.code === 0 ? "Terminé" : r.code >= 128 ? "Arrêté" : "Terminé avec le code " + r.code) + " en " + duree + " s");
      enTests = false;
      boutons(false);
      if (lignesErr.size) dessiner();
      return;
    }
    setTimeout(suivre, 150);
  }

  // V0.59 : ce qu'ecrit clair --test, en liste : ✓ ou ✗ pour chaque test, et ce qu'a dit chaque test rate
  function afficherTests(texte, codeSortie, duree) {
    const resultats = [], echecs = new Map();
    let dans = null, sections = false;
    for (const l of texte.split("\n")) {
      let m = /^test (.*) \.\.\. (ok|FAILED)$/.exec(l);
      if (m && !sections) { resultats.push({ nom: m[1], ok: m[2] === "ok" }); continue; }
      if (l === "failures:") { if (sections && echecs.size) dans = null; sections = true; continue; }
      m = /^---- (.*) ----$/.exec(l);
      if (sections && m) { dans = m[1]; echecs.set(dans, ""); continue; }
      if (/^test result: /.test(l)) { dans = null; continue; }
      if (dans !== null) echecs.set(dans, echecs.get(dans) + l + "\n");
    }
    if (!resultats.length) {
      etat(codeSortie >= 128 ? "Tests arrêtés" : "Les tests n'ont pas pu tourner (code " + codeSortie + ")");
      return;
    }
    lignesErr = new Set();
    sortie.textContent = "";
    let reussis = 0;
    for (const t of resultats) {
      const div = document.createElement("div");
      div.className = "test " + (t.ok ? "ok" : "ko");
      div.textContent = (t.ok ? "✓ " : "✗ ") + t.nom;
      sortie.appendChild(div);
      if (t.ok) { reussis++; continue; }
      const detail = document.createElement("div");
      detail.className = "test-detail";
      ecrireSortie((echecs.get(t.nom) || "").replace(/\n+$/, "") + "\n", null, detail);
      sortie.appendChild(detail);
    }
    const rates = resultats.length - reussis;
    if (codeSortie >= 128) etat("Tests arrêtés : " + reussis + " réussi" + (reussis > 1 ? "s" : "") + " avant l'arrêt");
    else etat((rates ? "✗ " : "✓ ") + reussis + " réussi" + (reussis > 1 ? "s" : "") + (rates ? ", " + rates + " raté" + (rates > 1 ? "s" : "") : "") + " en " + duree + " s");
    sortie.scrollTop = 0;
    dessiner();
  }

  $("b-lancer").addEventListener("click", lancer);
  $("b-tester").addEventListener("click", tester);
  $("b-arreter").addEventListener("click", () => { if (enCours) api("/api/arreter/" + enCours, ""); });
  $("b-fermer").addEventListener("click", () => { if (!enCours) $("console").hidden = true; });
  $("f-entree").addEventListener("submit", (e) => {
    e.preventDefault();
    if (!enCours) return;
    const t = $("entree").value + "\n";
    ecrireSortie(t, "info");
    $("entree").value = "";
    api("/api/entree/" + enCours, t);
  });

  // ---- V0.59 : l'autocompletion ----
  // Ce qui est propose depend de l'endroit : apres os. ou web. les fonctions du module (lues dans
  // sa source), apres x. les champs et methodes de la classe de x, apres : ou -> les types, sinon
  // les variables visibles a cet endroit, les fonctions et classes du fichier, puis Clair lui-meme.
  const BASE = [
    ["print", "valeur", "", "affiche une valeur, puis passe à la ligne"],
    ["len", "x", "int", "la longueur d'une liste, d'une chaîne ou d'un dict"],
    ["str", "x", "string", "x écrit en texte"],
    ["int", "x", "int", "un entier, depuis un float ou un texte"],
    ["float", "x", "float", "un float, depuis un entier ou un texte"],
    ["range", "début, fin", "", "les entiers de début à fin - 1 (range(fin) : de 0 à fin - 1)"],
    ["input", "question", "string", "une ligne tapée au clavier"],
    ["input_int", "", "int", "un entier tapé au clavier"],
    ["format", "x, \".2f\"", "string", "un nombre écrit avec 2 chiffres après la virgule"],
    ["read_text", "chemin", "string", "tout le texte d'un fichier"],
    ["read_lines", "chemin", "list[string]", "les lignes d'un fichier"],
    ["write_file", "chemin, texte", "int", "écrit un fichier"],
    ["read_bytes", "chemin", "list[u8]", "les octets d'un fichier"],
    ["write", "texte", "int", "écrit sans passer à la ligne"],
    ["split", "texte, séparateur", "list[string]", "coupe un texte"],
    ["strip", "texte", "string", "le texte sans les espaces du début et de la fin"],
    ["load_csv", "chemin", "", "un fichier CSV"],
    ["save_csv", "chemin, lignes", "int", "écrit un fichier CSV"],
    ["encode", "texte", "list[u8]", "les octets UTF-8 d'un texte"],
    ["decode", "octets", "string", "le texte de ces octets"],
    ["ord", "c", "int", "le numéro d'un caractère"],
    ["chr", "n", "string", "le caractère de ce numéro"],
    ["abs", "x", "", "la valeur absolue"], ["min", "a, b", "", "le plus petit"], ["max", "a, b", "", "le plus grand"],
    ["sqrt", "x", "float", "la racine carrée"], ["pow", "x, y", "float", "x puissance y"],
    ["floor", "x", "float", "arrondi vers le bas"], ["ceil", "x", "float", "arrondi vers le haut"],
    ["exp", "x", "float", ""], ["log", "x", "float", "logarithme népérien"], ["sin", "x", "float", ""],
    ["cos", "x", "float", ""], ["tan", "x", "float", ""], ["atan2", "y, x", "float", ""], ["hypot", "x, y", "float", ""],
    ["random", "", "float", "un float au hasard entre 0 et 1"], ["randint", "a, b", "int", "un entier au hasard de a à b"],
    ["uniform", "a, b", "float", "un float au hasard entre a et b"], ["gauss", "moyenne, écart", "float", "loi normale"],
    ["choice", "liste", "", "un élément au hasard"], ["shuffle", "liste", "", "mélange la liste"],
    ["seed", "n", "", "le même hasard à chaque lancement"],
    ["matrix", "lignes, colonnes", "", "une matrice de zéros"],
    ["assert", "condition, message", "", "dans un test : rate si la condition est fausse"],
    ["assert_equal", "valeur, attendu", "", "dans un test : rate si les deux sont différents"],
  ].map(([nom, p, ret, doc]) => ({ nom, params: p ? p.split(", ") : [], ret, doc, sorte: "f" }));
  const MOTS_CLES = ["def", "let", "return", "if", "else", "while", "for", "in", "Class", "import", "and", "or", "not",
    "true", "false", "break", "continue", "test"];
  const TYPES_BASE = ["int", "float", "string", "bool", "list[int]", "list[float]", "list[string]", "dict[string, int]",
    "dict[string, string]", "float32", "u8", "u16", "u32", "u64", "i8", "i16", "i32", "measure", "bigint", "bigfloat"];
  const MATH = ["sqrt", "exp", "exp2", "expm1", "log", "log10", "log2", "log1p", "sin", "cos", "tan", "asin", "acos", "atan",
    "sinh", "cosh", "tanh", "asinh", "acosh", "atanh", "floor", "ceil", "trunc", "fabs", "cbrt", "erf", "erfc", "gamma",
    "lgamma", "degrees", "radians"];

  const sansChaines = (l) => l.replace(/"(?:[^"\\]|\\.)*"/g, '""');
  function accolades(l) {
    const t = sansChaines(l).replace(/\/\/.*$/, "");
    let n = 0;
    for (const c of t) { if (c === "{") n++; else if (c === "}") n--; }
    return n;
  }
  function lireParams(p) { return p.split(",").map((s) => s.trim()).filter(Boolean); }

  // les fonctions, classes (champs, methodes), constantes et imports d'une source Clair
  function analyser(texte, estModule) {
    const a = { fonctions: new Map(), classes: new Map(), constantes: new Map(), imports: new Map() };
    let prof = 0, classe = null, doc = [];
    for (const [k, l] of texte.split("\n").entries()) {
      const t = l.trim();
      if (t.startsWith("//")) { doc.push(t.slice(2).trim()); continue; }
      let m;
      const garder = (nom) => !estModule || !nom.startsWith("__");
      if (prof === 0 && (m = /^import\s+([A-Za-z_]\w*)(?:\s+as\s+([A-Za-z_]\w*))?/.exec(t))) a.imports.set(m[2] || m[1], m[1]);
      else if (prof === 0 && (m = /^Class\s+([A-Za-z_]\w*)/.exec(t))) {
        classe = { nom: m[1], champs: [], methodes: new Map(), doc: doc.join(" "), ligne: k + 1 };
        if (garder(m[1])) a.classes.set(m[1], classe);
      } else if ((m = /^def\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*(?:->\s*([^{]+?))?\s*\{?\s*$/.exec(t))) {
        const f = { nom: m[1], params: lireParams(m[2]), ret: (m[3] || "").trim(), doc: doc.join(" "), sorte: "f", ligne: k + 1 };
        if (classe && prof === 1) { if (garder(f.nom)) classe.methodes.set(f.nom, f); }
        else if (prof === 0 && garder(f.nom) && !(estModule && f.nom === "main")) a.fonctions.set(f.nom, f);
      } else if (classe && prof === 1 && (m = /^([A-Za-z_]\w*)\s*:\s*(.+)$/.exec(t))) {
        classe.champs.push({ nom: m[1], type: m[2].trim(), doc: doc.join(" ") });
      } else if (prof === 0 && (m = /^let\s+([A-Za-z_]\w*)\s*:\s*([^=]+?)\s*=/.exec(t))) {
        a.constantes.set(m[1], m[2]);
      }
      doc = [];
      prof += accolades(l);
      if (prof <= 0) { prof = 0; classe = null; }
    }
    return a;
  }

  let memo = { texte: null, a: null, quand: 0 };
  function analyseFichier() {
    const t = performance.now();
    if (memo.texte !== code.value && (!memo.a || t - memo.quand > 400 || code.value.length < 20000))
      memo = { texte: code.value, a: analyser(code.value, false), quand: t };
    return memo.a;
  }

  // les modules importes : os et web lus dans leur source (/api/module), les autres dans le dossier
  const modules = new Map();
  function module(nom) {
    if (nom === "math") {
      if (!modules.has("math")) {
        const a = analyser("", true);
        for (const f of MATH) a.fonctions.set(f, { nom: f, params: ["x"], ret: "float", doc: "", sorte: "f" });
        a.fonctions.set("pow", { nom: "pow", params: ["x", "y"], ret: "float", doc: "", sorte: "f" });
        a.fonctions.set("atan2", { nom: "atan2", params: ["y", "x"], ret: "float", doc: "", sorte: "f" });
        a.fonctions.set("hypot", { nom: "hypot", params: ["x", "y"], ret: "float", doc: "", sorte: "f" });
        a.fonctions.set("gcd", { nom: "gcd", params: ["a", "b"], ret: "int", doc: "", sorte: "f" });
        a.fonctions.set("isqrt", { nom: "isqrt", params: ["n"], ret: "int", doc: "", sorte: "f" });
        for (const c of ["pi", "e", "tau", "inf", "nan"]) a.constantes.set(c, "float");
        modules.set("math", { analyse: a, fixe: true });
      }
      return modules.get("math").analyse;
    }
    const m = modules.get(nom);
    if (!m || (!m.fixe && !m.enCours && performance.now() - m.quand > 5000)) chargerModule(nom);
    return m ? m.analyse : null;
  }
  async function chargerModule(nom) {
    const integre = nom === "os" || nom === "web" || nom === "gfx";
    const m = modules.get(nom) || { analyse: null };
    m.enCours = true;
    m.quand = performance.now();
    modules.set(nom, m);
    try {
      const r = await fetch(integre ? "/api/module/" + nom : "/api/lire/" + enc(nom + ".cl"));
      if (r.ok) m.analyse = analyser(await r.text(), true);
      m.fixe = integre && r.ok;
    } catch (e) { /* rien */ }
    m.enCours = false;
    m.quand = performance.now();
    planifierAide();
  }

  function trouverClasse(type, a) {
    type = (type || "").replace(/^[A-Za-z_]\w*\./, "");
    if (a.classes.has(type)) return a.classes.get(type);
    for (const nom of new Set(a.imports.values())) {
      const am = module(nom);
      if (am && am.classes.has(type)) return am.classes.get(type);
    }
    return null;
  }

  // le type d'une expression simple : 3, "a", Point(...), os.run(...), carre(2), x
  function typeDe(expr, a, vars) {
    expr = expr.trim();
    let m;
    if (/^-?\d+$/.test(expr)) return "int";
    if (/^-?\d*\.\d+/.test(expr)) return "float";
    if (/^"/.test(expr)) return "string";
    if (/^(true|false)$/.test(expr)) return "bool";
    if ((m = /^([A-Za-z_]\w*)\.([A-Za-z_]\w*)\s*\(/.exec(expr))) {
      const mod = a.imports.get(m[1]);
      if (mod) {
        const am = module(mod);
        if (am && am.classes.has(m[2])) return m[2];
        if (am && am.fonctions.has(m[2])) return am.fonctions.get(m[2]).ret;
      }
      const c = trouverClasse(vars.get(m[1]), a);
      if (c && c.methodes.has(m[2])) return c.methodes.get(m[2]).ret;
      return "";
    }
    if ((m = /^([A-Za-z_]\w*)\s*\(/.exec(expr))) {
      if (trouverClasse(m[1], a)) return m[1];
      if (a.fonctions.has(m[1])) return a.fonctions.get(m[1]).ret;
      const b = BASE.find((f) => f.nom === m[1]);
      return b ? b.ret : "";
    }
    if ((m = /^([A-Za-z_]\w*)$/.exec(expr))) return vars.get(m[1]) || "";
    return "";
  }

  // les variables visibles a la position pos : les parametres de la fonction, les let et for au-dessus
  function variables(texte, pos, a) {
    // le bloc de premier niveau (def, test, Class) qui contient pos commence en colonne 0
    let depart = pos;
    while (depart > 0) {
      const d = texte.lastIndexOf("\n", depart - 2) + 1;
      if (/^(def|test|Class)\b/.test(texte.slice(d, d + 6))) { depart = d; break; }
      depart = d;
    }
    const avant = texte.slice(depart, pos).split("\n");
    let vars = new Map(), prof = 0, classe = null;
    for (const l of avant) {
      const t = l.trim();
      let m;
      if (prof === 0 && (m = /^Class\s+([A-Za-z_]\w*)/.exec(t))) classe = m[1];
      if ((m = /^def\s+[A-Za-z_]\w*\s*\(([^)]*)\)/.exec(t)) && (prof === 0 || (classe && prof === 1))) {
        vars = new Map();
        if (classe && prof === 1) vars.set("self", classe);
        for (const p of lireParams(m[1])) {
          const q = /^([A-Za-z_]\w*)\s*:\s*(.+)$/.exec(p);
          if (q) vars.set(q[1], q[2]);
        }
      } else if (prof === 0 && /^test\s+"/.test(t)) {
        vars = new Map();
      } else if ((m = /^let\s+([A-Za-z_]\w*)\s*(?::\s*([^=]+?))?\s*=\s*(.*)$/.exec(t))) {
        vars.set(m[1], m[2] ? m[2].trim() : typeDe(m[3], a, vars));
      } else if ((m = /^for\s+([A-Za-z_]\w*)(?:\s*,\s*([A-Za-z_]\w*))?\s+in\s+(.+?)\s*\{?$/.exec(t))) {
        const src = m[3];
        const tl = /^range\s*\(/.test(src) ? "int" : typeDe(src, a, vars);
        const elem = /^list\[(.*)\]$/.exec(tl);
        const te = /^range\s*\(/.test(src) ? "int" : elem ? elem[1] : tl === "string" ? "string" : "";
        if (m[2]) { vars.set(m[1], "int"); vars.set(m[2], te); } else vars.set(m[1], te);
      }
      prof += accolades(l);
      if (prof <= 0) { prof = 0; classe = null; }
    }
    return vars;
  }

  const fonctionsClasse = (c) => ({ nom: c.nom, params: c.champs.map((f) => f.nom + ": " + f.type), ret: c.nom,
    doc: c.doc, sorte: "c" });

  // ce que l'on est en train d'ecrire : objet.debut, debut d'un mot, un type...
  function contexte() {
    const pos = code.selectionStart;
    if (pos !== code.selectionEnd) return null;
    const texte = code.value;
    const ligne = texte.slice(texte.lastIndexOf("\n", pos - 1) + 1, pos);
    const vue = sansChaines(ligne);
    if (/"/.test(vue.replace(/""/g, "")) || /\/\//.test(vue)) return null;   // dans un texte ou un commentaire
    let m = /([A-Za-z_]\w*)\.([A-Za-z_]\w*)?$/.exec(vue);
    if (m) return { objet: m[1], prefixe: m[2] || "", debut: pos - (m[2] || "").length, vue, pos };
    m = /(?:^|[^\w.])([A-Za-z_]\w*)$/.exec(vue);
    const prefixe = m ? m[1] : "";
    const reste = vue.slice(0, vue.length - prefixe.length);
    const enType = /(\blet\s+\w+|[(,]\s*\w+|^\s*\w+)\s*:\s*(?:\w+\[[\w\[\], ]*)?$/.test(reste) || /->\s*(?:\w+\[[\w\[\], ]*)?$/.test(reste);
    return { prefixe, debut: pos - prefixe.length, vue, pos, enType };
  }

  function candidats(ctx) {
    const texte = code.value, a = analyseFichier();
    const vars = variables(texte, ctx.pos, a);
    const liste = [];
    const ajouter = (nom, sorte, detail, info) => liste.push({ nom, sorte, detail, info });
    if (ctx.objet) {
      const mod = a.imports.get(ctx.objet);
      if (mod) {
        const am = module(mod);
        if (am) {
          for (const f of am.fonctions.values()) ajouter(f.nom, "f", "", f);
          for (const c of am.classes.values()) ajouter(c.nom, "c", "", fonctionsClasse(c));
          for (const [n, t] of am.constantes) ajouter(n, "v", t, null);
        }
      } else {
        const t = vars.get(ctx.objet) || (a.constantes.get(ctx.objet) || "");
        if (/^list\[/.test(t)) ajouter("append", "f", "", { nom: "append", params: ["valeur"], ret: "", doc: "ajoute à la fin de la liste", sorte: "f" });
        const c = trouverClasse(t, a);
        if (c) {
          for (const f of c.champs) ajouter(f.nom, "v", f.type, null);
          for (const f of c.methodes.values()) ajouter(f.nom, "f", "", f);
        }
      }
    } else if (ctx.enType) {
      for (const t of TYPES_BASE) ajouter(t, "t", "", null);
      for (const c of a.classes.values()) ajouter(c.nom, "t", "", null);
      for (const nom of new Set(a.imports.values())) {
        const am = module(nom);
        if (am) for (const c of am.classes.values()) ajouter(c.nom, "t", nom, null);
      }
    } else {
      if (!ctx.prefixe) return [];
      for (const [n, t] of vars) ajouter(n, "v", t, null);
      for (const [n, t] of a.constantes) ajouter(n, "v", t, null);
      for (const f of a.fonctions.values()) ajouter(f.nom, "f", "", f);
      for (const c of a.classes.values()) ajouter(c.nom, "c", "", fonctionsClasse(c));
      for (const n of a.imports.keys()) ajouter(n, "m", "module", null);
      for (const f of BASE) ajouter(f.nom, "f", "", f);
      for (const k of MOTS_CLES) ajouter(k, "k", "", null);
      for (const t of ["int", "float", "string", "bool"]) ajouter(t, "t", "", null);
    }
    // d'abord ce qui commence par ce qui est tape, puis ce qui le contient ; sans doublon
    const p = ctx.prefixe.toLowerCase(), vus = new Set(), debuts = [], dedans = [];
    for (const c of liste) {
      const n = c.nom.toLowerCase();
      if (vus.has(c.nom) || (c.nom === ctx.prefixe && !ctx.objet)) continue;
      if (n.startsWith(p)) debuts.push(c);
      else if (p.length >= 2 && n.includes(p)) dedans.push(c);
      else continue;
      vus.add(c.nom);
    }
    return debuts.concat(dedans).slice(0, 40);
  }

  // la fonction dont la parenthese est encore ouverte avant le curseur, et quel argument on ecrit
  function appelOuvert(vue) {
    let prof = 0, virgules = 0;
    for (let i = vue.length - 1; i >= 0; i--) {
      const c = vue[i];
      if (c === ")" || c === "]" || c === "}") prof++;
      else if (c === "(" || c === "[" || c === "{") {
        if (prof > 0) { prof--; continue; }
        if (c !== "(") return null;
        const devant = vue.slice(0, i);
        if (/\bdef\s+[A-Za-z_]\w*\s*$/.test(devant)) return null;      // def f(... n'est pas un appel
        const m = /([A-Za-z_]\w*)(?:\.([A-Za-z_]\w*))?\s*$/.exec(devant);
        return m ? { objet: m[2] ? m[1] : null, nom: m[2] || m[1], arg: virgules } : null;
      } else if (c === "," && prof === 0) virgules++;
    }
    return null;
  }

  function signature(appel) {
    const texte = code.value, a = analyseFichier();
    if (appel.objet) {
      const mod = a.imports.get(appel.objet);
      if (mod) {
        const am = module(mod);
        if (!am) return null;
        if (am.fonctions.has(appel.nom)) return am.fonctions.get(appel.nom);
        return am.classes.has(appel.nom) ? fonctionsClasse(am.classes.get(appel.nom)) : null;
      }
      const vars = variables(texte, code.selectionStart, a);
      const c = trouverClasse(vars.get(appel.objet), a);
      if (c && c.methodes.has(appel.nom)) return c.methodes.get(appel.nom);
      if (appel.nom === "append") return { nom: "append", params: ["valeur"], ret: "", doc: "ajoute à la fin de la liste" };
      return null;
    }
    if (a.fonctions.has(appel.nom)) return a.fonctions.get(appel.nom);
    const c = trouverClasse(appel.nom, a);
    if (c) return fonctionsClasse(c);
    return BASE.find((f) => f.nom === appel.nom) || null;
  }

  function texteSignature(f, arg) {
    const params = f.params.map((p, i) => (i === arg ? "<b>" + esc(p) + "</b>" : esc(p))).join(", ");
    return esc(f.nom) + "(" + params + ")" + (f.ret ? " → " + esc(f.ret) : "") + (f.doc ? ' <span class="discret">' + esc(f.doc) + "</span>" : "");
  }

  let propositions = [], aideFermee = false, aideImage = 0;
  function planifierAide() {
    if (aideImage) return;
    aideImage = requestAnimationFrame(() => { aideImage = 0; majAide(); });
  }
  function cacherAide() { propositions = []; $("aide").hidden = true; }

  function majAide() {
    if (!fichier || !fichier.endsWith(".cl") || aideFermee || document.activeElement !== code) return cacherAide();
    const ctx = contexte();
    if (!ctx) return cacherAide();
    propositions = candidats(ctx);
    const appel = appelOuvert(ctx.vue.slice(0, ctx.vue.length - ctx.prefixe.length));
    const sig = appel ? signature(appel) : null;
    const ligne = $("signature"), liste = $("suggestions");
    liste.textContent = "";
    for (const p of propositions) {
      const b = document.createElement("button");
      b.className = "sug " + p.sorte;
      b.textContent = p.nom;
      if (p.detail) {
        const s = document.createElement("span");
        s.textContent = p.detail;
        b.appendChild(s);
      }
      b.addEventListener("pointerdown", (e) => e.preventDefault());    // le clavier reste ouvert
      b.addEventListener("click", () => accepter(p));
      liste.appendChild(b);
    }
    liste.hidden = !propositions.length;
    liste.scrollLeft = 0;
    const premier = propositions[0];
    if (premier && premier.info) ligne.innerHTML = texteSignature(premier.info, -1);
    else if (sig) ligne.innerHTML = texteSignature(sig, appel.arg);
    else ligne.innerHTML = "";
    ligne.hidden = !ligne.innerHTML;
    $("aide").hidden = liste.hidden && ligne.hidden;
  }

  function accepter(p) {
    const ctx = contexte();
    if (!ctx) return;
    code.focus();
    code.setSelectionRange(ctx.debut, ctx.pos);
    const fonction = (p.sorte === "f" || p.sorte === "c") && code.value[ctx.pos] !== "(";
    inserer(fonction ? p.nom + "()" : p.nom);
    if (fonction && p.info && p.info.params.length) {
      const q = code.selectionStart - 1;
      code.setSelectionRange(q, q);
    }
    planifierAide();
  }

  // ---- le C ----
  $("b-c").addEventListener("click", async () => {
    if (!fichier) return;
    await enregistrer();
    $("titre-c").textContent = "Le C écrit par Clair pour " + fichier;
    $("texte-c").textContent = await (await api("/api/c/" + enc(fichier))).text();
    $("fenetre-c").hidden = false;
  });
  $("b-fermer-c").addEventListener("click", () => { $("fenetre-c").hidden = true; });

  // ---- les fichiers ----
  async function montrerFichiers() {
    const liste = await listerFichiers();
    const ul = $("liste");
    ul.textContent = "";
    for (const f of liste) {
      const li = document.createElement("li");
      li.textContent = f.nom;
      const t = document.createElement("span");
      t.className = "discret";
      t.textContent = f.taille < 1024 ? f.taille + " o" : Math.round(f.taille / 1024) + " Ko";
      li.appendChild(t);
      if (f.nom === fichier) li.className = "actif";
      li.addEventListener("click", async () => { await ouvrir(f.nom); fermerTiroir(); });
      ul.appendChild(li);
    }
    $("tiroir").hidden = false;
    $("voile").hidden = false;
  }
  function fermerTiroir() { $("tiroir").hidden = true; $("voile").hidden = true; }
  $("b-fichiers").addEventListener("click", montrerFichiers);
  $("voile").addEventListener("click", fermerTiroir);

  function nomValide(nom) {
    nom = (nom || "").trim();
    if (!nom) return null;
    if (!/\.[A-Za-z0-9]+$/.test(nom)) nom += ".cl";
    if (nom.startsWith(".") || nom.includes("/")) { alert("Un nom sans / et sans . au début."); return null; }
    return nom;
  }

  $("b-nouveau").addEventListener("click", async () => {
    const nom = nomValide(prompt("Nom du nouveau fichier :", "programme.cl"));
    if (!nom) return;
    const existants = (await listerFichiers()).map((f) => f.nom);
    if (existants.includes(nom)) { await ouvrir(nom); fermerTiroir(); return; }
    await api("/api/ecrire/" + enc(nom), nom.endsWith(".cl") ? "def main() -> int {\n    print(\"Bonjour\")\n    return 0\n}\n" : "");
    await ouvrir(nom);
    fermerTiroir();
  });
  $("b-renommer").addEventListener("click", async () => {
    if (!fichier) return;
    const nom = nomValide(prompt("Nouveau nom de " + fichier + " :", fichier));
    if (!nom || nom === fichier) return;
    await enregistrer();
    const r = await (await api("/api/renommer/" + enc(fichier) + "/" + enc(nom), "")).json();
    if (!r.ok) { alert(r.erreur); return; }
    await ouvrir(nom);
    montrerFichiers();
  });
  $("b-supprimer").addEventListener("click", async () => {
    if (!fichier || !confirm("Supprimer " + fichier + " ?")) return;
    const r = await (await api("/api/supprimer/" + enc(fichier), "")).json();
    if (!r.ok) { alert(r.erreur); return; }
    fichier = null;
    sauve = true;
    const reste = await listerFichiers();
    if (reste.length) await ouvrir(reste[0].nom);
    else { code.value = ""; $("nom-fichier").textContent = "…"; montrerDiag([]); dessiner(); }
    montrerFichiers();
  });

  window.addEventListener("beforeunload", () => { if (!sauve && fichier) navigator.sendBeacon("/api/ecrire/" + enc(fichier), code.value); });
  document.addEventListener("visibilitychange", () => { if (document.hidden) enregistrer(); });
  code.addEventListener("blur", () => setTimeout(() => { if (document.activeElement !== code) cacherAide(); }, 150));

  // ---- au debut : le dernier fichier ouvert, sinon le premier .cl ----
  (async function () {
    mesurer();
    dessiner();
    let dernier = null;
    try { dernier = localStorage.getItem("clair-studio-fichier"); } catch (e) { /* rien */ }
    const liste = await listerFichiers();
    const noms = liste.map((f) => f.nom);
    const choix = dernier && noms.includes(dernier) ? dernier : noms.find((n) => n.endsWith(".cl")) || noms[0];
    if (choix) await ouvrir(choix);
    else montrerFichiers();
  })();
})();
