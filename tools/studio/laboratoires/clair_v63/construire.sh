#!/usr/bin/env bash
set -euo pipefail

R="$HOME/clair"
P="$HOME/clair_labs/apk_clair_v63"
JAR="${PREFIX:-}/share/java/android.jar"
PIC_SO="$HOME/clair_labs/tinycc_source_v5/libtcc.so"

echo "===== PREREQUIS ====="

[ "$(uname -m)" = "aarch64" ] || {
    echo "ARCHITECTURE_NON_ARM64"
    exit 1
}

for cmd in clang javac d8 aapt aapt2 keytool apksigner; do
    command -v "$cmd" >/dev/null || {
        echo "OUTIL_MANQUANT=$cmd"
        exit 1
    }
done

[ -r "$JAR" ] || {
    echo "ANDROID_JAR_ABSENT"
    exit 1
}

if [ ! -r "$PIC_SO" ] ||
   [ ! -r "$PREFIX/include/libtcc.h" ]; then
    echo "===== INSTALLATION TINYCC ====="
    echo "LIBTCC_PIC_OU_ENTETE_ABSENT"
    exit 1
fi

[ -r "$PIC_SO" ] || {
    echo "LIBTCC_ABSENTE"
    exit 1
}

[ -r "$PREFIX/include/libtcc.h" ] || {
    echo "LIBTCC_H_ABSENT"
    exit 1
}

JAVAC_BIN="$(readlink -f "$(command -v javac)")"
JDK="$(dirname "$(dirname "$JAVAC_BIN")")"

[ -r "$JDK/include/jni.h" ] || {
    echo "JNI_H_ABSENT"
    exit 1
}

mkdir -p \
    "$P/src/clair/labs/compilv63" \
    "$P/classes" \
    "$P/dex" \
    "$P/pack/lib/arm64-v8a"

echo "===== PONT COMPILATEUR JNI ====="

cat > "$P/pont.c" <<'C'
#define _POSIX_C_SOURCE 200809L
#include <jni.h>
#include <libtcc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

extern int clair_lab_generate_c(
    const char *, const char *
);

struct Diag {
    char texte[1200];
};

static void diagnostic(
    void *opaque, const char *msg
) {
    struct Diag *d = (struct Diag *)opaque;
    size_t n = strlen(d->texte);

    if (n + 2 < sizeof(d->texte))
        snprintf(
            d->texte + n,
            sizeof(d->texte) - n,
            "%s\n", msg
        );
}

static jstring reponse(
    JNIEnv *env, const char *texte
) {
    return (*env)->NewStringUTF(
        env, texte
    );
}

static double temps_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);

    return t.tv_sec * 1000.0 +
           t.tv_nsec / 1000000.0;
}

static void lire_erreur(
    const char *chemin,
    char *sortie,
    size_t taille
) {
    FILE *f = fopen(chemin, "rb");

    if (!f) {
        snprintf(
            sortie, taille,
            "Diagnostic non disponible"
        );
        return;
    }

    size_t n = fread(
        sortie, 1, taille - 1, f
    );

    sortie[n] = 0;
    fclose(f);
}

