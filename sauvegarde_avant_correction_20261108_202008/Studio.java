package clair.studio;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.Window;
import android.view.textclassifier.TextClassifier;
import android.webkit.CookieManager;
import android.webkit.JavascriptInterface;
import android.webkit.RenderProcessGoneDetail;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.net.HttpURLConnection;
import java.net.URL;

/*
 * Clair Studio en application (V0.71.6, V0.71.7).
 *
 * Le Studio reste le meme : un petit serveur web ecrit en Clair, lance dans Termux (tools/studio/studio.sh).
 * L'appli l'affiche dans une WebView, sans ce que Chrome ajoute : barre d'adresse, tirer pour recharger,
 * et surtout le menu de selection (Partager, Rechercher, Traduire, Gemini...). Ce menu ne garde que Couper,
 * Copier, Coller et Tout selectionner.
 *
 * L'adresse du Studio porte une cle (?t=...). L'appli la recoit de deux facons :
 *  - studio.sh, lance dans Termux, ouvre l'appli avec l'adresse complete ;
 *  - touchee depuis son icone, l'appli demande a Termux (permission RUN_COMMAND, une fois) de lancer le
 *    Studio s'il ne tourne pas, puis de lire sa cle (tools/studio/data/jeton).
 * L'adresse est gardee pour les fois suivantes.
 *
 * V0.71.7 : aucune classe anonyme ni interne (non statique). Java 21 leur ajoute un attribut MethodParameters
 * sans noms, que le d8 de Termux (R8 3.3) ne sait pas lire. Les classes du bas sont statiques, et les suites
 * d'une sonde passent par des numeros d'etape.
 */
public class Studio extends Activity {
    static final String TERMUX = "com.termux";
    static final String PERMISSION = "com.termux.permission.RUN_COMMAND";
    static final String SH = "/data/data/com.termux/files/usr/bin/sh";
    static final String MAISON = "/data/data/com.termux/files/home";
    static final String DEFAUT = "http://127.0.0.1:8090/";
    static final int DEMANDE_PERMISSION = 1, RESULTAT_JETON = 2, RESULTAT_LANCEMENT = 3;
    static final int FLAG_MUTABLE = 0x02000000;            // PendingIntent.FLAG_MUTABLE (Android 12)
    static final int REESSAYER = 1, REGLAGES = 2;          // les boutons des pages d'attente

    // ce qu'il faut faire quand une sonde revient
    static final int E_CONNECTER = 1, E_ATTENTE = 2;
    // ce qu'il faut faire quand une attente finit
    static final int S_AVANT_LANCER = 1, S_APRES_LANCER = 2;
    // ce qui demande Termux (et peut attendre la permission)
    static final int A_LANCER = 1, A_JETON = 2;
    // les taches remises a plus tard sur le fil de l'interface
    static final int T_DEMARRER = 1, T_RECONNECTER = 2, T_REGLAGES = 3, T_BARRES_CLAIRES = 4, T_BARRES_SOMBRES = 5,
            T_SONDER_ENCORE = 6, T_DELAI_JETON = 7;

    Vue vue;
    SharedPreferences prefs;
    final Handler ui = new Handler(Looper.getMainLooper());
    int essai = 0;                // chaque tentative annule les attentes de la precedente
    boolean attente = false;      // une page d'attente ou d'erreur est affichee (pas le Studio)
    boolean occupe = false;       // une tentative est en cours
    boolean parti = false;        // l'appli a ete quittee (Termux, l'accueil...) depuis la derniere erreur
    boolean attendJeton = false;
    int apresPermission = 0;
    boolean dejaLance = false;    // connecter : le Studio a deja ete lance (ou attendu)
    boolean cleLue = false;       // connecter : la cle vient d'etre lue dans Termux
    String attCible;              // l'attente en cours : l'adresse sondee, le debut, la fin, la suite
    long attDebut, attFin;
    int attSuite;

