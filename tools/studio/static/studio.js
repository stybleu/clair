// Clair Studio (V0.58 a V0.64.1) : l'editeur. Le serveur est tools/studio/studio.cl.
"use strict";
(function () {
  const $ = (id) => document.getElementById(id);
  const code = $("code"), couleurs = $("couleurs"), numeros = $("numeros"), sortie = $("sortie");
  let fichier = null;            // le nom du fichier ouvert
  let sauve = true, minuterie = 0;
  let enCours = null;            // le numero du programme qui tourne
  let enTests = false, texteTests = "", enAttente = "", fichierLance = null;
  let lignesErr = new Set();     // les lignes d'une erreur au dernier lancement
  let lignesVerif = new Map();   // V0.59 : les lignes des erreurs trouvees en tapant -> leur message
  let lignesGit = new Map();     // V0.60 : les lignes changees depuis la derniere version -> "a", "m" ou "d"
  let debut = 0;
  // V0.62 : un fichier peut etre dans un sous-dossier : "meteo/station.cl" (chaque morceau encode, / garde)
  const enc = (chemin) => String(chemin).split("/").map(encodeURIComponent).join("/");
  const dossierDe = (chemin) => (chemin && chemin.includes("/") ? chemin.slice(0, chemin.lastIndexOf("/")) : "");
  const nomDe = (chemin) => (chemin || "").slice((chemin || "").lastIndexOf("/") + 1);
  const joindre = (d, n) => (d ? d + "/" + n : n);

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
      const g = lignesGit.get(n);
      nums += '<span class="nl' + (cls ? " " + cls : "") + (g ? " g" + g : "") + '">' + n + "</span>";
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
  if (window.ResizeObserver) new ResizeObserver(() => codeRedimensionne()).observe(code);   // la console s'ouvre, le clavier aussi

  for (const b of document.querySelectorAll("#touches button")) {
    b.addEventListener("pointerdown", (e) => e.preventDefault());      // le clavier reste ouvert
    b.addEventListener("click", () => {
      if (b.dataset.cmd) { code.focus(); document.execCommand(b.dataset.cmd); modifie(); return; }
      inserer(b.dataset.t);
    });
  }

  // ---- V0.64.1 : la barre des touches et l'aide suivent le clavier ----
  // Un navigateur qui suit interactive-widget=resizes-content (index.html) rapetisse deja la page quand le
  // clavier s'ouvre. D'autres posent le clavier par-dessus la page : elle prend alors la taille de ce qui se
  // voit (visualViewport), sauf pendant un zoom. Clavier ouvert dans le code : la console se cache pour laisser
  // de la place ; dans l'entree d'un programme : l'editeur se cache, la console prend la place.
  const vue = window.visualViewport;
  let hauteurPleine = 0, largeurVue = 0, hauteurAvant = 0;
  function suivreClavier() {
    const app = $("app"), champ = document.activeElement;
    const page = document.documentElement.clientHeight;
    const zoom = vue && Math.abs(vue.scale - 1) > 0.01;
    const recouvre = !!vue && !zoom && (page - vue.height > 1 || vue.offsetTop > 1);
    app.classList.toggle("ajuste", recouvre);
    if (recouvre) {
      app.style.setProperty("--visible", vue.height + "px");
      app.style.setProperty("--decalage", vue.offsetTop + "px");
    }
    const visible = recouvre ? vue.height : page;
    const largeur = document.documentElement.clientWidth;
    if (Math.abs(largeur - largeurVue) > 1) { largeurVue = largeur; hauteurPleine = 0; }   // l'ecran a tourne
    hauteurPleine = Math.max(hauteurPleine, visible);
    const clavier = (champ === code || champ === $("entree")) && visible < hauteurPleine * 0.8;
    app.classList.toggle("clavier", clavier);
    app.classList.toggle("dans-code", clavier && champ === code);
    app.classList.toggle("dans-entree", clavier && champ !== code);
    if (clavier && champ === code && visible < hauteurAvant - 1) requestAnimationFrame(montrerCurseur);
    hauteurAvant = visible;
  }
  // la ligne du curseur reste visible quand l'editeur rapetisse (le clavier, l'aide ou une erreur apparaissent)
  function hautCurseur() {
    const v = code.value, fin = code.selectionEnd;
    let n = 0;
    for (let i = v.indexOf("\n"); i >= 0 && i < fin; i = v.indexOf("\n", i + 1)) n++;
    return 8 + n * hauteur;                        // 8 : le haut de la zone de texte
  }
  function montrerCurseur() {
    const y = hautCurseur();
    if (y < code.scrollTop) code.scrollTop = Math.max(0, y - hauteur);
    else if (y + hauteur > code.scrollTop + code.clientHeight) code.scrollTop = y + 2 * hauteur - code.clientHeight;
  }
  let hauteurCode = 0;
  function codeRedimensionne() {
    const avant = hauteurCode;
    hauteurCode = code.clientHeight;
    if (document.activeElement === code && hauteurCode < avant) {
      const y = hautCurseur();
      if (y >= code.scrollTop && y + hauteur <= code.scrollTop + avant) montrerCurseur();   // il se voyait
    }
    redessiner();
  }
  if (vue) { vue.addEventListener("resize", suivreClavier); vue.addEventListener("scroll", suivreClavier); }
  window.addEventListener("resize", suivreClavier);
  document.addEventListener("focusin", suivreClavier);
  document.addEventListener("focusout", () => setTimeout(suivreClavier, 0));
  suivreClavier();

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
      apresEnregistrement();
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
    if (lignesGit.size) lignesGit = new Map();
    dessiner();
    verifier();
    majLignesGit();
    return true;
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
    for (const e of liste) {
      if (e.fichier === nomDe(fichier)) e.fichier = null;
      else if (e.fichier && !e.fichier.includes("/")) e.fichier = joindre(dossierDe(fichier), e.fichier);
    }
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
  // V0.61 : plot.show() ecrit une ligne ESC[clair:plot NOM] : la figure prend sa place dans la console
  const MARQUE = /\x1b\[clair:(plot|anim|image) ([^\]\n]*)\]\n?/;
  function ecrireSortie(texte, classe, cible) {
    cible = cible || sortie;
    const bas = cible.scrollTop + cible.clientHeight >= cible.scrollHeight - 4;
    let reste = texte, m;
    while ((m = MARQUE.exec(reste))) {
      if (m.index) ecrireTexte(reste.slice(0, m.index), classe, cible);
      ajouterFigure(m[1], m[2], cible);
      reste = reste.slice(m.index + m[0].length);
    }
    if (reste) ecrireTexte(reste, classe, cible);
    if (bas) cible.scrollTop = cible.scrollHeight;
  }

  // une figure : le SVG de plot, sans rien qui puisse s'executer, a la largeur de la console
  function nettoyerSvg(texte) {
    const doc = new DOMParser().parseFromString(texte, "image/svg+xml");
    const svg = doc.documentElement;
    if (!svg || svg.nodeName !== "svg" || doc.querySelector("parsererror")) return null;
    for (const e of svg.querySelectorAll("script, foreignObject, iframe, a")) e.remove();
    for (const e of [svg, ...svg.querySelectorAll("*")]) {
      for (const a of [...e.attributes]) {
        const n = a.name.toLowerCase(), v = a.value.trim().toLowerCase();
        if (n.startsWith("on") || (/href$/.test(n) && !v.startsWith("#") && !v.startsWith("data:image/png"))) e.removeAttribute(a.name);
      }
    }
    svg.removeAttribute("width");
    svg.removeAttribute("height");
    return document.importNode(svg, true);
  }

  function ajouterFigure(sorte, nom, cible) {
    const boite = document.createElement("div");
    boite.className = "figure";
    cible.appendChild(boite);
    if (sorte === "image") {
      const img = document.createElement("img");
      img.src = "/api/image/" + enc(joindre(dossierDe(fichierLance), nom));
      img.alt = nom;
      img.addEventListener("click", () => agrandir(img.cloneNode(), null, nom));
      boite.appendChild(img);
      return;
    }
    fetch("/api/figure/" + enc(nom)).then((r) => (r.ok ? r.text() : Promise.reject(r.status))).then((t) => {
      const bas = cible.scrollTop + cible.clientHeight >= cible.scrollHeight - 4;
      const svg = nettoyerSvg(t);
      if (!svg) throw new Error("svg");
      boite.appendChild(svg);
      if (sorte === "anim") controlesAnimation(boite, svg);
      svg.addEventListener("click", () => agrandir(nettoyerSvg(t), t, nom));
      if (bas) cible.scrollTop = cible.scrollHeight;
    }).catch(() => { boite.textContent = "(figure " + nom + " illisible)"; });
  }

  // une animation : lecture, pause, et un curseur pour aller a une image
  function controlesAnimation(boite, svg) {
    const n = +svg.getAttribute("data-frames") || 1, fps = +svg.getAttribute("data-fps") || 10;
    const barre = document.createElement("div");
    barre.className = "anim-controles";
    const bouton = document.createElement("button");
    bouton.textContent = "❚❚";
    bouton.setAttribute("aria-label", "Pause");
    const curseur = document.createElement("input");
    curseur.type = "range";
    curseur.min = "0";
    curseur.max = String(n - 1);
    curseur.value = "0";
    const info = document.createElement("span");
    info.className = "discret";
    barre.append(bouton, curseur, info);
    boite.appendChild(barre);
    let lecture = true;
    bouton.addEventListener("click", () => {
      if (lecture) svg.pauseAnimations(); else svg.unpauseAnimations();
      lecture = !lecture;
      bouton.textContent = lecture ? "❚❚" : "▶";
    });
    curseur.addEventListener("input", () => {
      svg.pauseAnimations();
      lecture = false;
      bouton.textContent = "▶";
      svg.setCurrentTime((+curseur.value + 0.5) / fps);
      info.textContent = +curseur.value + 1 + " / " + n;
    });
    (function suivreAnimation() {
      if (!boite.isConnected) return;
      if (lecture) {
        const k = Math.floor(svg.getCurrentTime() * fps) % n;
        curseur.value = String(k);
        info.textContent = k + 1 + " / " + n;
      }
      requestAnimationFrame(suivreAnimation);
    })();
  }

  // une figure en grand : zoom, et l'enregistrer dans le dossier
  let figureTexte = null;
  function agrandir(element, texte, nom) {
    if (!element) return;
    figureTexte = texte;
    const corps = $("figure-corps");
    corps.textContent = "";
    corps.appendChild(element);
    corps.style.setProperty("--zoom", "1");
    if (texte && element.getAttribute("data-frames")) controlesAnimation(corps, element);
    $("b-figure-garder").hidden = !texte;
    $("figure-info").textContent = "";
    $("fenetre-figure").hidden = false;
  }
  $("b-fermer-figure").addEventListener("click", () => { $("fenetre-figure").hidden = true; $("figure-corps").textContent = ""; });
  for (const [id, f] of [["b-figure-plus", 1.5], ["b-figure-moins", 1 / 1.5]]) {
    $(id).addEventListener("click", () => {
      const c = $("figure-corps"), z = Math.min(6, Math.max(1, (+c.style.getPropertyValue("--zoom") || 1) * f));
      c.style.setProperty("--zoom", String(z));
    });
  }
  $("b-figure-garder").addEventListener("click", async () => {
    if (!figureTexte) return;
    const nom = nomValide(prompt("Nom du fichier de la figure :", "figure.svg"));
    if (!nom) return;
    await api("/api/ecrire/" + enc(nom), figureTexte);
    $("figure-info").textContent = "enregistrée : " + nom;
  });

  function ecrireTexte(texte, classe, cible) {
    const morceaux = texte.split(/(\((?:in )?(?:[^()\n,]+, )?line \d+\)|error at \d+:\d+)/);
    for (const m of morceaux) {
      if (!m) continue;
      const ici = /^\((?:in )?(?:([^()\n,]+), )?line (\d+)\)$/.exec(m), at = /^error at (\d+):/.exec(m);
      if (ici || at) {
        const a = document.createElement("a");
        a.textContent = m;
        a.dataset.ligne = ici ? ici[2] : at[1];
        const autre = ici && ici[1] && ici[1] !== nomDe(fichier) ? joindre(dossierDe(fichier), ici[1]) : "";
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
    enAttente = "";
    fichierLance = fichier;
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
    // une ligne ESC[clair:... coupee entre deux lectures attend la suite
    let texte = enAttente + (r.sortie || "");
    enAttente = "";
    const coupe = texte.lastIndexOf("\x1b[");
    if (!r.fini && coupe >= 0 && texte.indexOf("\n", coupe) < 0) {
      enAttente = texte.slice(coupe);
      texte = texte.slice(0, coupe);
    }
    if (texte) {
      if (enTests) texteTests += texte;
      ecrireSortie(texte);
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

  // V0.64 : tout ce que le programme a ecrit (sans les figures), dans le presse-papiers
  $("b-copier").addEventListener("click", async () => {
    const copie = sortie.cloneNode(true);
    for (const f of copie.querySelectorAll(".figure")) f.replaceWith("[figure]\n");
    for (const t of copie.querySelectorAll(".test")) t.append("\n");
    const texte = (($("etat").textContent || "") + "\n" + copie.textContent).trim() + "\n";
    let ok = false;
    try { await navigator.clipboard.writeText(texte); ok = true; } catch (e) { /* plus bas */ }
    if (!ok) {                     // sans presse-papiers moderne : la vieille methode
      const zone = document.createElement("textarea");
      zone.value = texte;
      zone.style.position = "fixed";
      zone.style.opacity = "0";
      document.body.appendChild(zone);
      zone.select();
      try { ok = document.execCommand("copy"); } catch (e) { ok = false; }
      zone.remove();
    }
    const b = $("b-copier");
    b.textContent = ok ? "Copié ✓" : "Échec";
    setTimeout(() => { b.textContent = "Copier"; }, 1500);
  });
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
    const integre = nom === "os" || nom === "web" || nom === "plot";
    const m = modules.get(nom) || { analyse: null };
    m.enCours = true;
    m.quand = performance.now();
    modules.set(nom, m);
    try {
      const r = await fetch(integre ? "/api/module/" + nom : "/api/lire/" + enc(joindre(dossierDe(fichier), nom + ".cl")));
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

  // ---- V0.60 : Git, les modifications et les versions du dossier ----
  // Le serveur lance git dans le dossier des programmes ; ici on lit ce que git ecrit.
  let depot = null;              // { git, depot, prefixe, statut, distant, identite }
  let ongletGit = "chg", gitQuand = 0;

  async function gitEtat() {
    try { depot = await (await api("/api/git/etat")).json(); } catch (e) { depot = null; }
    gitQuand = performance.now();
    const n = depot && depot.depot ? lireStatut().fichiers.length : 0;
    $("git-badge").textContent = n > 99 ? "99+" : String(n);
    $("git-badge").hidden = !n;
    return depot;
  }

  // "## main...origin/main [ahead 1]", puis "XY chemin" (chemins depuis la racine du depot)
  function lireStatut() {
    const r = { branche: "", avance: 0, retard: 0, fichiers: [] };
    if (!depot || !depot.depot) return r;
    for (const l of depot.statut.split("\n")) {
      if (!l) continue;
      if (l.startsWith("## ")) {
        const t = l.slice(3).replace(/^(No commits yet on|Initial commit on) /, "");
        r.branche = t.split(/\.\.\.| /)[0];
        const a = /ahead (\d+)/.exec(t), b = /behind (\d+)/.exec(t);
        r.avance = a ? +a[1] : 0;
        r.retard = b ? +b[1] : 0;
        continue;
      }
      const xy = l.slice(0, 2);
      let chemin = l.slice(3);
      if (chemin.includes(" -> ")) chemin = chemin.split(" -> ").pop();
      if (/^".*"$/.test(chemin)) chemin = chemin.slice(1, -1).replace(/\\(.)/g, "$1");
      if (depot.prefixe && chemin.startsWith(depot.prefixe)) chemin = chemin.slice(depot.prefixe.length);
      const lettre = xy === "??" ? "N" : /R/.test(xy) ? "R" : /D/.test(xy) ? "D" : /A/.test(xy) ? "A" : "M";
      r.fichiers.push({ chemin, lettre, nouveau: xy === "??" });
    }
    return r;
  }
  const LETTRES = { N: "nouveau", M: "modifié", A: "ajouté", D: "supprimé", R: "renommé" };

  function apresEnregistrement() {
    majLignesGit();
    if (performance.now() - gitQuand > 2000) gitEtat();
  }

  // la marge : les lignes ajoutees, modifiees, et ou des lignes ont ete retirees
  async function majLignesGit() {
    const nom = fichier;
    if (!nom || !depot || !depot.depot) {
      if (lignesGit.size) { lignesGit = new Map(); dessiner(); }
      return;
    }
    const m = new Map();
    const suivi = lireStatut().fichiers.find((f) => f.chemin === nom);
    if (suivi && suivi.nouveau) {
      const n = code.value.split("\n").length;
      for (let k = 1; k <= n; k++) m.set(k, "a");
    } else {
      let r;
      try { r = await (await api("/api/git/lignes/" + enc(nom))).json(); } catch (e) { return; }
      if (nom !== fichier) return;
      if (r.ok) {
        for (const l of r.sortie.split("\n")) {
          const h = /^@@ -\d+(?:,(\d+))? \+(\d+)(?:,(\d+))? @@/.exec(l);
          if (!h) continue;
          const avant = h[1] === undefined ? 1 : +h[1], debutL = +h[2], apres = h[3] === undefined ? 1 : +h[3];
          if (apres === 0) { if (!m.has(debutL + 1)) m.set(debutL + 1, "d"); }
          else for (let k = 0; k < apres; k++) m.set(debutL + k, avant === 0 ? "a" : "m");
        }
      }
    }
    lignesGit = m;
    dessiner();
  }

  function colorierDiff(texte) {
    let dans = false;          // avant le premier diff --git : le message et le resume d'une version
    return texte.replace(/\n$/, "").split("\n").map((l) => {
      if (l.startsWith("diff --git")) dans = true;
      const c = !dans ? "" : /^(diff --git|index |--- |\+\+\+ |new file|deleted file|similarity|rename |\\ No newline)/.test(l) ? "meta"
        : l.startsWith("@@") ? "hunk" : l.startsWith("+") ? "add" : l.startsWith("-") ? "del" : "";
      return '<span class="' + c + '">' + esc(l || " ") + "</span>";
    }).join("\n");
  }

  const el = (tag, cls, texte) => {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (texte !== undefined) e.textContent = texte;
    return e;
  };

  function gitMessage(texte, erreur) {
    const s = $("git-sortie");
    s.textContent = texte;
    s.className = "git-sortie" + (erreur ? " err" : "");
    s.hidden = !texte;
  }

  async function gitPost(chemin, corps) {
    const r = await (await api(chemin, corps === undefined ? "" : corps)).json();
    return r;
  }

  // un fichier ou une version : un appui montre ou cache ses changements
  function depliable(tete, charger) {
    const boite = el("div", "git-boite");
    tete.classList.add("git-tete");
    boite.appendChild(tete);
    let corps = null;
    tete.addEventListener("click", async () => {
      if (corps) { corps.remove(); corps = null; return; }
      corps = el("div", "git-detail");
      corps.appendChild(el("div", "discret", "…"));
      boite.appendChild(corps);
      await charger(corps);
    });
    return boite;
  }

  async function afficherGit() {
    await enregistrer();
    await gitEtat();
    const c = $("git-corps");
    c.textContent = "";
    $("git-onglets").hidden = true;
    if (!depot || !depot.git) {
      c.appendChild(el("p", "", "Git n'est pas installé. Dans Termux : pkg install git"));
      return;
    }
    if (!depot.depot) {
      c.appendChild(el("p", "", "Ce dossier n'a pas encore de dépôt Git : rien n'est gardé des versions précédentes."));
      const b = el("button", "principal", "Créer un dépôt Git dans ce dossier");
      b.addEventListener("click", async () => {
        const r = await gitPost("/api/git/creer");
        if (!r.ok) { gitMessage(r.sortie, true); return; }
        afficherGit();
      });
      c.appendChild(b);
      c.appendChild(el("pre", "git-sortie", ""));
      c.lastChild.id = "git-sortie";
      c.lastChild.hidden = true;
      return;
    }
    $("git-onglets").hidden = false;
    $("git-o-chg").classList.toggle("actif", ongletGit === "chg");
    $("git-o-hist").classList.toggle("actif", ongletGit === "hist");
    const st = lireStatut();
    $("git-o-chg").textContent = "Changements" + (st.fichiers.length ? " (" + st.fichiers.length + ")" : "");
    const ligne = el("div", "git-ligne");
    ligne.textContent = "Branche " + (st.branche || "?") + (st.avance ? " · ↑ " + st.avance + " à envoyer" : "") +
      (st.retard ? " · ↓ " + st.retard + " à récupérer" : "") + (depot.distant ? " · " + depot.distant.replace(/^https:\/\//, "") : "");
    c.appendChild(ligne);
    if (ongletGit === "chg") afficherChangements(c, st);
    else await afficherHistorique(c);
    const sortieGit = el("pre", "git-sortie");
    sortieGit.id = "git-sortie";
    sortieGit.hidden = true;
    c.appendChild(sortieGit);
  }

  function afficherChangements(c, st) {
    if (!st.fichiers.length) c.appendChild(el("p", "discret", "Rien n'a changé depuis la dernière version."));
    for (const f of st.fichiers) {
      const tete = el("div");
      const lettre = el("span", "lettre " + f.lettre, f.lettre);
      lettre.title = LETTRES[f.lettre];
      tete.appendChild(lettre);
      tete.appendChild(el("span", "git-nom", f.chemin));
      tete.appendChild(el("span", "discret", LETTRES[f.lettre]));
      c.appendChild(depliable(tete, async (corps) => {
        const r = await (await api("/api/git/diff/" + enc(f.chemin) + (f.nouveau ? "?nouveau=1" : ""))).json();
        corps.textContent = "";
        const pre = el("pre", "diff");
        pre.innerHTML = colorierDiff(r.sortie || "(pas de différence de texte)");
        corps.appendChild(pre);
        if (f.lettre === "M" || f.lettre === "D") {
          const b = el("button", "danger", "Annuler mes changements de " + f.chemin);
          b.addEventListener("click", async () => {
            if (!confirm("Remettre " + f.chemin + " comme dans la dernière version ? Les changements seront perdus.")) return;
            const r2 = await gitPost("/api/git/annuler/" + enc(f.chemin));
            if (!r2.ok) { gitMessage(r2.sortie, true); return; }
            if (f.chemin === fichier) { sauve = true; await ouvrir(fichier); }
            afficherGit();
          });
          corps.appendChild(b);
        }
      }));
    }
    const msg = el("textarea", "git-message");
    msg.id = "git-message";
    msg.placeholder = "Ce qui a changé, par exemple : ajoute la fonction carre";
    msg.rows = 2;
    c.appendChild(msg);
    const b = el("button", "principal", "Enregistrer une version");
    b.id = "git-b-version";
    b.disabled = !st.fichiers.length;
    b.addEventListener("click", () => enregistrerVersion(msg.value));
    c.appendChild(b);
    const distant = el("div", "git-distant");
    if (depot.distant) {
      const envoyer = el("button", "", "↑ Envoyer");
      const recuperer = el("button", "", "↓ Récupérer");
      envoyer.addEventListener("click", () => echanger("/api/git/envoyer", "Envoi…", envoyer));
      recuperer.addEventListener("click", () => echanger("/api/git/recuperer", "Récupération…", recuperer));
      distant.append(envoyer, recuperer);
    }
    const relier = el("button", "", depot.distant ? "Changer d'adresse" : "Relier à GitHub…");
    relier.addEventListener("click", async () => {
      const url = prompt("Adresse du dépôt (https://github.com/toi/projet.git) :", depot.distant || "https://github.com/");
      if (!url) return;
      const r = await gitPost("/api/git/distant", url);
      if (!r.ok) { gitMessage(r.sortie, true); return; }
      afficherGit();
    });
    distant.appendChild(relier);
    c.appendChild(distant);
  }

  async function enregistrerVersion(message) {
    if (!message.trim()) { gitMessage("Écris d'abord ce qui a changé.", true); $("git-message").focus(); return; }
    await enregistrer();
    if (!depot.identite) {
      const nom = prompt("Ton nom, pour signer tes versions :");
      const mail = nom && prompt("Ton adresse mail :");
      if (!nom || !mail) return;
      const r = await gitPost("/api/git/identite", nom + "\n" + mail);
      if (!r.ok) { gitMessage(r.sortie, true); return; }
    }
    const r = await gitPost("/api/git/version", message);
    if (!r.ok) { gitMessage(r.sortie, true); return; }
    await afficherGit();
    gitMessage("Version enregistrée : " + message.trim().split("\n")[0]);
    majLignesGit();
  }

  async function echanger(chemin, attente, bouton) {
    await enregistrer();
    bouton.disabled = true;
    gitMessage(attente);
    let r;
    try { r = await gitPost(chemin); } finally { bouton.disabled = false; }
    let texte = r.sortie.trim() || (r.ok ? "C'est fait." : "Échec (code " + r.code + ")");
    if (!r.ok && /terminal prompts disabled|could not read Username|Authentication failed|Permission denied/.test(r.sortie))
      texte += "\n\nGitHub demande qui tu es. Dans Termux, une fois : pkg install gh puis gh auth login (ou une clé SSH).";
    if (r.ok && chemin.endsWith("recuperer") && fichier) { sauve = true; await ouvrir(fichier); }
    await afficherGit();
    gitMessage(texte, !r.ok);
  }

  // "a l'instant", "il y a 5 min", "hier a 14:02", "le 3/10/2026"
  function ilYa(secondes) {
    const d = new Date(secondes * 1000), ecart = (Date.now() - d.getTime()) / 1000;
    const heure = d.toLocaleTimeString("fr-FR", { hour: "2-digit", minute: "2-digit" });
    if (ecart < 60) return "à l'instant";
    if (ecart < 3600) return "il y a " + Math.floor(ecart / 60) + " min";
    const jour = (x) => new Date(x.getFullYear(), x.getMonth(), x.getDate()).getTime();
    const jours = Math.round((jour(new Date()) - jour(d)) / 86400000);
    if (jours === 0) return "aujourd'hui à " + heure;
    if (jours === 1) return "hier à " + heure;
    return "le " + d.toLocaleDateString("fr-FR") + " à " + heure;
  }

  async function afficherHistorique(c) {
    const r = await (await api("/api/git/historique")).json();
    const lignesH = (r.sortie || "").split("\n").filter(Boolean);
    if (!lignesH.length) { c.appendChild(el("p", "discret", "Pas encore de version : enregistre la première dans Changements.")); return; }
    for (const l of lignesH) {
      const [h, auteur, date, sujet] = l.split("\x1f");
      const quand = ilYa(+date);
      const tete = el("div");
      const texte = el("div", "git-nom");
      texte.appendChild(el("div", "", sujet || "(sans message)"));
      texte.appendChild(el("div", "discret", h + " · " + auteur + " · " + quand));
      tete.appendChild(texte);
      c.appendChild(depliable(tete, async (corps) => {
        const v = await (await api("/api/git/voir/" + h)).json();
        corps.textContent = "";
        if (fichier && v.sortie.includes(" b/" + (depot.prefixe || "") + fichier + "\n")) {
          const b = el("button", "", "Remettre " + fichier + " comme dans cette version");
          b.addEventListener("click", async () => {
            if (!confirm("Remettre " + fichier + " comme dans la version " + h + " ? Tu pourras enregistrer ce retour comme une nouvelle version.")) return;
            await enregistrer();
            const r2 = await gitPost("/api/git/restaurer/" + h + "/" + enc(fichier));
            if (!r2.ok) { gitMessage(r2.sortie, true); return; }
            sauve = true;
            await ouvrir(fichier);
            ongletGit = "chg";
            await afficherGit();
            gitMessage(fichier + " est revenu à la version " + h + ".");
          });
          corps.appendChild(b);
        }
        const pre = el("pre", "diff");
        pre.innerHTML = colorierDiff(v.sortie);
        corps.appendChild(pre);
      }));
    }
  }

  $("b-git").addEventListener("click", async () => { $("fenetre-git").hidden = false; await afficherGit(); });
  $("b-fermer-git").addEventListener("click", () => { $("fenetre-git").hidden = true; majLignesGit(); });
  $("git-o-chg").addEventListener("click", () => { ongletGit = "chg"; afficherGit(); });
  $("git-o-hist").addEventListener("click", () => { ongletGit = "hist"; afficherGit(); });

  // ---- le C ----
  $("b-c").addEventListener("click", async () => {
    if (!fichier) return;
    await enregistrer();
    $("titre-c").textContent = "Le C écrit par Clair pour " + fichier;
    $("texte-c").textContent = await (await api("/api/c/" + enc(fichier))).text();
    $("fenetre-c").hidden = false;
  });
  $("b-fermer-c").addEventListener("click", () => { $("fenetre-c").hidden = true; });

  // ---- V0.62 : l'explorateur de fichiers, avec les sous-dossiers ----
  let dossierCourant = "";

  async function listerFichiers(d) {
    const r = await api("/api/fichiers" + (d ? "?d=" + encodeURIComponent(d) : ""));
    if (!r.ok) return null;
    return r.json();
  }

  const IMAGES = /\.(png|jpe?g|gif|svg|webp)$/i;
  function icone(f) {
    if (f.dossier) return "📁";
    if (/\.cl$/.test(f.nom)) return "◆";
    if (IMAGES.test(f.nom)) return "🖼";
    return "📄";
  }
  const taille = (n) => (n < 1024 ? n + " o" : n < 1048576 ? Math.round(n / 1024) + " Ko" : (n / 1048576).toFixed(1) + " Mo");

  async function montrerFichiers(d) {
    if (d !== undefined) dossierCourant = d;
    let liste = await listerFichiers(dossierCourant);
    if (!liste) { dossierCourant = ""; liste = (await listerFichiers("")) || []; }
    // le chemin : chaque dossier se touche pour y revenir
    const chemin = $("chemin");
    chemin.textContent = "";
    const racine = document.createElement("button");
    racine.textContent = $("chemin").dataset.racine || "dossier";
    racine.addEventListener("click", () => montrerFichiers(""));
    chemin.appendChild(racine);
    let acc = "";
    for (const morceau of dossierCourant ? dossierCourant.split("/") : []) {
      acc = joindre(acc, morceau);
      const sep = document.createElement("span");
      sep.textContent = "/";
      sep.className = "discret";
      const b = document.createElement("button");
      b.textContent = morceau;
      const cible = acc;
      b.addEventListener("click", () => montrerFichiers(cible));
      chemin.append(sep, b);
    }
    const ul = $("liste");
    ul.textContent = "";
    if (dossierCourant) {
      const li = document.createElement("li");
      li.className = "fichier";
      li.innerHTML = '<span class="icone">↩</span><span class="nom">..</span>';
      li.addEventListener("click", () => montrerFichiers(dossierDe(dossierCourant)));
      ul.appendChild(li);
    }
    if (!liste.length) {
      const li = document.createElement("li");
      li.className = "discret vide";
      li.textContent = "Ce dossier est vide.";
      ul.appendChild(li);
    }
    for (const f of liste) {
      const complet = joindre(dossierCourant, f.nom);
      const li = document.createElement("li");
      li.className = "fichier" + (complet === fichier ? " actif" : "") + (f.dossier ? " est-dossier" : "");
      const ic = document.createElement("span");
      ic.className = "icone";
      ic.textContent = icone(f);
      const nom = document.createElement("span");
      nom.className = "nom";
      nom.textContent = f.nom;
      const info = document.createElement("span");
      info.className = "discret";
      info.textContent = f.dossier ? "" : taille(f.taille);
      const plus = document.createElement("button");
      plus.className = "plus icone petit";
      plus.textContent = "⋯";
      plus.setAttribute("aria-label", "Actions sur " + f.nom);
      li.append(ic, nom, info, plus);
      li.addEventListener("click", async (e) => {
        if (e.target.closest(".actions") || e.target === plus) return;
        if (f.dossier) { montrerFichiers(complet); return; }
        if (IMAGES.test(f.nom)) {
          const img = document.createElement("img");
          img.src = "/api/image/" + enc(complet);
          img.alt = f.nom;
          agrandir(img, null, f.nom);
          return;
        }
        if (await ouvrir(complet)) fermerTiroir();
      });
      plus.addEventListener("click", () => actions(li, f, complet));
      ul.appendChild(li);
    }
    $("tiroir").hidden = false;
    $("voile").hidden = false;
  }

  // ⋯ : renommer (ou deplacer : le chemin entier se change), supprimer
  function actions(li, f, complet) {
    const ouvert = li.querySelector(".actions");
    for (const a of document.querySelectorAll("#liste .actions")) a.remove();
    if (ouvert) return;
    const barre = document.createElement("div");
    barre.className = "actions";
    const renommer = document.createElement("button");
    renommer.textContent = "Renommer ou déplacer";
    const supprimer = document.createElement("button");
    supprimer.className = "danger";
    supprimer.textContent = "Supprimer";
    barre.append(renommer, supprimer);
    li.appendChild(barre);
    renommer.addEventListener("click", async () => {
      const nouveau = cheminValide(prompt("Nouveau nom (avec son dossier : meteo/" + f.nom + " pour le déplacer) :", complet), f.dossier);
      if (!nouveau || nouveau === complet) return;
      if (complet === fichier) await enregistrer();
      const r = await (await api("/api/renommer/" + enc(complet), nouveau)).json();
      if (!r.ok) { alert(r.erreur); return; }
      if (fichier && (fichier === complet || fichier.startsWith(complet + "/"))) {
        fichier = nouveau + fichier.slice(complet.length);
        $("nom-fichier").textContent = fichier;
        try { localStorage.setItem("clair-studio-fichier", fichier); } catch (e) { /* rien */ }
      }
      montrerFichiers(dossierDe(nouveau));
    });
    supprimer.addEventListener("click", async () => {
      if (!confirm("Supprimer " + complet + (f.dossier ? " (il doit être vide)" : "") + " ?")) return;
      const r = await (await api("/api/supprimer/" + enc(complet), "")).json();
      if (!r.ok) { alert(f.dossier ? "Le dossier n'est pas vide." : r.erreur); return; }
      if (complet === fichier) {
        fichier = null;
        sauve = true;
        code.value = "";
        $("nom-fichier").textContent = "…";
        montrerDiag([]);
        dessiner();
      }
      montrerFichiers();
    });
  }

  function fermerTiroir() { $("tiroir").hidden = true; $("voile").hidden = true; }
  $("b-fichiers").addEventListener("click", () => montrerFichiers(fichier ? dossierDe(fichier) : dossierCourant));
  $("voile").addEventListener("click", fermerTiroir);

  // un nom dans le dossier courant : sans . au debut d'un morceau, sans ..
  function nomValide(nom) {
    nom = (nom || "").trim();
    if (!nom) return null;
    if (!/\.[A-Za-z0-9]+$/.test(nom)) nom += ".cl";
    if (nom.startsWith(".") || nom.includes("/")) { alert("Un nom sans / et sans . au début."); return null; }
    return nom;
  }
  function cheminValide(chemin, dossier) {
    chemin = (chemin || "").trim().replace(/^\/+|\/+$/g, "");
    if (!chemin) return null;
    if (chemin.split("/").some((m) => !m || m.startsWith(".") || m.startsWith("-"))) {
      alert("Chaque morceau du chemin : sans . ni - au début.");
      return null;
    }
    if (!dossier && !/\.[A-Za-z0-9]+$/.test(chemin)) chemin += ".cl";
    return chemin;
  }

  $("b-nouveau").addEventListener("click", async () => {
    const nom = nomValide(prompt("Nom du nouveau fichier" + (dossierCourant ? " dans " + dossierCourant : "") + " :", "programme.cl"));
    if (!nom) return;
    const complet = joindre(dossierCourant, nom);
    const existants = ((await listerFichiers(dossierCourant)) || []).map((f) => f.nom);
    if (!existants.includes(nom))
      await api("/api/ecrire/" + enc(complet), nom.endsWith(".cl") ? "def main() -> int {\n    print(\"Bonjour\")\n    return 0\n}\n" : "");
    if (await ouvrir(complet)) fermerTiroir();
  });
  $("b-nouveau-dossier").addEventListener("click", async () => {
    const nom = (prompt("Nom du nouveau dossier" + (dossierCourant ? " dans " + dossierCourant : "") + " :", "") || "").trim();
    if (!nom) return;
    if (nom.startsWith(".") || nom.startsWith("-") || nom.includes("/")) { alert("Un nom sans / et sans . ni - au début."); return; }
    const r = await (await api("/api/dossier/" + enc(joindre(dossierCourant, nom)), "")).json();
    if (!r.ok) { alert(r.erreur); return; }
    montrerFichiers(joindre(dossierCourant, nom));
  });

  // ---- V0.62 : les reglages : theme et taille du texte, gardes par le serveur (et par le navigateur, pour le demarrage) ----
  let reglages = { theme: "auto", taille: 14 };
  function appliquerReglages() {
    const r = document.documentElement;
    if (reglages.theme === "auto") r.removeAttribute("data-theme");
    else r.setAttribute("data-theme", reglages.theme);
    r.style.setProperty("--taille", reglages.taille + "px");
    r.style.setProperty("--ligne", Math.round(reglages.taille * 1.5) + "px");
    const sombre = reglages.theme === "sombre" || (reglages.theme === "auto" && matchMedia("(prefers-color-scheme: dark)").matches);
    document.querySelector('meta[name="theme-color"]').setAttribute("content", sombre ? "#16181d" : "#ffffff");
    for (const b of document.querySelectorAll("#r-theme button")) b.classList.toggle("actif", b.dataset.v === reglages.theme);
    $("r-taille").textContent = reglages.taille;
    mesurer();
    dejaColories.clear();
    dessiner();
  }
  function garderReglages() {
    appliquerReglages();
    const texte = JSON.stringify(reglages);
    try { localStorage.setItem("clair-studio-reglages", texte); } catch (e) { /* rien */ }
    api("/api/reglages", texte).catch(() => {});
  }
  for (const b of document.querySelectorAll("#r-theme button")) b.addEventListener("click", () => { reglages.theme = b.dataset.v; garderReglages(); });
  function changerTaille(d) {
    reglages.taille = Math.min(26, Math.max(10, reglages.taille + d));
    garderReglages();
  }
  $("r-moins").addEventListener("click", () => changerTaille(-1));
  $("r-plus").addEventListener("click", () => changerTaille(1));
  document.addEventListener("keydown", (e) => {
    if (!(e.ctrlKey || e.metaKey)) return;
    if (e.key === "+" || e.key === "=") { e.preventDefault(); changerTaille(1); }
    if (e.key === "-") { e.preventDefault(); changerTaille(-1); }
  });
  matchMedia("(prefers-color-scheme: dark)").addEventListener("change", appliquerReglages);
  async function chargerReglages() {
    try { Object.assign(reglages, JSON.parse(localStorage.getItem("clair-studio-reglages") || "{}")); } catch (e) { /* rien */ }
    appliquerReglages();
    try {
      const r = await (await api("/api/reglages")).json();
      if (r && (r.theme || r.taille)) { Object.assign(reglages, r); appliquerReglages(); }
    } catch (e) { /* rien */ }
    if (!["auto", "clair", "sombre"].includes(reglages.theme)) reglages.theme = "auto";
    if (!(reglages.taille >= 10 && reglages.taille <= 26)) reglages.taille = 14;
  }

  window.addEventListener("beforeunload", () => { if (!sauve && fichier) navigator.sendBeacon("/api/ecrire/" + enc(fichier), code.value); });
  document.addEventListener("visibilitychange", () => { if (document.hidden) enregistrer(); });
  code.addEventListener("blur", () => setTimeout(() => { if (document.activeElement !== code) cacherAide(); }, 150));

  // ---- au debut : le dernier fichier ouvert, sinon le premier .cl ----
  (async function () {
    $("chemin").dataset.racine = nomDe(($("chemin").dataset.dossier || "").replace(/\/+$/, "")) || "dossier";
    await chargerReglages();
    gitEtat();
    let dernier = null;
    try { dernier = localStorage.getItem("clair-studio-fichier"); } catch (e) { /* rien */ }
    if (dernier && (await ouvrir(dernier))) return;
    const liste = (await listerFichiers("")) || [];
    const premier = liste.find((f) => !f.dossier && f.nom.endsWith(".cl")) || liste.find((f) => !f.dossier);
    if (premier) await ouvrir(premier.nom);
    else montrerFichiers("");
  })();
})();