JNIEXPORT jstring JNICALL
Java_clair_labs_compilv63_Probe_executer(
    JNIEnv *env,
    jobject obj,
    jstring programme,
    jstring repertoire
) {
    (void)obj;

    if (!programme || !repertoire)
        return reponse(
            env,"ERREUR=PARAMETRE_ABSENT"
        );

    const char *code =
        (*env)->GetStringUTFChars(
            env, programme, NULL
        );

    const char *dir =
        (*env)->GetStringUTFChars(
            env, repertoire, NULL
        );

    if (!code || !dir) {
        if (code)
            (*env)->ReleaseStringUTFChars(
                env, programme, code
            );

        if (dir)
            (*env)->ReleaseStringUTFChars(
                env, repertoire, dir
            );

        return reponse(
            env,"ERREUR=CONVERSION_JNI"
        );
    }

    char racine[640];
    char source[700];
    char fichier_c[760];
    char errpath[700];
    char includes[700];

    int ok = snprintf(
        racine, sizeof(racine),
        "%s/clair_v63", dir
    );

    (*env)->ReleaseStringUTFChars(
        env, repertoire, dir
    );

    if (
        ok < 0 ||
        (size_t)ok >= sizeof(racine) ||
        strlen(code) > 8192 ||
        !*code
    ) {
        (*env)->ReleaseStringUTFChars(
            env, programme, code
        );

        return reponse(
            env,
            "ERREUR=CHEMIN_OU_SOURCE_INVALIDE"
        );
    }

    snprintf(
        source, sizeof(source),
        "%s.cl", racine
    );

    snprintf(
        fichier_c, sizeof(fichier_c),
        "%s.generated.c", racine
    );

    snprintf(
        errpath, sizeof(errpath),
        "%s_erreur.log", racine
    );

    snprintf(
        includes, sizeof(includes),
        "%s/include", racine
    );

    FILE *f = fopen(source, "wb");

    if (!f) {
        (*env)->ReleaseStringUTFChars(
            env, programme, code
        );
        return reponse(
            env,"ERREUR=ECRITURE_SOURCE"
        );
    }

    size_t len = strlen(code);
    size_t ecrits = fwrite(
        code, 1, len, f
    );

    int ferme = fclose(f);

    (*env)->ReleaseStringUTFChars(
        env, programme, code
    );

    if (ecrits != len || ferme != 0)
        return reponse(
            env,"ERREUR=ECRITURE_SOURCE"
        );

    unlink(fichier_c);

    int saved = dup(STDERR_FILENO);

    int fd = open(
        errpath,
        O_WRONLY | O_CREAT | O_TRUNC,
        0600
    );

    if (saved < 0 || fd < 0) {
        if (saved >= 0) close(saved);
        if (fd >= 0) close(fd);

        return reponse(
            env,"ERREUR=CAPTURE_DIAGNOSTICS"
        );
    }

    if (dup2(fd, STDERR_FILENO) < 0) {
        close(fd);
        close(saved);

        return reponse(
            env,"ERREUR=REDIRECTION_DIAGNOSTICS"
        );
    }

    close(fd);

    double debut = temps_ms();

    int rc = clair_lab_generate_c(
        source, racine
    );

    double generation =
        temps_ms() - debut;

    fflush(stderr);
    dup2(saved, STDERR_FILENO);
    close(saved);

    if (rc != 0) {
        char diag[1000];
        char msg[1300];

        lire_erreur(
            errpath, diag, sizeof(diag)
        );

        snprintf(
            msg, sizeof(msg),
            "GENERATION_CLAIR=ECHEC\n"
            "DIAGNOSTIC=%.900s",
            diag
        );

        return reponse(env,msg);
    }

    FILE *g = fopen(
        fichier_c, "rb"
    );

    if (!g)
        return reponse(
            env,
            "GENERATION_CLAIR=ECHEC\n"
            "FICHIER_C=ABSENT"
        );

    if (fseek(g,0,SEEK_END) != 0) {
        fclose(g);
        return reponse(
            env,"ERREUR=LECTURE_C"
        );
    }

    long taille = ftell(g);

    if (
        taille <= 0 ||
        taille > 1000000 ||
        fseek(g,0,SEEK_SET) != 0
    ) {
        fclose(g);
        return reponse(
            env,"ERREUR=TAILLE_C_INVALIDE"
        );
    }

    char *c = malloc(
        (size_t)taille + 1
    );

    if (!c) {
        fclose(g);
        return reponse(
            env,"ERREUR=MEMOIRE"
        );
    }

    size_t nb = fread(
        c,1,(size_t)taille,g
    );

    fclose(g);

    if (nb != (size_t)taille) {
        free(c);
        return reponse(
            env,"ERREUR=LECTURE_C"
        );
    }

    c[nb] = 0;

    struct Diag d = {{0}};

    TCCState *s = tcc_new();

    if (!s) {
        free(c);
        return reponse(
            env,"ERREUR=INITIALISATION_TINYCC"
        );
    }

    tcc_set_error_func(
        s, &d, diagnostic
    );

    tcc_set_lib_path(
        s, racine
    );

    tcc_add_library_path(
        s, racine
    );

    tcc_set_options(
        s, "-nostdinc"
    );

    tcc_add_include_path(
        s, includes
    );

    tcc_add_sysinclude_path(
        s, includes
    );

    debut = temps_ms();

    rc = tcc_set_output_type(
        s,TCC_OUTPUT_MEMORY
    );

    if (rc == 0)
        rc = tcc_compile_string(
            s,c
        );

    free(c);

    if (rc != 0) {
        char msg[1450];

        snprintf(
            msg,sizeof(msg),
            "GENERATION_CLAIR=OK\n"
            "COMPILATION_TINYCC=ECHEC\n"
            "DIAGNOSTIC=%.1100s",
            d.texte
        );

        tcc_delete(s);
        return reponse(env,msg);
    }

    rc = tcc_relocate(
        s,TCC_RELOCATE_AUTO
    );

    if (rc != 0) {
        char msg[1450];

        snprintf(
            msg,sizeof(msg),
            "GENERATION_CLAIR=OK\n"
            "COMPILATION_TINYCC=OK\n"
            "RELOCALISATION=ECHEC\n"
            "DIAGNOSTIC=%.1000s",
            d.texte
        );

        tcc_delete(s);
        return reponse(env,msg);
    }

    int (*main_clair)(void) =
        (int (*)(void))
        tcc_get_symbol(s,"main");

    if (!main_clair) {
        tcc_delete(s);

        return reponse(
            env,
            "GENERATION_CLAIR=OK\n"
            "COMPILATION_TINYCC=OK\n"
            "ERREUR=MAIN_ABSENT"
        );
    }

    double compilation =
        temps_ms() - debut;

    debut = temps_ms();

    int resultat =
        main_clair();

    double execution =
        temps_ms() - debut;

    tcc_delete(s);

    char msg[800];

    snprintf(
        msg,sizeof(msg),
        "GENERATION_CLAIR=OK\n"
        "COMPILATION_TINYCC=OK\n"
        "RELOCALISATION=OK\n"
        "EXECUTION_ARM64=OK\n"
        "RESULTAT_MAIN=%d\n"
        "GENERATION_MS=%.3f\n"
        "COMPILATION_MS=%.3f\n"
        "EXECUTION_MS=%.3f\n"
        "LLVM_DANS_APK=NON\n",
        resultat,
        generation,
        compilation,
        execution
    );

    return reponse(env,msg);
}
C

