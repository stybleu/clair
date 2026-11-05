package clair.labs.compilv5;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.content.Context;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.widget.Toast;

public class Probe extends Activity
        implements View.OnClickListener {

    private EditText source;
    private TextView sortie;

    private String journal =
        "LABORATOIRE=COMPILATEUR_V5\n" +
        "LLVM_DANS_APK=NON\n" +
        "TEST_A_EFFECTUER=OUI\n";

    private static String erreurNative;

    static {
        try {
            System.loadLibrary("tcc");
            System.loadLibrary("clairv5");
        } catch (Throwable t) {
            erreurNative = t.toString();
        }
    }

    private native String compiler(
        String code,int entree,String dossier
    );

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

        try (
            java.io.InputStream entree =
                getAssets().open("libtcc1.a");
            java.io.FileOutputStream sortieArchive =
                new java.io.FileOutputStream(
                    new java.io.File(
                        getFilesDir(), "libtcc1.a"
                    )
                )
        ) {
            byte[] tampon = new byte[8192];
            int n;

            while ((n = entree.read(tampon)) != -1) {
                sortieArchive.write(tampon,0,n);
            }

        } catch (Throwable t) {
            erreurNative =
                "ARCHIVE_LIBTCC1=ECHEC " + t;
        }

        LinearLayout l =
            new LinearLayout(this);

        l.setOrientation(
            LinearLayout.VERTICAL
        );

        l.setPadding(16,16,16,16);

        TextView titre =
            new TextView(this);

        titre.setText(
            "Clair V5.5 - compilation C Android"
        );

        titre.setTextSize(19);
        l.addView(titre);

        source = new EditText(this);

        source.setText(
            "int clair_calcul(int n) {\n" +
            "    int s = 0;\n" +
            "    for (int i = 0; i < n; i++)\n" +
            "        s += i % 97;\n" +
            "    return s;\n" +
            "}\n"
        );

        source.setTextSize(14);
        source.setMinLines(7);

        source.setGravity(
            android.view.Gravity.TOP
        );

        source.setHorizontallyScrolling(true);

        l.addView(source);

        bouton(
            l,
            "Compiler et executer le C",
            "compiler"
        );

        bouton(
            l,
            "Tester une erreur C",
            "erreur"
        );

        bouton(
            l,
            "COPIER LE JOURNAL",
            "copier"
        );

        ScrollView sc =
            new ScrollView(this);

        sortie = new TextView(this);
        sortie.setTextSize(15);
        sortie.setTextIsSelectable(true);

        sc.addView(sortie);
        l.addView(sc);

        if (erreurNative != null) {
            journal +=
                "CHARGEMENT_COMPILATEUR=ECHEC\n" +
                erreurNative + "\n";
        } else {
            journal +=
                "CHARGEMENT_COMPILATEUR=OK\n" +
                "LIBTCC1_PREPAREE=OUI\n";
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
                        "Clair V5",
                        journal
                    )
                );

                Toast.makeText(
                    this,
                    "Journal copie",
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

        String texte = "erreur".equals(id)
            ? "int clair_calcul(int n) { return n + ; }"
            : source.getText().toString();

        try {
            journal +=
                "\n===== ESSAI " +
                id.toUpperCase() +
                " =====\n";

            journal += compiler(
                texte,2500000,
                getFilesDir().getAbsolutePath()
            ) + "\n";

        } catch (Throwable t) {
            journal +=
                "ECHEC_NATIF=" + t + "\n";
        }

        sortie.setText(journal);
    }
}
