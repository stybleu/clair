package clair.labs.compilv63;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.*;
import android.content.*;
import java.io.*;

public class Probe extends Activity
        implements View.OnClickListener {

    private EditText source;
    private TextView sortie;

    private String journal =
        "LABORATOIRE=CLAIR_V63\n" +
        "LANGAGE_SOURCE=CLAIR\n" +
        "BACKEND=TINYCC_ARM64\n" +
        "LLVM_DANS_APK=NON\n";

    private static String erreurNative;

    private static final String SIMPLE =
        "def main() -> int {\n" +
        "    return 42\n" +
        "}\n";

    private static final String BOUCLE =
        "def main() -> int {\n" +
        "    let somme: int = 0\n" +
        "    for i in range(2500000) {\n" +
        "        somme = somme + (i % 97)\n" +
        "    }\n" +
        "    return somme\n" +
        "}\n";

    private static final String ERREUR =
        "def main() -> int {\n" +
        "    let x: int = 5 +\n" +
        "    return x\n" +
        "}\n";

    static {
        try {
            System.loadLibrary("clairfront");
            System.loadLibrary("tcc");
            System.loadLibrary("clairv63");
        } catch (Throwable t) {
            erreurNative = t.toString();
        }
    }

    private native String executer(
        String code, String dossier
    );

    private void copierAsset(
        String nom, File dest
    ) throws IOException {

        try (
            InputStream entree =
                getAssets().open(nom);
            FileOutputStream sortie =
                new FileOutputStream(dest)
        ) {
            byte[] tampon = new byte[8192];
            int n;

            while (
                (n = entree.read(tampon)) != -1
            ) {
                sortie.write(
                    tampon,0,n
                );
            }
        }
    }

    private void bouton(
        LinearLayout l,
        String texte,
        String id
    ) {
        Button b = new Button(this);

        b.setText(texte);
        b.setTag(id);
        b.setOnClickListener(this);

        l.addView(b);
    }

    @Override
    public void onCreate(Bundle saved) {
        super.onCreate(saved);

        try {
            File racine = new File(
                getFilesDir(),"clair_v63"
            );

            File include = new File(
                racine,"include"
            );

            if (!include.exists() &&
                !include.mkdirs()) {
                throw new IOException(
                    "Dossier include inaccessible"
                );
            }

            copierAsset(
                "libtcc1.a",
                new File(racine,"libtcc1.a")
            );

            String[] noms = {
                "stdint.h",
                "stdbool.h",
                "inttypes.h",
                "stdio.h",
                "stdlib.h",
                "string.h"
            };

            for (String nom : noms) {
                copierAsset(
                    "include/" + nom,
                    new File(include,nom)
                );
            }

        } catch (Throwable t) {
            erreurNative =
                "RESSOURCES=ECHEC " + t;
        }

        LinearLayout l =
            new LinearLayout(this);

        l.setOrientation(
            LinearLayout.VERTICAL
        );

        l.setPadding(12,12,12,12);

        TextView titre =
            new TextView(this);

        titre.setText(
            "Clair V6.3 - compilation native"
        );

        titre.setTextSize(19);
        l.addView(titre);

        source = new EditText(this);

        source.setText(SIMPLE);
        source.setTextSize(14);
        source.setMinLines(6);
        source.setMaxLines(9);

        source.setGravity(
            android.view.Gravity.TOP
        );

        l.addView(source);

        bouton(
            l,"Compiler et executer Clair",
            "compiler"
        );

        bouton(
            l,"Tester 42","simple"
        );

        bouton(
            l,"Tester la boucle","boucle"
        );

        bouton(
            l,"Tester une erreur Clair",
            "erreur"
        );

        bouton(
            l,"COPIER LE JOURNAL",
            "copier"
        );

        ScrollView sc =
            new ScrollView(this);

        sortie = new TextView(this);
        sortie.setTextSize(14);
        sortie.setTextIsSelectable(true);

        sc.addView(sortie);

        l.addView(
            sc,
            new LinearLayout.LayoutParams(
                -1,0,1
            )
        );

        if (erreurNative == null) {
            journal +=
                "CHARGEMENT_NATIF=OK\n" +
                "RESSOURCES_TCC=OK\n" +
                "TEST_DANS_APK=A_EFFECTUER\n";
        } else {
            journal +=
                "CHARGEMENT_NATIF=ECHEC\n" +
                erreurNative + "\n";
        }

        sortie.setText(journal);
        setContentView(l);
    }

    @Override
    public void onClick(View v) {
        String id = (String)v.getTag();

        if ("copier".equals(id)) {
            try {
                ClipboardManager cb =
                    (ClipboardManager)
                    getSystemService(
                        Context.CLIPBOARD_SERVICE
                    );

                cb.setPrimaryClip(
                    ClipData.newPlainText(
                        "Clair V6.3",journal
                    )
                );

                Toast.makeText(
                    this,"Journal copie",
                    Toast.LENGTH_SHORT
                ).show();

            } catch (Throwable t) {
                sortie.append(
                    "\nCOPIE=ECHEC " + t
                );
            }

            return;
        }

        if (erreurNative != null)
            return;

        String code =
            source.getText().toString();

        if ("simple".equals(id))
            code = SIMPLE;

        if ("boucle".equals(id))
            code = BOUCLE;

        if ("erreur".equals(id))
            code = ERREUR;

        try {
            journal +=
                "\n===== TEST " +
                id.toUpperCase() +
                " =====\n";

            journal += executer(
                code,
                getFilesDir().getAbsolutePath()
            ) + "\n";

        } catch (Throwable t) {
            journal +=
                "ECHEC_NATIF=" + t + "\n";
        }

        sortie.setText(journal);
    }
}