echo "===== COMPILATION DU MOTEUR C ====="

[ -s "$HOME/clair_labs/v62_frontend/libclairfront.so" ] || {
    echo "FRONTEND_V62_ABSENT"
    exit 1
}

cp "$HOME/clair_labs/v62_frontend/libclairfront.so" \
   "$P/pack/lib/arm64-v8a/libclairfront.so"

echo "FRONTEND_CLAIR=EMBARQUE"

clang -std=c11 -O2 -fPIC -shared \
    -I "$JDK/include" \
    -I "$JDK/include/linux" \
    "$P/pont.c" \
    -L "$P/pack/lib/arm64-v8a" -lclairfront \
    "$PIC_SO" -lm -ldl -Wl,--no-undefined \
    -o "$P/pack/lib/arm64-v8a/libclairv63.so"

cp "$PIC_SO" \
   "$P/pack/lib/arm64-v8a/libtcc.so"

if command -v llvm-readelf >/dev/null 2>&1; then
    echo "===== DEPENDANCES TCC ====="

    llvm-readelf -d \
      "$P/pack/lib/arm64-v8a/libtcc.so" |
      grep NEEDED || true
fi

echo "===== ARCHIVE TCC EMBARQUEE ====="

[ -s "$PREFIX/lib/tcc/libtcc1.a" ] || {
    echo "LIBTCC1_A=ABSENTE"
    exit 1
}

mkdir -p "$P/pack/assets"

cp "$PREFIX/lib/tcc/libtcc1.a" \
   "$P/pack/assets/libtcc1.a"

echo "LIBTCC1_A=EMBARQUEE"

echo "===== EN-TETES C EMBARQUES ====="

mkdir -p "$P/pack/assets/include"

cp "$HOME/clair_labs/v63_headers/"*.h \
   "$P/pack/assets/include/"

echo "EN_TETES_C=EMBARQUES"

echo "===== APPLICATION ANDROID ====="

cat > "$P/src/clair/labs/compilv63/Probe.java" <<'JAVA'
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
JAVA

