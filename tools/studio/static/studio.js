// Clair Studio (V0.58) : l'editeur. Le serveur est tools/studio/studio.cl.
"use strict";
(function () {
  const $ = (id) => document.getElementById(id);
  const code = $("code"), couleurs = $("couleurs"), numeros = $("numeros"), sortie = $("sortie");
  let fichier = null;            // le nom du fichier ouvert
  let sauve = true, minuterie = 0;
  let enCours = null;            // le numero du programme qui tourne
  let lignesErr = new Set();
  let debut = 0;

  // ---- les couleurs du code ----
  const MOTS = new Set(["def", "let", "return", "if", "else", "while", "for", "in", "Class", "import", "from", "as",
    "and", "or", "not", "true", "false", "break", "continue", "print", "absolue", "range"]);
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

  function dessiner() {
    const lignes = code.value.split("\n");
    let html = "", nums = "";
    for (let k = 0; k < lignes.length; k++) {
      const err = lignesErr.has(k + 1);
      html += '<span class="l' + (err ? " err" : "") + '">' + (colorier(lignes[k]) || " ") + "</span>";
      nums += err ? '<span class="err">' + (k + 1) + "</span>\n" : (k + 1) + "\n";
    }
    couleurs.innerHTML = html + '<span class="l"> </span>';
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
    if (e.key === "Tab") { e.preventDefault(); inserer("    "); }
    if ((e.ctrlKey || e.metaKey) && e.key === "s") { e.preventDefault(); enregistrer(); }
    if ((e.ctrlKey || e.metaKey) && e.key === "Enter") { e.preventDefault(); lancer(); }
  });
  code.addEventListener("input", modifie);
  code.addEventListener("scroll", suivreDefilement);

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
    $("etat-sauve").textContent = "modifié";
    clearTimeout(minuterie);
    minuterie = setTimeout(enregistrer, 800);
    redessiner();
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
    await api("/api/ecrire/" + encodeURIComponent(fichier), texte);
    if (code.value === texte) { sauve = true; $("etat-sauve").textContent = "enregistré"; }
  }

  async function ouvrir(nom) {
    await enregistrer();
    const r = await api("/api/lire/" + encodeURIComponent(nom));
    if (!r.ok) return false;
    fichier = nom;
    code.value = await r.text();
    sauve = true;
    lignesErr = new Set();
    $("nom-fichier").textContent = nom;
    $("etat-sauve").textContent = "";
    document.title = nom + " · Clair Studio";
    try { localStorage.setItem("clair-studio-fichier", nom); } catch (e) { /* rien */ }
    code.scrollTop = 0;
    dessiner();
    return true;
  }

  async function listerFichiers() {
    const r = await api("/api/fichiers");
    return r.json();
  }

  // ---- lancer ----
  function ecrireSortie(texte, classe) {
    const bas = sortie.scrollTop + sortie.clientHeight >= sortie.scrollHeight - 4;
    const morceaux = texte.split(/(\(line \d+\)|line \d+\)|error at \d+:\d+)/);
    for (const m of morceaux) {
      if (!m) continue;
      const ligne = /line (\d+)\)/.exec(m) || /error at (\d+):/.exec(m);
      if (ligne) {
        const a = document.createElement("a");
        a.textContent = m;
        a.dataset.ligne = ligne[1];
        sortie.appendChild(a);
        lignesErr.add(+ligne[1]);
      } else {
        const s = document.createElement("span");
        if (classe) s.className = classe;
        else if (/error/.test(m)) s.className = "err";
        s.textContent = m;
        sortie.appendChild(s);
      }
    }
    if (bas) sortie.scrollTop = sortie.scrollHeight;
  }

  sortie.addEventListener("click", (e) => {
    const n = e.target.dataset && e.target.dataset.ligne;
    if (n) allerA(+n);
  });

  function allerA(n) {
    const lignes = code.value.split("\n");
    let pos = 0;
    for (let k = 0; k < n - 1 && k < lignes.length; k++) pos += lignes[k].length + 1;
    code.focus();
    code.setSelectionRange(pos, pos + (lignes[n - 1] || "").length);
    const lh = parseFloat(getComputedStyle(code).lineHeight) || 21;
    code.scrollTop = Math.max(0, (n - 3) * lh);
    suivreDefilement();
  }

  function etat(texte) { $("etat").textContent = texte; }

  async function lancer() {
    if (!fichier || enCours) return;
    await enregistrer();
    sortie.textContent = "";
    lignesErr = new Set();
    $("console").hidden = false;
    etat("Compilation…");
    $("b-lancer").disabled = true;
    let r;
    try {
      r = await (await api("/api/lancer/" + encodeURIComponent(fichier), "")).json();
    } catch (e) {
      etat("Le serveur ne répond pas");
      $("b-lancer").disabled = false;
      return;
    }
    if (!r.ok) {
      ecrireSortie(r.erreurs, "err");
      etat("Erreur à la compilation (" + r.duree + " s)");
      $("b-lancer").disabled = false;
      dessiner();
      if (lignesErr.size) allerA(Math.min(...lignesErr));
      return;
    }
    enCours = r.id;
    debut = performance.now();
    etat("En cours… (compilé en " + r.duree + " s)");
    $("b-arreter").hidden = false;
    $("f-entree").hidden = false;
    suivre();
  }

  async function suivre() {
    if (!enCours) return;
    let r;
    try {
      r = await (await api("/api/suivre/" + enCours)).json();
    } catch (e) {
      setTimeout(suivre, 500);
      return;
    }
    if (r.sortie) ecrireSortie(r.sortie);
    if (r.fini) {
      const duree = ((performance.now() - debut) / 1000).toFixed(2);
      etat((r.code === 0 ? "Terminé" : r.code >= 128 ? "Arrêté" : "Terminé avec le code " + r.code) + " en " + duree + " s");
      enCours = null;
      $("b-arreter").hidden = true;
      $("f-entree").hidden = true;
      $("b-lancer").disabled = false;
      if (lignesErr.size) dessiner();
      return;
    }
    setTimeout(suivre, 150);
  }

  $("b-lancer").addEventListener("click", lancer);
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

  // ---- le C ----
  $("b-c").addEventListener("click", async () => {
    if (!fichier) return;
    await enregistrer();
    $("titre-c").textContent = "Le C écrit par Clair pour " + fichier;
    $("texte-c").textContent = await (await api("/api/c/" + encodeURIComponent(fichier))).text();
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
    await api("/api/ecrire/" + encodeURIComponent(nom), nom.endsWith(".cl") ? "def main() -> int {\n    print(\"Bonjour\")\n    return 0\n}\n" : "");
    await ouvrir(nom);
    fermerTiroir();
  });
  $("b-renommer").addEventListener("click", async () => {
    if (!fichier) return;
    const nom = nomValide(prompt("Nouveau nom de " + fichier + " :", fichier));
    if (!nom || nom === fichier) return;
    await enregistrer();
    const r = await (await api("/api/renommer/" + encodeURIComponent(fichier) + "/" + encodeURIComponent(nom), "")).json();
    if (!r.ok) { alert(r.erreur); return; }
    await ouvrir(nom);
    montrerFichiers();
  });
  $("b-supprimer").addEventListener("click", async () => {
    if (!fichier || !confirm("Supprimer " + fichier + " ?")) return;
    const r = await (await api("/api/supprimer/" + encodeURIComponent(fichier), "")).json();
    if (!r.ok) { alert(r.erreur); return; }
    fichier = null;
    sauve = true;
    const reste = await listerFichiers();
    if (reste.length) await ouvrir(reste[0].nom);
    else { code.value = ""; $("nom-fichier").textContent = "…"; dessiner(); }
    montrerFichiers();
  });

  window.addEventListener("beforeunload", () => { if (!sauve && fichier) navigator.sendBeacon("/api/ecrire/" + encodeURIComponent(fichier), code.value); });
  document.addEventListener("visibilitychange", () => { if (document.hidden) enregistrer(); });

  // ---- au debut : le dernier fichier ouvert, sinon le premier .cl ----
  (async function () {
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