    @Override
    protected void onCreate(Bundle etat) {
        super.onCreate(etat);
        prefs = getSharedPreferences("studio", MODE_PRIVATE);
        barres(nuit());
        vue = new Vue(this);
        setContentView(vue);
        WebSettings s = vue.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setAllowFileAccess(false);
        s.setAllowContentAccess(false);
        s.setSupportMultipleWindows(false);
        // le menu de selection sans Partager, Rechercher sur le Web, ni les applis qui traitent le texte
        s.setDisabledActionModeMenuItems(WebSettings.MENU_ITEM_SHARE | WebSettings.MENU_ITEM_WEB_SEARCH
                | WebSettings.MENU_ITEM_PROCESS_TEXT);
        vue.setTextClassifier(TextClassifier.NO_OP);       // ni Appeler, ni Itineraire sur un texte choisi
        vue.addJavascriptInterface(new Pont(this), "ClairAndroid");
        vue.setWebViewClient(new Client(this));
        vue.setWebChromeClient(new WebChromeClient());    // alert, confirm et prompt du Studio
        CookieManager.getInstance().setAcceptCookie(true);
        if (!ouvrirAdresse(getIntent())) demarrer();
    }

    @Override
    protected void onNewIntent(Intent i) {
        super.onNewIntent(i);
        setIntent(i);
        ouvrirAdresse(i);
    }

    @Override
    protected void onResume() {
        super.onResume();
        vue.onResume();
        // revenu de Termux apres une erreur (le Studio y a peut-etre ete lance) : on reessaie
        if (attente && !occupe && parti) demarrer();
        parti = false;
    }

    @Override
    protected void onPause() {
        super.onPause();
        vue.onPause();
        CookieManager.getInstance().flush();
    }

    @Override
    protected void onStop() {
        super.onStop();
        parti = true;
    }

    @Override
    protected void onDestroy() {
        if (vue != null) vue.destroy();
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        moveTaskToBack(true);       // le Studio reste ouvert, comme un onglet
    }

    // ---- l'adresse du Studio ----

    static boolean local(Uri u) {
        if (u == null) return false;
        String h = u.getHost();
        return "http".equals(u.getScheme()) && ("127.0.0.1".equals(h) || "localhost".equals(h));
    }

    String adresse() {
        return prefs.getString("adresse", null);
    }

    String base() {
        String a = adresse();
        if (a == null) return DEFAUT;
        Uri u = Uri.parse(a);
        return u.getScheme() + "://" + u.getHost() + (u.getPort() > 0 ? ":" + u.getPort() : "") + "/";
    }

    int port() {
        int p = Uri.parse(base()).getPort();
        return p > 0 ? p : 8090;
    }

    // l'appli ouverte par studio.sh : l'adresse complete, avec sa cle
    boolean ouvrirAdresse(Intent i) {
        Uri u = i == null ? null : i.getData();
        if (!local(u)) return false;
        prefs.edit().putString("adresse", u.toString()).apply();
        charger(u.toString());
        return true;
    }

    void charger(String a) {
        essai++;
        occupe = false;
        attente = false;
        attendJeton = false;
        vue.loadUrl(a);
    }

    // ---- trouver le Studio, le lancer si besoin ----