cat > "$P/AndroidManifest.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<manifest
 xmlns:android="http://schemas.android.com/apk/res/android"
 package="clair.labs.compilv63"
 android:versionCode="1"
 android:versionName="0.2-labo">

 <uses-sdk
  android:minSdkVersion="29"
  android:targetSdkVersion="34" />

 <application
  android:label="Clair Compilateur Labo V6.3"
  android:extractNativeLibs="true">

  <activity
   android:name=".Probe"
   android:exported="true"
   android:windowSoftInputMode="adjustResize">

   <intent-filter>
    <action
     android:name="android.intent.action.MAIN"/>
    <category
     android:name="android.intent.category.LAUNCHER"/>
   </intent-filter>

  </activity>
 </application>
</manifest>
XML

echo "===== JAVA ====="

javac -source 8 -target 8 \
    -Xlint:-options \
    -bootclasspath "$JAR" \
    -encoding UTF-8 \
    -d "$P/classes" \
    "$P/src/clair/labs/compilv63/Probe.java"

echo "===== DEX ====="

d8 --release --min-api 29 \
    --lib "$JAR" \
    --output "$P/dex" \
    "$P/classes/clair/labs/compilv63/Probe.class"

echo "===== CONSTRUCTION APK ====="

FW="/system/framework/framework-res.apk"

if ! aapt2 link \
    -I "$FW" \
    --manifest "$P/AndroidManifest.xml" \
    -o "$P/base.apk" \
    >"$P/cadre.log" 2>&1
then
    echo "CADRE_ANDROID=REPLI"

    CADRE="$R/tools/studio/android/cadre"

    [ -d "$CADRE/res" ] &&
    [ -r "$CADRE/AndroidManifest.xml" ] || {
        echo "CADRE_ANDROID_ABSENT"
        exit 1
    }

    aapt2 compile \
        --dir "$CADRE/res" \
        -o "$P/cadre.zip"

    aapt2 link -x \
        --manifest "$CADRE/AndroidManifest.xml" \
        -o "$P/cadre.apk" \
        "$P/cadre.zip"

    aapt2 link \
        -I "$P/cadre.apk" \
        --manifest "$P/AndroidManifest.xml" \
        -o "$P/base.apk"
fi

cp "$P/dex/classes.dex" \
   "$P/pack/classes.dex"

(
    cd "$P/pack"

    aapt add "$P/base.apk" \
        classes.dex \
        lib/arm64-v8a/libclairv63.so \
        lib/arm64-v8a/libclairfront.so \
        lib/arm64-v8a/libtcc.so \
        assets/libtcc1.a \
        assets/include/stdint.h \
        assets/include/stdbool.h \
        assets/include/inttypes.h \
        assets/include/stdio.h \
        assets/include/stdlib.h \
        assets/include/string.h
)

echo "===== ALIGNEMENT ====="

if command -v zipalign >/dev/null 2>&1
then
    zipalign -f -p 4 \
        "$P/base.apk" \
        "$P/aligne.apk"
else
    cp "$P/base.apk" "$P/aligne.apk"
fi

echo "===== SIGNATURE ====="

KEY="$P/labo_v63.jks"

if [ ! -f "$KEY" ]; then
    keytool -genkeypair \
        -keystore "$KEY" \
        -storepass clair-labo \
        -keypass clair-labo \
        -alias labo \
        -keyalg RSA \
        -keysize 2048 \
        -validity 3650 \
        -dname "CN=Clair Labo V5" \
        >/dev/null 2>&1
fi

apksigner sign \
    --ks "$KEY" \
    --ks-pass pass:clair-labo \
    --key-pass pass:clair-labo \
    --ks-key-alias labo \
    --out "$P/clair-labo-v63.apk" \
    "$P/aligne.apk"

apksigner verify \
    "$P/clair-labo-v63.apk"

echo "===== CONTENU APK ====="

aapt list "$P/clair-labo-v63.apk" |
    grep -E 'classes.dex|libclairv63.so|libclairfront.so|libtcc.so|libtcc1.a|include/stdint.h'

echo "===== TELECHARGEMENTS ====="

DEST="$HOME/storage/downloads/clair-labo-v63.apk"

[ -d "$HOME/storage/downloads" ] || {
    echo "TELECHARGEMENTS_INACCESSIBLES"
    exit 1
}

cp "$P/clair-labo-v63.apk" "$DEST"

echo "===== BILAN ====="
echo "APK_LABO_V5=CONSTRUITE"
echo "APK_CHEMIN=$DEST"
echo "COMPILATEUR_CLAIR_DANS_APK=A_TESTER"
echo "LLVM_DANS_APK=NON"
echo "CLAIR_STUDIO=INCHANGE"
