// Clair Studio (V0.58 a V0.66) : l'editeur. Le serveur est tools/studio/studio.cl.
"use strict";
(function () {
  const $ = (id) => document.getElementById(id);
  // V0.71.6 : dans l'appli Clair Studio (Android), le presse-papiers et les barres du telephone passent par elle
  const android = window.ClairAndroid || null;
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
    "Request", "Response", "RunResult", "Process", "Window", "Color", "Event", "Image", "Simulation"]);
  const esc = (s) => s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");


  // Clair Studio Labo V2 : index lexical des noms et des portees.
  // Le compilateur Clair reste seul responsable des erreurs.
  let symbolesClair = {
    classes: new Set(),
    fonctions: new Set(),
    modules: new Set(),
    locaux: new Map()
  };

  function indexerSymbolesClair(texte) {
    const a = {
      classes: new Set(),
      fonctions: new Set(),
      modules: new Set(),
      locaux: new Map()
    };

    // Eviter un traitement supplementaire sur les fichiers enormes.
    if (texte.length > 250000) {
      symbolesClair = a;
      return;
    }

    const lignesSource = texte.split("\n");
    const pile = [[]];
    let attente = null;

    function declarer(nom, sorte, numero, colonne) {
      if (!nom) return;

      const d = {
        sorte,
        debut: numero,
        colonne,
        fin: lignesSource.length
      };

      if (!a.locaux.has(nom)) a.locaux.set(nom, []);
      a.locaux.get(nom).push(d);
      pile[pile.length - 1].push(d);
    }

    for (let numero = 0; numero < lignesSource.length; numero++) {
      // Les textes et commentaires ne creent pas de declarations.
      const ligne = lignesSource[numero]
        .replace(/"(?:\\.|[^"\\])*"/g,
          s => " ".repeat(s.length))
        .replace(/\/\/.*$/, "");

      const classe =
        /^\s*Class\s+([A-Za-z_]\w*)\b/.exec(ligne);
      const fonction =
        /^\s*def\s+([A-Za-z_]\w*)\s*\(([^)]*)\)/.exec(ligne);
      const boucle =
        /^\s*for\s+([A-Za-z_]\w*)(?:\s*,\s*([A-Za-z_]\w*))?\s+in\b/.exec(ligne);
      const imp =
        /^\s*import\s+([A-Za-z_]\w*)(?:\s+as\s+([A-Za-z_]\w*))?/.exec(ligne);
      const depuis =
        /^\s*from\s+([A-Za-z_]\w*)\s+import\b/.exec(ligne);

      if (classe) a.classes.add(classe[1]);
      if (fonction) a.fonctions.add(fonction[1]);
      if (imp) a.modules.add(imp[2] || imp[1]);
      if (depuis) a.modules.add(depuis[1]);

      let lies = attente;

      if (fonction) {
        lies = [];
        const debutParams = ligne.indexOf("(", fonction.index) + 1;

        for (const segment of fonction[2].split(",")) {
          const p = /^\s*([A-Za-z_]\w*)\s*:/.exec(segment);

          if (p) {
            lies.push({
              nom: p[1],
              sorte: "param",
              colonne: debutParams + fonction[2].indexOf(segment)
            });
          }
        }
      } else if (boucle) {
        lies = [{
          nom: boucle[1],
          sorte: "var",
          colonne: boucle.index
        }];

        if (boucle[2]) {
          lies.push({
            nom: boucle[2],
            sorte: "var",
            colonne: boucle.index
          });
        }
      }

      const evenements = [];
      const reLet = /\blet\s+([A-Za-z_]\w*)\b/g;
      let m;

      while ((m = reLet.exec(ligne))) {
        evenements.push({
          pos: m.index,
          type: "let",
          nom: m[1]
        });
      }

      for (let i = 0; i < ligne.length; i++) {
        if (ligne[i] === "{") {
          evenements.push({pos: i, type: "ouvrir"});
        } else if (ligne[i] === "}") {
          evenements.push({pos: i, type: "fermer"});
        }
      }

      evenements.sort((x, y) => x.pos - y.pos);

      let ouvert = false;

      for (const e of evenements) {
        if (e.type === "fermer") {
          if (pile.length > 1) {
            const bloc = pile.pop();
            for (const d of bloc) d.fin = numero + 1;
          }
        } else if (e.type === "ouvrir") {
          pile.push([]);

          if (!ouvert && lies) {
            for (const p of lies) {
              declarer(p.nom, p.sorte, numero, p.colonne);
            }
            ouvert = true;
            attente = null;
          }
        } else {
          declarer(e.nom, "var", numero, e.pos);
        }
      }

      if ((fonction || boucle) && !ouvert) {
        attente = lies;
      }
    }

    symbolesClair = a;
  }

  function typeSymboleClair(nom, numero, colonne) {
    const candidats = symbolesClair.locaux.get(nom);

    if (!candidats) return "";

    // La declaration la plus recente et encore visible prime.
    for (let i = candidats.length - 1; i >= 0; i--) {
      const d = candidats[i];

      if (numero >= d.debut &&
          numero < d.fin &&
          (numero > d.debut || colonne >= d.colonne)) {
        return d.sorte;
      }
    }

    return "";
  }


  // Clair Studio Labo V3 : membres des classes.
  // Analyse visuelle uniquement : Clair valide le programme.
  let couleursClassesV3 = new Map();

  function indexerClassesV3(texte) {
    const couleurs = new Map();
    if (texte.length > 250000) {
      couleursClassesV3 = couleurs;
      return;
    }

    const lignes = texte.split("\n").map(ligne =>
      ligne.replace(/"(?:\\.|[^"\\])*"/g,
        x => " ".repeat(x.length))
           .replace(/\/\/.*$/, ""));

    const classes = new Map();
    const diff = ligne =>
      (ligne.match(/\{/g) || []).length -
      (ligne.match(/\}/g) || []).length;

    const nom = /^\s*Class\s+([A-Za-z_]\w*)\b/;
    const fonction =
      /^\s*def\s+([A-Za-z_]\w*)\s*\(([^)]*)\)\s*(?:->\s*([^{]+))?/;
    const champ =
      /^\s*([A-Za-z_]\w*)\s*:\s*([A-Za-z_]\w*)/;

    let profondeur = 0;
    let classe = null;

    // Premiere passe : champs et methodes declares.
    for (const ligne of lignes) {
      let m;

      if (profondeur === 0 &&
          (m = nom.exec(ligne))) {
        classe = m[1];
        classes.set(classe, {
          champs: new Map(),
          methodes: new Map()
        });

      } else if (classe && profondeur === 1) {
        const meta = classes.get(classe);

        if ((m = fonction.exec(ligne))) {
          meta.methodes.set(
            m[1], (m[3] || "").trim()
          );
        } else if ((m = champ.exec(ligne))) {
          meta.champs.set(m[1], m[2]);
        }
      }

      profondeur = Math.max(
        0, profondeur + diff(ligne)
      );

      if (classe && profondeur === 0)
        classe = null;
    }

    const marquer = (ligne, colonne, style) => {
      if (!couleurs.has(ligne))
        couleurs.set(ligne, new Map());

      couleurs.get(ligne).set(colonne, style);
    };

    let blocs = [new Map()];
    let methode = null;

    profondeur = 0;
    classe = null;

    const typeVariable = n => {
      for (let i = blocs.length - 1; i >= 0; i--) {
        if (blocs[i].has(n))
          return blocs[i].get(n);
      }
      return "";
    };

    // Seconde passe : references aux membres.
    for (let numero = 0; numero < lignes.length; numero++) {
      const ligne = lignes[numero];
      let m;

      const cl = profondeur === 0 &&
                 nom.exec(ligne);

      if (cl) classe = cl[1];

      const def = fonction.exec(ligne);
      const declarationMethode =
        !!(classe && profondeur === 1 && def);

      if (declarationMethode) {
        methode = {
          classe,
          niveau: profondeur + 1
        };

        marquer(
          numero,
          ligne.indexOf(def[1], def.index + 3),
          "clair-methode"
        );
      }

      if (classe && profondeur === 1 &&
          !def && (m = champ.exec(ligne))) {
        if (classes.get(classe).champs.has(m[1])) {
          marquer(
            numero,
            ligne.indexOf(m[1], m.index),
            "clair-attribut"
          );
        }
      }

      // Parametres dont le type est une classe.
      const params = new Map();

      if (def) {
        const p =
          /\b([A-Za-z_]\w*)\s*:\s*([A-Za-z_]\w*)/g;

        while ((m = p.exec(def[2]))) {
          if (classes.has(m[2]))
            params.set(m[1], m[2]);
        }
      }

      // Variables explicitement typees.
      const declaration =
        /\blet\s+([A-Za-z_]\w*)\s*(?::\s*([A-Za-z_]\w*))?\s*=\s*([A-Za-z_]\w*)?/g;

      while ((m = declaration.exec(ligne))) {
        const type = classes.has(m[2]) ? m[2] :
          (!m[2] && classes.has(m[3]) ? m[3] : "");

        if (type) {
          blocs[blocs.length - 1].set(
            m[1], type
          );
        }
      }

      // self est implicite dans les methodes.
      if (methode) {
        const re = /\bself\b/g;

        while ((m = re.exec(ligne)))
          marquer(numero, m.index, "clair-self");
      }

      // Reconnaissance des chaines objet.membre.
      const chaines =
        /\b[A-Za-z_]\w*(?:\s*\.\s*[A-Za-z_]\w*)+/g;

      while ((m = chaines.exec(ligne))) {
        const elements =
          [...m[0].matchAll(/[A-Za-z_]\w*/g)];

        let type =
          elements[0][0] === "self" && methode
            ? methode.classe
            : typeVariable(elements[0][0]);

        for (let k = 1; k < elements.length; k++) {
          const meta = classes.get(type);
          if (!meta) break;

          const membre = elements[k][0];
          const position =
            m.index + elements[k].index;

          if (meta.champs.has(membre)) {
            marquer(
              numero, position, "clair-attribut"
            );
            type = meta.champs.get(membre);

          } else if (meta.methodes.has(membre)) {
            marquer(
              numero, position, "clair-methode"
            );
            type = meta.methodes.get(membre);

          } else {
            break;
          }
        }
      }

      // Les types des parametres entrent dans
      // la portee de leur fonction.
      for (const caractere of ligne) {
        if (caractere === "{") {
          blocs.push(new Map());

          if (def) {
            for (const [n, t] of params)
              blocs[blocs.length - 1].set(n, t);
          }
        } else if (
          caractere === "}" && blocs.length > 1
        ) {
          blocs.pop();
        }
      }

      profondeur = Math.max(
        0, profondeur + diff(ligne)
      );

      if (methode && profondeur < methode.niveau)
        methode = null;

      if (classe && profondeur === 0)
        classe = null;
    }

    couleursClassesV3 = couleurs;
  }

  function typeMembreV3(numero, colonne) {
    return couleursClassesV3.get(numero)?.get(colonne) || "";
  }

  function colorier(ligne, numero) {
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
        const local = typeSymboleClair(m, numero, i);
        const membreV3 = typeMembreV3(numero, i);
        let cls = MOTS.has(m) ? "k" :
          membreV3 ? membreV3 :
          local === "param" ? "clair-param" :
          local === "var" ? "clair-var" :
          (TYPES.has(m) || symbolesClair.classes.has(m)) ? "t" :
          symbolesClair.modules.has(m) ? "clair-module" :
          (symbolesClair.fonctions.has(m) || /^\s*\(/.test(apres)) ? "f" : "";
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
  function couleur(ligne, numero) {
    const cle = numero + "\u0000" + ligne;
    let h = dejaColories.get(cle);
    if (h === undefined) {
      if (dejaColories.size > 5000) dejaColories.clear();
      h = colorier(ligne, numero);
      dejaColories.set(cle, h);
    }
    return h;
  }

  let hauteur = 21, lignes = [""], plusLongue = 0, texteDessine = null;
  function mesurer() { hauteur = parseFloat(getComputedStyle(code).lineHeight) || 21; }

  function dessiner() {
    if (texteDessine !== code.value) {
      indexerSymbolesClair(code.value);
      indexerClassesV3(code.value);
      dejaColories.clear();
      texteDessine = code.value;
      lignes = texteDessine.split("\n");
      plusLongue = 0;
      for (const l of lignes) if (l.length > plusLongue) plusLongue = l.length;
      if (ch.ouvert) chercherMaj();                 // V0.71.4 : le texte a change, les endroits trouves aussi
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
      let h = couleur(lignes[k], k);
      const ids = ch.ouvert ? ch.parLigne.get(k) : null;
      if (ids) h = marquerZones(h, ids.map((i) => [ch.trouves[i].a, ch.trouves[i].b, i === ch.actuel ? "trouve actuel" : "trouve"]));
      else if (surligne && !(ch.ouvert && ch.re)) h = marquer(h, lignes[k]);
      html += '<span class="l' + (cls ? " " + cls : "") + '">' + (h || " ") + "</span>";
      const g = lignesGit.get(n);
      nums += '<span class="nl' + (cls ? " " + cls : "") + (g ? " g" + g : "") + '">' + n + "</span>";
    }
    if (total > dernier) html += '<span class="vide" style="height:' + (total - dernier) * hauteur + 'px"></span>';
    couleurs.innerHTML = html + '<span class="l"> </span>';
    numeros.style.paddingTop = 8 + premier * hauteur + "px";
    numeros.innerHTML = nums;
    suivreDefilement();
  }

  // ---- V0.71.3 : comme VS Code, le texte selectionne est surligne partout ou il revient ----
  // Un mot entier (selectionne d'un double toucher) : les memes mots entiers. Un autre morceau d'une ligne
  // (2 caracteres au moins) : partout ou il apparait. Seules les lignes visibles sont touchees.
  let surligne = null;                              // { texte, re }
  const LETTRE = /[\p{L}\p{N}_]/u;
  function majSurligne() {
    const a = code.selectionStart, b = code.selectionEnd, v = code.value;
    const t = a < b ? v.slice(a, b) : "";
    let nouveau = null;
    if (t && t.length <= 200 && !t.includes("\n") && t.trim() === t) {
      const mot = /^[\p{L}\p{N}_]+$/u.test(t) && !LETTRE.test(v[a - 1] || "") && !LETTRE.test(v[b] || "");
      if (mot || t.length >= 2) {
        const e = t.replace(/[.*+?^${}()|[\]\\\/]/g, "\\$&");
        nouveau = { texte: (mot ? "mot:" : "") + t, re: new RegExp(mot ? "(?<![\\p{L}\\p{N}_])" + e + "(?![\\p{L}\\p{N}_])" : e, "gu") };
      }
    }
    if ((nouveau ? nouveau.texte : null) === (surligne ? surligne.texte : null)) return;
    surligne = nouveau;
    redessiner();
  }

  // la ligne coloriee (html) avec <mark class="meme"> autour de chaque endroit trouve dans le texte de la ligne
  function marquer(html, ligne) {
    const re = surligne.re, zones = [];
    re.lastIndex = 0;
    for (let m; (m = re.exec(ligne)); ) zones.push([m.index, m.index + m[0].length, "meme"]);
    return zones.length ? marquerZones(html, zones) : html;
  }

  // zones : [debut, fin, classe] dans le texte de la ligne, dans l'ordre. Les balises ne comptent pas, &amp; &lt;
  // &gt; comptent pour un caractere ; une marque qui traverse une couleur est fermee avant la balise et rouverte
  // au caractere suivant (V0.71.4 : aussi pour rechercher)
  function marquerZones(html, zones) {
    let out = "", i = 0, pos = 0, z = 0, ouvert = null;
    while (i < html.length) {
      if (html[i] === "<") {
        const j = html.indexOf(">", i) + 1;
        if (ouvert) { out += "</mark>"; ouvert = null; }
        out += html.slice(i, j);
        i = j;
        continue;
      }
      while (z < zones.length && pos >= zones[z][1]) z++;
      const cls = z < zones.length && pos >= zones[z][0] ? zones[z][2] : null;
      if (ouvert && ouvert !== cls) { out += "</mark>"; ouvert = null; }
      if (cls && !ouvert) { out += '<mark class="' + cls + '">'; ouvert = cls; }
      const j = html[i] === "&" ? html.indexOf(";", i) + 1 : i + 1;
      out += html.slice(i, j);
      i = j;
      pos++;
    }
    if (ouvert) out += "</mark>";
    return out;
  }

  document.addEventListener("selectionchange", () => { if (document.activeElement === code) majSurligne(); });
  for (const ev of ["select", "keyup", "mouseup", "touchend"]) code.addEventListener(ev, majSurligne);

  // ---- V0.71.4 : rechercher et remplacer, comme VS Code ----
  // Les endroits trouves sont cherches ligne par ligne (une recherche ne passe pas d'une ligne a l'autre), et
  // surlignes dans les lignes visibles ; celui ou l'on est, plus fort. Remplacer passe par l'annulation du
  // navigateur : ↶ revient en arriere (Tout : en une fois).
  const ch = { ouvert: false, re: null, trouves: [], parLigne: new Map(), actuel: -1, erreur: "", depart: 0, MAX: 20000,
               message: "" };                       // message : « 7 remplacés », jusqu'a la prochaine recherche
  const option = (id) => $(id).getAttribute("aria-pressed") === "true";

  function chercherRegex() {
    const t = $("ch-texte").value;
    ch.erreur = "";
    if (!t) return null;
    let src = option("ch-regex") ? t : t.replace(/[.*+?^${}()|[\]\\\/]/g, "\\$&");
    if (option("ch-mot")) src = "(?<![\\p{L}\\p{N}_])(?:" + src + ")(?![\\p{L}\\p{N}_])";
    try {
      return new RegExp(src, "gu" + (option("ch-casse") ? "" : "i"));
    } catch (e) {
      ch.erreur = "invalide";
      return null;
    }
  }

  // tous les endroits trouves ; depuis : le premier a partir de cette position du texte devient l'actuel
  function chercherMaj(depuis) {
    ch.re = chercherRegex();
    ch.trouves = [];
    ch.parLigne = new Map();
    if (ch.re) {
      const v = code.value;
      let debut = 0;
      for (const [k, l] of v.split("\n").entries()) {
        ch.re.lastIndex = 0;
        for (let m; ch.trouves.length < ch.MAX && (m = ch.re.exec(l)); ) {
          if (!m[0].length) { ch.re.lastIndex++; continue; }      // un motif qui peut etre vide (a*) : rien a montrer
          if (!ch.parLigne.has(k)) ch.parLigne.set(k, []);
          ch.parLigne.get(k).push(ch.trouves.length);
          ch.trouves.push({ ligne: k, a: m.index, b: m.index + m[0].length, debut: debut + m.index, m });
        }
        debut += l.length + 1;
      }
    }
    if (depuis !== undefined) {
      ch.actuel = ch.trouves.findIndex((t) => t.debut >= depuis);
      if (ch.actuel < 0) ch.actuel = ch.trouves.length ? 0 : -1;
    } else if (ch.actuel >= ch.trouves.length || (ch.actuel < 0 && ch.trouves.length)) ch.actuel = ch.trouves.length ? 0 : -1;
    compteRecherche();
    redessiner();
  }

  function compteRecherche() {
    const n = ch.trouves.length;
    $("ch-texte").classList.toggle("invalide", !!ch.erreur);
    if (ch.message && code.value !== ch.messageTexte) ch.message = "";   // le texte a change depuis (↶ par exemple)
    $("ch-compte").textContent = ch.erreur ? "invalide" : ch.message ? ch.message : !$("ch-texte").value ? "" : !n ? "aucun"
      : (ch.actuel + 1) + " / " + n + (n >= ch.MAX ? "+" : "");
  }

  function largeurCaractere() {
    const c = largeurCaractere.c || (largeurCaractere.c = document.createElement("canvas").getContext("2d"));
    c.font = getComputedStyle(code).font;
    return c.measureText("0000000000").width / 10 || 8;
  }

  // l'endroit actuel, dans la vue (en hauteur, et en largeur pour une longue ligne)
  function montrerActuel() {
    const t = ch.trouves[ch.actuel];
    if (!t) return;
    const y = 8 + t.ligne * hauteur;
    if (y < code.scrollTop || y + hauteur > code.scrollTop + code.clientHeight) code.scrollTop = Math.max(0, y - code.clientHeight / 3);
    const cw = largeurCaractere(), x = 10 + t.a * cw, x2 = 10 + t.b * cw;
    if (x < code.scrollLeft + 10 || x2 > code.scrollLeft + code.clientWidth - 10) code.scrollLeft = Math.max(0, x - code.clientWidth / 3);
    suivreDefilement();
    compteRecherche();
    redessiner();
  }

  function allerRecherche(sens) {
    ch.message = "";
    const n = ch.trouves.length;
    if (!n) return;
    ch.actuel = ((ch.actuel < 0 ? (sens > 0 ? -1 : 0) : ch.actuel) + sens + n) % n;
    montrerActuel();
  }

  function ouvrirRecherche(remplacer) {
    const a = code.selectionStart, b = code.selectionEnd, sel = code.value.slice(a, b);
    if (sel && !sel.includes("\n") && sel.length <= 200) $("ch-texte").value = sel;
    $("chercher").hidden = false;
    ch.ouvert = true;
    ch.message = "";
    ch.depart = Math.min(a, b);
    montrerRemplacer(remplacer || !$("ch-remplacement").hidden);
    chercherMaj(ch.depart);
    const champ = remplacer && $("ch-texte").value ? $("ch-par") : $("ch-texte");
    champ.focus();
    champ.select();
    if (ch.actuel >= 0) montrerActuel();
  }

  function fermerRecherche() {
    const t = ch.trouves[ch.actuel];
    $("chercher").hidden = true;
    ch.ouvert = false;
    ch.re = null;
    ch.trouves = [];
    ch.parLigne = new Map();
    code.focus();
    if (t) code.setSelectionRange(t.debut, t.debut + t.b - t.a);   // l'editeur reprend la ou l'on etait
    redessiner();
  }

  function montrerRemplacer(oui) {
    $("ch-remplacement").hidden = !oui;
    $("ch-bascule").setAttribute("aria-pressed", oui ? "true" : "false");
  }

  // le texte de remplacement d'un endroit : avec .* , $& (tout), $1 a $99 (les groupes), $$ (un $)
  function remplacement(t) {
    const par = $("ch-par").value;
    if (!option("ch-regex")) return par;
    return par.replace(/\$(\$|&|\d{1,2})/g, (x, k) => k === "$" ? "$" : k === "&" ? t.m[0]
      : +k > 0 && +k < t.m.length ? (t.m[+k] === undefined ? "" : t.m[+k]) : x);
  }

  // remplace le texte de debut a fin par nouveau, en passant par l'annulation du navigateur
  function ecrireA(debut, fin, nouveau) {
    const retour = document.activeElement, haut = code.scrollTop, gauche = code.scrollLeft;
    code.focus();
    code.setSelectionRange(debut, fin);
    const ok = nouveau ? document.execCommand("insertText", false, nouveau) : document.execCommand("delete");
    if (!ok) code.setRangeText(nouveau, debut, fin, "end");
    modifie();
    code.scrollTop = haut;
    code.scrollLeft = gauche;
    if (retour && retour !== code && retour.focus) retour.focus();
  }

  function remplacerUn() {
    ch.message = "";
    const t = ch.trouves[ch.actuel];
    if (!t) return;
    const nouveau = remplacement(t);
    ecrireA(t.debut, t.debut + t.b - t.a, nouveau);
    chercherMaj(t.debut + nouveau.length);           // puis le suivant
    montrerActuel();
  }

  function remplacerTout() {
    const n = ch.trouves.length;
    if (!n) return;
    const v = code.value;
    let out = "", pos = 0;
    for (const t of ch.trouves) {
      out += v.slice(pos, t.debut) + remplacement(t);
      pos = t.debut + t.b - t.a;
    }
    ecrireA(0, v.length, out + v.slice(pos));
    ch.message = n + " remplacé" + (n > 1 ? "s" : "");
    ch.messageTexte = code.value;
    chercherMaj(0);
  }

  $("ch-texte").addEventListener("input", () => { ch.message = ""; chercherMaj(ch.depart); montrerActuel(); });
  $("ch-texte").addEventListener("keydown", (e) => {
    if (e.key === "Enter") { e.preventDefault(); allerRecherche(e.shiftKey ? -1 : 1); }
    if (e.key === "Escape") { e.preventDefault(); fermerRecherche(); }
  });
  $("ch-par").addEventListener("keydown", (e) => {
    if (e.key === "Enter") { e.preventDefault(); remplacerUn(); }
    if (e.key === "Escape") { e.preventDefault(); fermerRecherche(); }
  });
  for (const id of ["ch-casse", "ch-mot", "ch-regex"]) {
    $(id).addEventListener("click", () => {
      $(id).setAttribute("aria-pressed", option(id) ? "false" : "true");
      ch.message = "";
      chercherMaj(ch.depart);
      montrerActuel();
    });
  }
  $("ch-bascule").addEventListener("click", () => {
    montrerRemplacer($("ch-remplacement").hidden);
    if (!$("ch-remplacement").hidden) $("ch-par").focus();
  });
  for (const b of document.querySelectorAll("#chercher button")) b.addEventListener("pointerdown", (e) => e.preventDefault());
  $("ch-suiv").addEventListener("click", () => allerRecherche(1));
  $("ch-prec").addEventListener("click", () => allerRecherche(-1));
  $("ch-fermer").addEventListener("click", fermerRecherche);
  $("ch-un").addEventListener("click", remplacerUn);
  $("ch-tous").addEventListener("click", remplacerTout);
  document.addEventListener("keydown", (e) => {
    if ((e.ctrlKey || e.metaKey) && (e.key === "f" || e.key === "h") && document.activeElement !== code) {
      e.preventDefault();
      ouvrirRecherche(e.key === "h");
    }
  });

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
    if ((e.ctrlKey || e.metaKey) && (e.key === "f" || e.key === "h")) { e.preventDefault(); ouvrirRecherche(e.key === "h"); }
    if ((e.ctrlKey || e.metaKey) && e.key === "Enter") { e.preventDefault(); lancer(); }
  });
  code.addEventListener("input", modifie);
  code.addEventListener("scroll", () => { suivreDefilement(); redessiner(); });
  for (const ev of ["click", "keyup", "select"]) code.addEventListener(ev, (e) => { if (e.key !== "Escape") planifierAide(); });
  window.addEventListener("resize", () => { mesurer(); redessiner(); });
  if (window.ResizeObserver) new ResizeObserver(() => codeRedimensionne()).observe(code);   // la console s'ouvre, le clavier aussi


  // V0.65.1 : selection et presse-papiers.
  let copieInterne = "";

  function toutSelectionner() {
    code.focus();
    code.setSelectionRange(0, code.value.length);
    cacherAide();
  }

  function copierSelection() {
    const a = code.selectionStart;
    const z = code.selectionEnd;

    if (a === z) {
      alert("Selectionne du code ou touche Tout avant de copier.");
      return;
    }

    const texte = code.value.slice(a, z);
    copieInterne = texte;
    if (android) {
      try { if (android.copier(texte)) return; } catch (e) { /* plus bas */ }
    }

    const copieClassique = () => {
      code.focus();
      code.setSelectionRange(a, z);
      try {
        return document.execCommand("copy");
      } catch (e) {
        return false;
      }
    };

    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(texte).catch(() => {
        if (!copieClassique()) {
          alert("Copie disponible dans Clair Studio, mais pas dans le presse-papiers Android.");
        }
      });
    } else if (!copieClassique()) {
      alert("Copie disponible dans Clair Studio, mais pas dans le presse-papiers Android.");
    }
  }

  async function collerSelection() {
    const a = code.selectionStart;
    const z = code.selectionEnd;
    let texte = null;

    if (android) {
      try { texte = android.coller() || null; } catch (e) { texte = null; }
    } else if (navigator.clipboard && navigator.clipboard.readText) {
      try {
        texte = await navigator.clipboard.readText();
      } catch (e) {
        // Repli vers le texte copie dans Clair Studio.
      }
    }

    if (!texte && copieInterne) {
      texte = copieInterne;
    }

    if (!texte) {
      code.focus();
      alert("Pour coller depuis Android, maintiens le doigt sur le code et choisis Coller.");
      return;
    }

    code.focus();
    code.setSelectionRange(a, z);
    inserer(texte);
  }

  for (const b of document.querySelectorAll("#touches button")) {
    b.addEventListener("pointerdown", (e) => e.preventDefault());      // le clavier reste ouvert
    b.addEventListener("click", () => {
      if (b.dataset.outil === "tout") { toutSelectionner(); return; }
      if (b.dataset.outil === "copier") { copierSelection(); return; }
      if (b.dataset.outil === "coller") { collerSelection(); return; }
      if (b.dataset.outil === "chercher") { ouvrirRecherche(false); return; }
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
    const dansCode = champ === code || !!(champ && champ.closest && champ.closest("#chercher"));   // V0.71.4
    const clavier = (dansCode || champ === $("entree")) && visible < hauteurPleine * 0.8;
    app.classList.toggle("clavier", clavier);
    app.classList.toggle("dans-code", clavier && dansCode);
    app.classList.toggle("dans-entree", clavier && !dansCode);
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
    if (r.status === 403) {
      if (android) android.reconnecter();         // V0.71.6 : l'appli redemande la cle du Studio
      else alert("Ouvre l'adresse affichée dans Termux (avec ?t=...).");
      throw new Error("403");
    }
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
  // V0.71.2 : la console montre les 5 000 dernieres lignes. Avant, chaque ligne etait un element de la page :
  // 100 000 lignes prenaient 17 s et alourdissaient tout. Copier garde toute la sortie.
  const LIGNES_MAX = 5000;
  let lignesConsole = 0, lignesCachees = 0, noteCachees = null;
  function viderConsole() {
    sortie.textContent = "";
    lignesConsole = 0;
    lignesCachees = 0;
    noteCachees = null;
  }

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
    if (cible === sortie) {                          // une figure compte pour une ligne (sa marque finit par \n)
      for (let i = texte.indexOf("\n"); i >= 0; i = texte.indexOf("\n", i + 1)) lignesConsole++;
      if (lignesConsole > LIGNES_MAX + 1000) couperConsole();
    }
    if (bas) cible.scrollTop = cible.scrollHeight;
  }

  // retire les plus vieilles lignes de la console, jusqu'a LIGNES_MAX
  function couperConsole() {
    let trop = lignesConsole - LIGNES_MAX;
    let n = noteCachees ? noteCachees.nextSibling : sortie.firstChild;
    while (trop > 0 && n) {
      const suivant = n.nextSibling;
      const t = n.textContent;
      let k = 0;
      if (n.nodeType === 1 && n.classList.contains("figure")) k = 1;
      else for (let i = t.indexOf("\n"); i >= 0; i = t.indexOf("\n", i + 1)) k++;
      if (k <= trop) {
        n.remove();
        trop -= k;
        lignesCachees += k;
        lignesConsole -= k;
      } else if (n.nodeType === 3) {               // le debut d'un texte de plusieurs lignes
        let i = -1;
        for (let j = 0; j < trop; j++) i = t.indexOf("\n", i + 1);
        n.data = t.slice(i + 1);
        lignesCachees += trop;
        lignesConsole -= trop;
        trop = 0;
      } else break;
      n = suivant;
    }
    if (!noteCachees) {
      noteCachees = document.createElement("div");
      noteCachees.className = "info";
      sortie.insertBefore(noteCachees, sortie.firstChild);
    }
    noteCachees.textContent = "… " + lignesCachees.toLocaleString("fr-FR") + " lignes plus haut ne sont plus affichées (Copier garde toute la sortie)";
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
      } else if (classe || !m.includes("error")) {   // V0.71.2 : tout le morceau d'un coup
        const s = classe ? document.createElement("span") : document.createTextNode(m);
        if (classe) { s.className = classe; s.textContent = m; }
        cible.appendChild(s);
      } else {
        let paquet = "";                              // seules les lignes d'une erreur en rouge
        for (const morceau of m.split(/(?<=\n)/)) {
          if (/error/.test(morceau)) {
            if (paquet) cible.appendChild(document.createTextNode(paquet));
            paquet = "";
            const s = document.createElement("span");
            s.className = "err";
            s.textContent = morceau;
            cible.appendChild(s);
          } else paquet += morceau;
        }
        if (paquet) cible.appendChild(document.createTextNode(paquet));
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
    $("b-sauver-csv").disabled = tourne;
  }

  // compile (le serveur ne recompile pas un programme qui n'a pas change), puis suit le programme
  // V0.65.2 : conserver la sortie brute.
  let sortieBrute = "";
  let csvDetecte = false;
  let longueurApercuCsv = 0;
  let apercuCoupe = false;

  async function demarrer(chemin, quoi) {
    if (!fichier || enCours) return;
    await enregistrer();
    viderConsole();
    sortieBrute = "";
    csvDetecte = false;
    longueurApercuCsv = 0;
    apercuCoupe = false;
    $("b-sauver-csv").hidden = true;
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
      if (enTests) {
        texteTests += texte;
        ecrireSortie(texte);
      } else {
        sortieBrute += texte;

        if (!csvDetecte &&
            /^time,[^\\r\\n]+/m.test(
              sortieBrute.slice(0, 4096)
            )) {
          csvDetecte = true;
          $("b-sauver-csv").hidden = false;
        }

        if (csvDetecte) {
          const restant = Math.max(
            0, 12000 - longueurApercuCsv
          );

          if (restant) {
            ecrireSortie(texte.slice(0, restant));
          }

          longueurApercuCsv += texte.length;

          if (texte.length > restant && !apercuCoupe) {
            apercuCoupe = true;
            ecrireSortie(
              "\\n[Apercu limite : la sortie complete est conservee pour le CSV]\\n",
              "info"
            );
          }
        } else {
          longueurApercuCsv += texte.length;
          ecrireSortie(texte);
        }
      }
    }
    if (r.fini) {
      const duree = ((performance.now() - debut) / 1000).toFixed(2);
      enCours = null;
      if (enTests) afficherTests(texteTests, r.code, duree);
      else etat((r.code === 0 ? "Terminé" : r.code >= 128 ? "Arrêté" : "Terminé avec le code " + r.code) + " en " + duree + " s");
      enTests = false;
      boutons(false);
      $("b-sauver-csv").disabled =
        !csvDetecte || r.code !== 0;
      if (lignesErr.size) dessiner();
      return;
    }
    setTimeout(suivre, texte ? 20 : 150);              // V0.71.2 : quand le programme ecrit, revenir vite
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
    viderConsole();
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

  // V0.65.2 : enregistrer directement un CSV.
  $("b-sauver-csv").addEventListener("click", () => {
    if (enCours || !csvDetecte || !sortieBrute) return;

    definirModeDossier("csv");
    $("fenetre-dossiers").hidden = false;

    const racine =
      ($("chemin").dataset.dossier || "").replace(/\/+$/, "");

    const sous = dossierDe(fichierLance || fichier);

    afficherDossiersChoix(
      racine + (sous ? "/" + sous : "")
    );
  });

  // V0.64 : tout ce que le programme a ecrit (sans les figures), dans le presse-papiers
  $("b-copier").addEventListener("click", async () => {
    let texte;
    if (lignesCachees && sortieBrute) {            // V0.71.2 : des lignes ne sont plus affichees : la sortie entiere
      texte = (($("etat").textContent || "") + "\n" + sortieBrute.replace(new RegExp(MARQUE.source, "g"), "[figure]\n")).trim() + "\n";
    } else {
      const copie = sortie.cloneNode(true);
      for (const f of copie.querySelectorAll(".figure")) f.replaceWith("[figure]\n");
      for (const t of copie.querySelectorAll(".test")) t.append("\n");
      texte = (($("etat").textContent || "") + "\n" + copie.textContent).trim() + "\n";
    }
    let ok = false;
    if (android) {
      try { ok = !!android.copier(texte); } catch (e) { ok = false; }
    }
    if (!ok) {
      try { await navigator.clipboard.writeText(texte); ok = true; } catch (e) { /* plus bas */ }
    }
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
  // V0.71.5 : les modules de Clair, proposes apres import et from (puis les fichiers .cl du dossier)
  const MODULES_CLAIR = [["math", "sqrt, sin, pi…"], ["phys", "constantes physiques"], ["astro", "constantes du ciel"],
    ["os", "fichiers, programmes"], ["web", "site web"], ["plot", "graphiques"], ["gui", "fenêtre, dessin"],
    ["nbody", "gravité, N corps"]];
  // leurs constantes (dans le compilateur, src/modules.c) : nom et unite
  const PHYS = [["c", "m/s"], ["h", "J*s"], ["hbar", "J*s"], ["e", "C"], ["N_A", "1/mol"], ["k_B", "J/K"], ["R", "J/(mol*K)"],
    ["F", "C/mol"], ["sigma", "W/(m^2*K^4)"], ["b_wien", "m*K"], ["g0", "m/s^2"], ["atm", "Pa"], ["eV", "J"],
    ["G", "m^3/(kg*s^2)"], ["m_e", "kg"], ["m_p", "kg"], ["m_n", "kg"], ["m_u", "kg"], ["eps0", "F/m"], ["mu0", "N/A^2"],
    ["alpha", "1"], ["R_inf", "1/m"], ["a0", "m"], ["l_P", "m"], ["m_P", "kg"], ["t_P", "s"], ["T_P", "K"]];
  const ASTRO = [["UA", "m"], ["AU", "m"], ["pc", "m"], ["al", "m"], ["ly", "m"], ["jour", "s"], ["an", "s"],
    ["GM_soleil", "m^3/s^2"], ["GM_mercure", "m^3/s^2"], ["GM_venus", "m^3/s^2"], ["GM_terre", "m^3/s^2"],
    ["GM_lune", "m^3/s^2"], ["GM_mars", "m^3/s^2"], ["GM_jupiter", "m^3/s^2"], ["GM_saturne", "m^3/s^2"],
    ["GM_uranus", "m^3/s^2"], ["GM_neptune", "m^3/s^2"], ["GM_pluton", "m^3/s^2"], ["R_soleil", "m"], ["L_soleil", "W"],
    ["R_terre", "m"], ["R_terre_polaire", "m"], ["R_jupiter", "m"], ["R_jupiter_polaire", "m"]];

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
        for (const f of ["fmod", "copysign"]) a.fonctions.set(f, { nom: f, params: ["x", "y"], ret: "float", doc: "", sorte: "f" });
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
    if (nom === "phys" || nom === "astro") {        // V0.71.5 : des constantes avec leur unite
      if (!modules.has(nom)) {
        const a = analyser("", true);
        for (const [c, u] of nom === "phys" ? PHYS : ASTRO) a.constantes.set(c, "float[" + u + "]");
        modules.set(nom, { analyse: a, fixe: true });
      }
      return modules.get(nom).analyse;
    }
    const m = modules.get(nom);
    if (!m || (!m.fixe && !m.enCours && performance.now() - m.quand > 5000)) chargerModule(nom);
    return m ? m.analyse : null;
  }
  async function chargerModule(nom) {
    const integre = ["os", "web", "plot", "gui", "nbody"].includes(nom);
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

  // V0.71.5 : les fichiers .cl du dossier du fichier ouvert, qu'on peut importer (lus au plus toutes les 5 s)
  let modulesLocaux = { dossier: null, noms: [], quand: 0, enCours: false };
  function modulesDuDossier() {
    const d = dossierDe(fichier);
    if (!modulesLocaux.enCours && (modulesLocaux.dossier !== d || performance.now() - modulesLocaux.quand > 5000)) {
      modulesLocaux.enCours = true;
      listerFichiers(d).then((liste) => {
        modulesLocaux = { dossier: d, quand: performance.now(), enCours: false,
          noms: (liste || []).filter((f) => !f.dossier && /^[A-Za-z_]\w*\.cl$/.test(f.nom) && joindre(d, f.nom) !== fichier)
            .map((f) => f.nom.slice(0, -3)) };
        planifierAide();
      }, () => { modulesLocaux.enCours = false; });
    }
    return modulesLocaux.dossier === d ? modulesLocaux.noms : [];
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
    // V0.71.5 : import nb… (les modules), from nbody import Sim… (ce que le module contient)
    let m = /^\s*(?:import|from)\s+([A-Za-z_]\w*)?$/.exec(vue);
    if (m) return { importer: true, prefixe: m[1] || "", debut: pos - (m[1] || "").length, vue, pos };
    m = /^\s*from\s+([A-Za-z_]\w*)\s+import\s+(?:[A-Za-z_]\w*(?:\s+as\s+[A-Za-z_]\w*)?\s*,\s*)*([A-Za-z_]\w*)?$/.exec(vue);
    if (m) return { depuis: m[1], prefixe: m[2] || "", debut: pos - (m[2] || "").length, vue, pos };
    m = /([A-Za-z_]\w*)\.([A-Za-z_]\w*)?$/.exec(vue);
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
    if (ctx.importer) {
      for (const [n, d] of MODULES_CLAIR) ajouter(n, "m", d, null);
      for (const n of modulesDuDossier()) ajouter(n, "m", n + ".cl", null);
    } else if (ctx.depuis) {
      const am = module(ctx.depuis);
      if (am) {
        for (const f of am.fonctions.values()) ajouter(f.nom, "f", "", f);
        for (const c of am.classes.values()) ajouter(c.nom, "c", "", fonctionsClasse(c));
        for (const [n, t] of am.constantes) ajouter(n, "v", t, null);
      }
    } else if (ctx.objet) {
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
    // V0.71.5 : parmi ceux qui commencent pareil, d'abord ceux qui ont aussi les memes majuscules (Sim -> Simulation)
    const exacts = debuts.filter((c) => c.nom.startsWith(ctx.prefixe)), autres = debuts.filter((c) => !c.nom.startsWith(ctx.prefixe));
    return exacts.concat(autres, dedans).slice(0, 40);
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
    const fonction = !ctx.depuis && (p.sorte === "f" || p.sorte === "c") && code.value[ctx.pos] !== "(";   // from x import f : sans ()
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

  // V0.65.1 : nouveau document dans le dossier actuel.
  $("b-nouveau-rapide").addEventListener("click", async () => {
    if (enCours || enTests) {
      alert("Arrete le programme ou les tests d'abord.");
      return;
    }

    const saisie = prompt(
      "Nom du nouveau document Clair :", "nouveau.cl"
    );
    if (saisie === null) return;

    const nom = nomValide(saisie);
    if (!nom || !nom.endsWith(".cl") ||
        nom.startsWith("-") || nom.includes("\\")) {
      alert("Choisis un nom de fichier .cl valide.");
      return;
    }

    const parent = fichier ? dossierDe(fichier) : dossierCourant;
    const complet = joindre(parent, nom);
    const bouton = $("b-nouveau-rapide");
    bouton.disabled = true;

    try {
      await enregistrer();
      if (!sauve) {
        throw new Error("Enregistrement precedent inacheve.");
      }

      const reponse = await api(
        "/api/nouveau/" + enc(complet), ""
      );
      if (!reponse.ok) {
        throw new Error("Impossible de creer le document.");
      }

      const resultat = await reponse.json();
      if (!resultat.ok) {
        throw new Error(resultat.erreur || "Document non cree.");
      }

      if (await ouvrir(complet)) {
        fermerTiroir();
        code.focus();
      }
    } catch (e) {
      alert(String(e.message || e));
    } finally {
      bouton.disabled = false;
    }
  });

  $("b-nouveau-dossier").addEventListener("click", async () => {
    const nom = (prompt("Nom du nouveau dossier" + (dossierCourant ? " dans " + dossierCourant : "") + " :", "") || "").trim();
    if (!nom) return;
    if (nom.startsWith(".") || nom.startsWith("-") || nom.includes("/")) { alert("Un nom sans / et sans . ni - au début."); return; }
    const r = await (await api("/api/dossier/" + enc(joindre(dossierCourant, nom)), "")).json();
    if (!r.ok) { alert(r.erreur); return; }
    montrerFichiers(joindre(dossierCourant, nom));
  });


  // ---- V0.65 : explorateur et selection du dossier ----
  let choixDossier = "";
  let demandeChoix = 0;
  let modeChoixDossier = "projet";

  function definirModeDossier(mode) {
    modeChoixDossier = mode;
    const csv = mode === "csv";

    $("selection-nom-csv").hidden = !csv;

    $("b-utiliser-dossier").textContent =
      csv ? "Enregistrer ici" : "Utiliser ce dossier";

    $("fenetre-dossiers")
      .querySelector(".console-tete strong")
      .textContent =
        csv ? "Enregistrer CSV sous…" : "Choisir un dossier";
  }

  function ajouterDossierChoix(nom, chemin, icone) {
    const li = document.createElement("li");
    const bouton = document.createElement("button");
    bouton.type = "button";
    bouton.textContent = icone + " " + nom;
    bouton.addEventListener("click", () => afficherDossiersChoix(chemin));
    li.appendChild(bouton);
    $("selection-liste").appendChild(li);
  }

  async function afficherDossiersChoix(chemin = "") {
    const demande = ++demandeChoix;
    const liste = $("selection-liste");
    const titre = $("selection-chemin");
    const erreur = $("selection-erreur");

    choixDossier = "";
    $("b-utiliser-dossier").disabled = true;
    erreur.hidden = true;
    liste.textContent = "";
    titre.textContent = "Ouverture des dossiers…";

    try {
      const url = "/api/selection-dossiers" +
        (chemin ? "?d=" + encodeURIComponent(chemin) : "");

      const reponse = await api(url);
      if (!reponse.ok) {
        throw new Error("Ce dossier n'est pas accessible.");
      }

      const resultat = await reponse.json();

      if (demande !== demandeChoix ||
          $("fenetre-dossiers").hidden) return;

      if (resultat.racines) {
        titre.textContent = "Emplacements accessibles";
        for (const r of resultat.racines) {
          ajouterDossierChoix(r.nom, r.chemin, "📁");
        }
        if (!resultat.racines.length) {
          erreur.textContent = "Aucun dossier accessible.";
          erreur.hidden = false;
        }
        return;
      }

      choixDossier = resultat.chemin;
      titre.textContent = resultat.chemin;
      $("b-utiliser-dossier").disabled = false;

      if (resultat.parent) {
        ajouterDossierChoix(
          ".. (dossier parent)", resultat.parent, "↩"
        );
      } else {
        ajouterDossierChoix(
          "Autres emplacements", "", "↩"
        );
      }

      for (const d of resultat.dossiers) {
        ajouterDossierChoix(d.nom, d.chemin, "📂");
      }

      if (!resultat.dossiers.length) {
        const li = document.createElement("li");
        li.className = "discret vide";
        li.textContent =
          "Aucun sous-dossier. Vous pouvez sélectionner ce dossier.";
        liste.appendChild(li);
      }

    } catch (e) {
      if (demande !== demandeChoix) return;
      titre.textContent = "Dossier indisponible";
      erreur.textContent = String(e.message || e);
      erreur.hidden = false;
    }
  }

  $("b-choisir-dossier").addEventListener("click", () => {
    if (enCours) {
      alert("Arrête le programme avant de changer de dossier.");
      return;
    }

    fermerTiroir();
    definirModeDossier("projet");
    $("fenetre-dossiers").hidden = false;
    afficherDossiersChoix(
      $("chemin").dataset.dossier || ""
    );
  });

  $("b-fermer-dossiers").addEventListener("click", () => {
    demandeChoix++;
    $("fenetre-dossiers").hidden = true;
  });

  $("b-emplacements").addEventListener(
    "click", () => afficherDossiersChoix()
  );

  $("b-utiliser-dossier").addEventListener("click", async () => {
    if (!choixDossier || enCours) return;

    const bouton = $("b-utiliser-dossier");
    bouton.disabled = true;

    if (modeChoixDossier === "csv") {
      try {
        const nom = $("nom-csv").value.trim();

        if (!/^[A-Za-z0-9_][A-Za-z0-9_.-]*\.csv$/.test(nom)
            || nom.length > 120) {
          throw new Error(
            "Nom invalide : utilise un nom se terminant par .csv, sans chemin."
          );
        }

        if (!csvDetecte || !sortieBrute) {
          throw new Error("Aucune sortie CSV disponible.");
        }

        const url =
          "/api/enregistrer-csv/" + enc(nom) +
          "?d=" + encodeURIComponent(choixDossier);

        const reponse = await api(url, sortieBrute);

        if (!reponse.ok) {
          throw new Error(
            "Enregistrement refuse (HTTP " +
            reponse.status + ")."
          );
        }

        const resultat = await reponse.json();

        if (!resultat.ok) {
          throw new Error(
            resultat.erreur || "Enregistrement impossible."
          );
        }

        if (resultat.taille !==
            new Blob([sortieBrute]).size) {
          throw new Error(
            "Le CSV enregistre n'a pas la taille attendue."
          );
        }

        demandeChoix++;
        $("fenetre-dossiers").hidden = true;

        alert(
          "CSV enregistre : " +
          choixDossier + "/" + nom
        );

      } catch (e) {
        $("selection-erreur").textContent =
          String(e.message || e);
        $("selection-erreur").hidden = false;

      } finally {
        bouton.disabled = false;
      }

      return;
    }

    try {
      await enregistrer();
      if (!sauve) {
        throw new Error("Enregistre d'abord le fichier ouvert.");
      }

      const reponse = await api(
        "/api/selection-dossier", choixDossier
      );

      if (!reponse.ok) {
        throw new Error("Dossier refusé par le serveur.");
      }

      const r = await reponse.json();
      if (!r.ok) {
        throw new Error(r.erreur || "Impossible de choisir le dossier.");
      }

      try {
        localStorage.removeItem("clair-studio-fichier");
      } catch (e) {}

      window.location.reload();

    } catch (e) {
      $("selection-erreur").textContent =
        String(e.message || e);
      $("selection-erreur").hidden = false;
      bouton.disabled = !choixDossier;
    }
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
    if (android) {
      try { android.barres(sombre); } catch (e) { /* rien */ }
    }
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