    // le code HTTP de l'adresse (sans suivre de redirection), -1 sans serveur
    static int code(String cible) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(cible).openConnection();
            c.setInstanceFollowRedirects(false);
            c.setConnectTimeout(1000);
            c.setReadTimeout(3000);
            c.setUseCaches(false);
            return c.getResponseCode();
        } catch (Exception e) {
            return -1;
        } finally {
            if (c != null) c.disconnect();
        }
    }

    // sonde l'adresse sur un autre fil ; la reponse revient a retour(), avec l'etape
    void sonder(String cible, int etape) {
        new Sonde(this, cible, essai, etape).start();
    }

    void retour(int n, int etape, int c) {
        if (n != essai) return;              // une tentative plus recente a pris la main
        if (etape == E_CONNECTER) apresConnexion(c);
        else if (etape == E_ATTENTE) apresAttente(c);
    }

    void demarrer() {
        essai++;
        occupe = true;
        attendJeton = false;
        page("Clair Studio", "Connexion au Studio…", 0);
        connecter(false, false);
    }

    // lance : le Studio a deja ete lance par l'appli ; cle : la cle vient d'etre lue dans Termux
    void connecter(boolean lance, boolean cle) {
        dejaLance = lance;
        cleLue = cle;
        String a = adresse();
        sonder(a != null ? a : base(), E_CONNECTER);
    }

    void apresConnexion(int c) {
        String a = adresse();
        if (c >= 200 && c < 400 && a != null) {
            charger(a);
        } else if (c > 0) {
            // le Studio tourne, mais l'appli n'a pas (ou plus) sa cle
            if (cleLue) erreur("La clé du Studio ne marche pas",
                    "Lance le Studio depuis Termux : <code>sh ~/clair/tools/studio/studio.sh</code>. Il ouvrira l'appli avec la bonne adresse.", REESSAYER);
            else avecTermux(A_JETON);
        } else if (!dejaLance) {
            // studio.sh le demarre peut-etre deja : quelques secondes avant de le lancer nous-memes
            attendre(base(), 4000, S_AVANT_LANCER);
        } else {
            erreur("Le Studio ne répond pas", "Regarde la session « Clair Studio » dans Termux.", REESSAYER);
        }
    }

    // sonde toutes les 0,7 s jusqu'a ce que le serveur reponde, ou pendant duree ms
    void attendre(String cible, long duree, int suite) {
        attCible = cible;
        attDebut = System.currentTimeMillis();
        attFin = attDebut + duree;
        attSuite = suite;
        sonder(cible, E_ATTENTE);
    }

    void apresAttente(int c) {
        long t = System.currentTimeMillis();
        if (c > 0 || t > attFin) {
            if (attSuite == S_AVANT_LANCER) {
                if (c > 0) connecter(true, cleLue);
                else avecTermux(A_LANCER);
            } else {
                if (c > 0) connecter(true, false);
                else erreur("Termux n'a pas démarré le Studio",
                        "Regarde la session « Clair Studio » dans Termux, ou lance le Studio toi-même : <code>sh ~/clair/tools/studio/studio.sh</code>", REESSAYER);
            }
            return;
        }
        compteur((t - attDebut) / 1000);
        ui.postDelayed(new Tache(this, T_SONDER_ENCORE, essai), 700);
    }

    void tache(int quoi, int n) {
        if (quoi == T_DEMARRER) demarrer();
        else if (quoi == T_RECONNECTER) {
            if (!occupe) demarrer();
        } else if (quoi == T_REGLAGES) ouvrirReglages();
        else if (quoi == T_BARRES_CLAIRES) barres(false);
        else if (quoi == T_BARRES_SOMBRES) barres(true);
        else if (quoi == T_SONDER_ENCORE) {
            if (n == essai) sonder(attCible, E_ATTENTE);
        } else if (quoi == T_DELAI_JETON) {
            if (n == essai && attendJeton) erreur("Termux n'a pas répondu",
                    "Dans Termux, <code>~/.termux/termux.properties</code> doit contenir <code>allow-external-apps = true</code>. Sinon, lance le Studio depuis Termux : <code>sh ~/clair/tools/studio/studio.sh</code>", REESSAYER);
        }
    }

    Intent commande(String script, boolean fond, String nom) {
        Intent i = new Intent("com.termux.RUN_COMMAND");
        i.setClassName(TERMUX, "com.termux.app.RunCommandService");
        i.putExtra("com.termux.RUN_COMMAND_PATH", SH);
        i.putExtra("com.termux.RUN_COMMAND_ARGUMENTS", new String[] { "-c", script });
        i.putExtra("com.termux.RUN_COMMAND_WORKDIR", MAISON);
        i.putExtra("com.termux.RUN_COMMAND_BACKGROUND", fond);
        i.putExtra("com.termux.RUN_COMMAND_COMMAND_LABEL", nom);
        return i;
    }

    PendingIntent resultat(int demande) {
        return createPendingResult(demande, new Intent(this, Studio.class), PendingIntent.FLAG_ONE_SHOT | FLAG_MUTABLE);
    }

    boolean envoyer(Intent i) {
        try {
            startForegroundService(i);
            return true;
        } catch (RuntimeException e) {
            erreur("Termux ne répond pas", html(String.valueOf(e.getMessage())), REESSAYER);
            return false;
        }
    }

    void avecTermux(int action) {
        try {
            getPackageManager().getPackageInfo(TERMUX, 0);
        } catch (PackageManager.NameNotFoundException e) {
            erreur("Termux n'est pas installé", "Clair Studio se lance dans Termux, où Clair est installé.", 0);
            return;
        }
        if (checkSelfPermission(PERMISSION) == PackageManager.PERMISSION_GRANTED) {
            faire(action);
            return;
        }
        apresPermission = action;
        page("Clair Studio", "Autorise Clair Studio à lancer le Studio dans Termux.", 0);
        requestPermissions(new String[] { PERMISSION }, DEMANDE_PERMISSION);
    }

    @Override
    public void onRequestPermissionsResult(int demande, String[] p, int[] g) {
        if (demande != DEMANDE_PERMISSION) return;
        int a = apresPermission;
        apresPermission = 0;
        if (g.length > 0 && g[0] == PackageManager.PERMISSION_GRANTED && a != 0) faire(a);
        else erreur("Sans autorisation",
                "Clair Studio ne peut pas lancer le Studio lui-même. Lance-le depuis Termux : <code>sh ~/clair/tools/studio/studio.sh</code>. Il s'ouvrira ici.", REESSAYER | REGLAGES);
    }

    void faire(int action) {
        if (action == A_LANCER) lancer();
        else if (action == A_JETON) jeton();
    }

    // le Studio, dans une nouvelle session de Termux (Termux reste derriere l'appli)
    void lancer() {
        Intent i = commande("CLAIR_STUDIO_OUVRIR=0 exec sh \"$HOME/clair/tools/studio/studio.sh\" \"\" " + port(), false, "Clair Studio");
        i.putExtra("com.termux.RUN_COMMAND_SESSION_ACTION", "2");
        i.putExtra("com.termux.RUN_COMMAND_SHELL_NAME", "Clair Studio");
        i.putExtra("com.termux.RUN_COMMAND_PENDING_INTENT", resultat(RESULTAT_LANCEMENT));
        if (!envoyer(i)) return;
        page("Clair Studio", "Démarrage du Studio dans Termux… <span id=\"n\"></span>", 0);
        attendre(base(), 240000, S_APRES_LANCER);
    }

    // la cle du Studio, lue dans Termux
    void jeton() {
        Intent i = commande("cat \"$HOME/clair/tools/studio/data/jeton\"", true, "Clair Studio : la clé");
        i.putExtra("com.termux.RUN_COMMAND_PENDING_INTENT", resultat(RESULTAT_JETON));
        if (!envoyer(i)) return;
        attendJeton = true;
        page("Clair Studio", "Lecture de la clé du Studio…", 0);
        ui.postDelayed(new Tache(this, T_DELAI_JETON, essai), 20000);
    }

    @Override
    protected void onActivityResult(int demande, int r, Intent data) {
        super.onActivityResult(demande, r, data);
        Bundle b = data == null ? null : data.getBundleExtra("result");
        String sortie = b == null ? null : b.getString("stdout");
        String probleme = b == null ? null : b.getString("errmsg");
        int fin = b == null ? -1 : b.getInt("exitCode", -1);
        if (demande == RESULTAT_JETON) {
            if (!attendJeton) return;
            attendJeton = false;
            String t = sortie == null ? "" : sortie.trim();
            if (t.matches("[A-Za-z0-9_-]{8,128}")) {
                prefs.edit().putString("adresse", base() + "?t=" + t).apply();
                connecter(true, true);
            } else if (probleme != null && probleme.length() > 0) {
                erreur("Termux a refusé", html(probleme), REESSAYER);
            } else {
                erreur("Pas de clé du Studio", "Lance le Studio une fois depuis Termux : <code>sh ~/clair/tools/studio/studio.sh</code>", REESSAYER);
            }
        } else if (demande == RESULTAT_LANCEMENT) {
            // la session s'est arretee (ou Termux a refuse) pendant que l'appli attendait le Studio
            if (!attente || !occupe) return;
            if (probleme != null && probleme.length() > 0) erreur("Termux a refusé", html(probleme), REESSAYER);
            else erreur("Le Studio s'est arrêté", "Code de sortie " + fin + ". Regarde la session « Clair Studio » dans Termux.", REESSAYER);
        }
    }

    void ouvrirReglages() {
        try {
            startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:" + getPackageName())));
        } catch (RuntimeException e) {
            // pas de page de reglages
        }
    }

    // ---- les pages d'attente ----

    boolean nuit() {
        return (getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
    }

    static String html(String t) {
        return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;");
    }

    void erreur(String titre, String texte, int boutons) {
        essai++;
        occupe = false;
        attendJeton = false;
        page(titre, texte, boutons);
    }

    void page(String titre, String texte, int boutons) {
        attente = true;
        boolean s = nuit();
        String fond = s ? "#16181d" : "#ffffff", ecrit = s ? "#e6e8ec" : "#1d2129", bloc = s ? "#1f2229" : "#f3f4f6";
        String accent = s ? "#60a5fa" : "#2563eb", sur = s ? "#16181d" : "#ffffff";
        StringBuilder b = new StringBuilder();
        b.append("<!doctype html><html lang=\"fr\"><head><meta charset=\"utf-8\">")
                .append("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><style>")
                .append("body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;background:").append(fond)
                .append(";color:").append(ecrit).append(";font:16px/1.5 system-ui,sans-serif}")
                .append("main{max-width:28em;padding:24px}h1{font-size:20px;margin:0 0 10px}p{margin:0 0 16px}")
                .append("code{font-family:monospace;font-size:14px;background:").append(bloc).append(";padding:2px 5px;border-radius:4px;word-break:break-all}")
                .append("button{font:inherit;font-weight:600;padding:10px 16px;margin:0 8px 8px 0;border:0;border-radius:8px;background:")
                .append(accent).append(";color:").append(sur).append("}button.second{background:").append(bloc).append(";color:").append(ecrit).append("}")
                .append("</style></head><body><main><h1>").append(html(titre)).append("</h1><p>").append(texte).append("</p>");
        if ((boutons & REESSAYER) != 0) b.append("<button onclick=\"ClairAndroid.reessayer()\">Réessayer</button>");
        if ((boutons & REGLAGES) != 0) b.append("<button class=\"second\" onclick=\"ClairAndroid.reglages()\">Autorisations de l'appli</button>");
        b.append("</main></body></html>");
        vue.loadDataWithBaseURL(null, b.toString(), "text/html", "utf-8", null);
    }

    void compteur(long secondes) {
        if (secondes >= 2) vue.evaluateJavascript("var e=document.getElementById('n');if(e)e.textContent='" + secondes + " s'", null);
    }

    // la barre d'etat et la barre de navigation prennent le fond du Studio
    void barres(boolean sombre) {
        Window w = getWindow();
        int c = sombre ? 0xff16181d : 0xffffffff;
        w.setStatusBarColor(c);
        w.setNavigationBarColor(c);
        View d = w.getDecorView();
        int f = d.getSystemUiVisibility() & ~(View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
        if (!sombre) f |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        d.setSystemUiVisibility(f);
    }

    // ---- les liens : le Studio reste ici, le reste s'ouvre dans le navigateur ----

    boolean dehors(Uri u) {
        if (u == null) return false;
        String s = u.getScheme();
        if (local(u) || "about".equals(s) || "data".equals(s) || "blob".equals(s) || "javascript".equals(s)) return false;
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, u));
        } catch (RuntimeException e) {
            // aucune appli pour ce lien
        }
        return true;
    }

    void pageEnPanne(WebResourceRequest r) {
        if (r.isForMainFrame() && local(r.getUrl()) && !attente)
            erreur("Le Studio ne répond plus", "Il a peut-être été arrêté dans Termux.", REESSAYER);
    }

    // ---- les classes d'aide : toutes statiques (voir plus haut, V0.71.7) ----

    // une sonde, sur son propre fil (pas de reseau sur le fil de l'interface)
    static final class Sonde extends Thread {
        final Studio s;
        final String cible;
        final int n, etape;

        Sonde(Studio s, String cible, int n, int etape) {
            this.s = s;
            this.cible = cible;
            this.n = n;
            this.etape = etape;
        }

        @Override
        public void run() {
            s.ui.post(new Retour(s, n, etape, code(cible)));
        }
    }

    static final class Retour implements Runnable {
        final Studio s;
        final int n, etape, c;

        Retour(Studio s, int n, int etape, int c) {
            this.s = s;
            this.n = n;
            this.etape = etape;
            this.c = c;
        }

        @Override
        public void run() {
            s.retour(n, etape, c);
        }
    }

    static final class Tache implements Runnable {
        final Studio s;
        final int quoi, n;

        Tache(Studio s, int quoi, int n) {
            this.s = s;
            this.quoi = quoi;
            this.n = n;
        }

        @Override
        public void run() {
            s.tache(quoi, n);
        }
    }

    static final class Client extends WebViewClient {
        final Studio s;

        Client(Studio s) {
            this.s = s;
        }

        @Override
        public boolean shouldOverrideUrlLoading(WebView v, WebResourceRequest r) {
            return s.dehors(r.getUrl());
        }

        @Override
        @SuppressWarnings("deprecation")
        public boolean shouldOverrideUrlLoading(WebView v, String url) {
            return s.dehors(Uri.parse(url));
        }

        @Override
        public void onReceivedError(WebView v, WebResourceRequest r, WebResourceError e) {
            s.pageEnPanne(r);
        }

        @Override
        public boolean onRenderProcessGone(WebView v, RenderProcessGoneDetail d) {
            s.recreate();        // le moteur de la page s'est arrete : l'appli repart, sans s'arreter elle-meme
            return true;
        }
    }

    // ce que la page du Studio peut demander a Android (window.ClairAndroid)
    static final class Pont {
        final Studio s;

        Pont(Studio s) {
            this.s = s;
        }

        @JavascriptInterface
        public boolean copier(String texte) {
            ClipboardManager cm = (ClipboardManager) s.getSystemService(Context.CLIPBOARD_SERVICE);
            if (cm == null) return false;
            cm.setPrimaryClip(ClipData.newPlainText("Clair", texte == null ? "" : texte));
            return true;
        }

        @JavascriptInterface
        public String coller() {
            ClipboardManager cm = (ClipboardManager) s.getSystemService(Context.CLIPBOARD_SERVICE);
            ClipData c = cm == null ? null : cm.getPrimaryClip();
            if (c == null || c.getItemCount() == 0) return "";
            CharSequence t = c.getItemAt(0).coerceToText(s);
            return t == null ? "" : t.toString();
        }

        @JavascriptInterface
        public void barres(boolean sombre) {
            s.ui.post(new Tache(s, sombre ? T_BARRES_SOMBRES : T_BARRES_CLAIRES, 0));
        }

        // la page a recu 403 : la session ou la cle ne vaut plus, on la redemande
        @JavascriptInterface
        public void reconnecter() {
            s.ui.post(new Tache(s, T_RECONNECTER, 0));
        }

        @JavascriptInterface
        public void reessayer() {
            s.ui.post(new Tache(s, T_DEMARRER, 0));
        }

        @JavascriptInterface
        public void reglages() {
            s.ui.post(new Tache(s, T_REGLAGES, 0));
        }

        @JavascriptInterface
        public int version() {
            return 2;
        }
    }

    // ---- le menu de selection : Couper, Copier, Coller, Tout selectionner, rien d'autre ----

    static final class Vue extends WebView {
        Vue(Context c) {
            super(c);
        }

        @Override
        public ActionMode startActionMode(ActionMode.Callback cb, int type) {
            return super.startActionMode(new Filtre(getContext(), cb), type);
        }

        @Override
        public ActionMode startActionMode(ActionMode.Callback cb) {
            return super.startActionMode(new Filtre(getContext(), cb));
        }
    }

    static final class Filtre extends ActionMode.Callback2 {
        final Context ctx;
        final ActionMode.Callback cb;

        Filtre(Context ctx, ActionMode.Callback cb) {
            this.ctx = ctx;
            this.cb = cb;
        }

        boolean garde(MenuItem it) {
            int id = it.getItemId();
            if (id == android.R.id.cut || id == android.R.id.copy || id == android.R.id.paste || id == android.R.id.selectAll) return true;
            CharSequence t = it.getTitle();
            if (t == null) return false;
            String s = t.toString();
            return s.equals(ctx.getString(android.R.string.cut)) || s.equals(ctx.getString(android.R.string.copy))
                    || s.equals(ctx.getString(android.R.string.paste)) || s.equals(ctx.getString(android.R.string.selectAll));
        }

        // cache le reste ; si rien n'est reconnu (un Android qui nommerait autrement), ne cache rien
        void trier(Menu m) {
            boolean connu = false;
            for (int i = 0; i < m.size(); i++) if (garde(m.getItem(i))) connu = true;
            if (!connu) return;
            for (int i = 0; i < m.size(); i++) {
                MenuItem it = m.getItem(i);
                if (!garde(it)) it.setVisible(false);
            }
        }

        @Override
        public boolean onCreateActionMode(ActionMode mode, Menu menu) {
            boolean r = cb.onCreateActionMode(mode, menu);
            trier(menu);
            return r;
        }

        @Override
        public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
            boolean r = cb.onPrepareActionMode(mode, menu);
            trier(menu);
            return r;
        }

        @Override
        public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
            return cb.onActionItemClicked(mode, item);
        }

        @Override
        public void onDestroyActionMode(ActionMode mode) {
            cb.onDestroyActionMode(mode);
        }

        @Override
        public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
            if (cb instanceof ActionMode.Callback2) ((ActionMode.Callback2) cb).onGetContentRect(mode, view, outRect);
            else super.onGetContentRect(mode, view, outRect);
        }
    }
}
