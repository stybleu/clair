#!/usr/bin/env bash
set -euo pipefail

R="$HOME/clair"
P="$HOME/clair_labs/apk_compilateur_v5"
JAR="${PREFIX:-}/share/java/android.jar"
PIC_SO="/data/data/com.termux/files/home/clair_labs/tinycc_source_v5/libtcc.so"

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
    "$P/src/clair/labs/compilv5" \
    "$P/classes" \
    "$P/dex" \
    "$P/pack/lib/arm64-v8a"

echo "===== PONT COMPILATEUR JNI ====="

cat > "$P/pont.c" <<'C'
#define _POSIX_C_SOURCE 200809L

#include <jni.h>
#include <libtcc.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

struct Diagnostic {
    char buffer[1600];
};

static void diagnostic(
    void *opaque,
    const char *msg
) {
    struct Diagnostic *d =
        (struct Diagnostic*)opaque;

    size_t len = strlen(d->buffer);

    if (len < sizeof(d->buffer)-3) {
        snprintf(
            d->buffer + len,
            sizeof(d->buffer)-len,
            "%s\n",
            msg
        );
    }
}

static jstring response(
    JNIEnv *env,
    const char *s
) {
    return (*env)->NewStringUTF(env,s);
}

static double millis(void) {
    struct timespec ts;

    clock_gettime(
        CLOCK_MONOTONIC,&ts
    );

    return ts.tv_sec*1000.0 +
           ts.tv_nsec/1000000.0;
}

JNIEXPORT jstring JNICALL
Java_clair_labs_compilv5_Probe_compiler(
    JNIEnv *env,
    jobject obj,
    jstring source,
    jint entree,
    jstring chemin
) {
    (void)obj;

    if (!source)
        return response(
            env,"ERREUR=SOURCE_ABSENTE"
        );

    const char *code =
        (*env)->GetStringUTFChars(
            env,source,NULL
        );

    if (!code)
        return response(
            env,"ERREUR=CONVERSION_JNI"
        );

    size_t taille = strlen(code);

    if (taille == 0 || taille > 8192) {
        (*env)->ReleaseStringUTFChars(
            env,source,code
        );

        return response(
            env,"ERREUR=TAILLE_SOURCE"
        );
    }

    struct Diagnostic diag = {{0}};

    TCCState *s = tcc_new();

    if (!s) {
        (*env)->ReleaseStringUTFChars(
            env,source,code
        );

        return response(
            env,"ERREUR=INITIALISATION_TCC"
        );
    }

    tcc_set_error_func(
        s,&diag,diagnostic
    );

    if (!chemin) {
        tcc_delete(s);
        (*env)->ReleaseStringUTFChars(
            env,source,code
        );
        return response(
            env,"DOSSIER_TCC=ABSENT"
        );
    }

    const char *dossier =
        (*env)->GetStringUTFChars(
            env,chemin,NULL
        );

    if (!dossier) {
        tcc_delete(s);
        (*env)->ReleaseStringUTFChars(
            env,source,code
        );
        return response(
            env,"DOSSIER_TCC=INACCESSIBLE"
        );
    }

    tcc_set_lib_path(s,dossier);
    tcc_add_library_path(s,dossier);

    (*env)->ReleaseStringUTFChars(
        env,chemin,dossier
    );

    double debut = millis();

    int rc = tcc_set_output_type(
        s,TCC_OUTPUT_MEMORY
    );

    if (rc == 0)
        rc = tcc_compile_string(s,code);

    (*env)->ReleaseStringUTFChars(
        env,source,code
    );

    if (rc != 0) {
        char msg[1900];

        snprintf(
            msg,sizeof(msg),
            "COMPILATION_DANS_APK=ECHEC\n"
            "DIAGNOSTIC=%.1550s",
            diag.buffer
        );

        tcc_delete(s);
        return response(env,msg);
    }

    rc = tcc_relocate(s, TCC_RELOCATE_AUTO);

    if (rc != 0) {
        char msg[1900];

        snprintf(
            msg,sizeof(msg),
            "COMPILATION_C=OK\n"
            "RELOCALISATION=ECHEC\n"
            "DIAGNOSTIC=%.1550s",
            diag.buffer
        );

        tcc_delete(s);
        return response(env,msg);
    }

    int (*fn)(int) =
        (int (*)(int))
        tcc_get_symbol(
            s,"clair_calcul"
        );

    if (!fn) {
        tcc_delete(s);

        return response(
            env,
            "COMPILATION_C=OK\n"
            "ERREUR=FONCTION_clair_calcul_ABSENTE"
        );
    }

    double compilation =
        millis()-debut;

    debut = millis();

    int resultat = fn((int)entree);

    double execution =
        millis()-debut;

    char msg[2000];

    snprintf(
        msg,sizeof(msg),
        "COMPILATION_DANS_APK=OK\n"
        "COMPILATEUR=TINYCC_ARM64\n"
        "LLVM_DANS_APK=NON\n"
        "FONCTION=clair_calcul\n"
        "ENTREE=%d\n"
        "RESULTAT=%d\n"
        "COMPILATION_MS=%.3f\n"
        "EXECUTION_MS=%.3f\n"
        "CHARGEMENT_MEMOIRE=OK\n",
        (int)entree,
        resultat,
        compilation,
        execution
    );

    tcc_delete(s);

    return response(env,msg);
}
C

echo "===== COMPILATION DU MOTEUR C ====="

clang -std=c11 -O2 -fPIC -shared \
    -I "$JDK/include" \
    -I "$JDK/include/linux" \
    "$P/pont.c" \
    "$PIC_SO" -lm -ldl \
    -o "$P/pack/lib/arm64-v8a/libclairv5.so"

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

echo "===== APPLICATION ANDROID ====="

cat > "$P/src/clair/labs/compilv5/Probe.java" <<'JAVA'
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
JAVA

cat > "$P/AndroidManifest.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<manifest
 xmlns:android="http://schemas.android.com/apk/res/android"
 package="clair.labs.compilv5"
 android:versionCode="2"
 android:versionName="0.2-labo">

 <uses-sdk
  android:minSdkVersion="29"
  android:targetSdkVersion="34" />

 <application
  android:label="Clair Compilateur Labo V5.5"
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
    "$P/src/clair/labs/compilv5/Probe.java"

echo "===== DEX ====="

d8 --release --min-api 29 \
    --lib "$JAR" \
    --output "$P/dex" \
    "$P/classes/clair/labs/compilv5/Probe.class"

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
        lib/arm64-v8a/libclairv5.so \
        lib/arm64-v8a/libtcc.so \
        assets/libtcc1.a
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

KEY="$P/labo_v5.jks"

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
    --out "$P/clair-labo-v55.apk" \
    "$P/aligne.apk"

apksigner verify \
    "$P/clair-labo-v55.apk"

echo "===== CONTENU APK ====="

aapt list "$P/clair-labo-v55.apk" |
    grep -E 'classes.dex|libclairv5.so|libtcc.so|libtcc1.a'

echo "===== TELECHARGEMENTS ====="

DEST="$HOME/storage/downloads/clair-labo-v55.apk"

[ -d "$HOME/storage/downloads" ] || {
    echo "TELECHARGEMENTS_INACCESSIBLES"
    exit 1
}

cp "$P/clair-labo-v55.apk" "$DEST"

echo "===== BILAN ====="
echo "APK_LABO_V5=CONSTRUITE"
echo "APK_CHEMIN=$DEST"
echo "COMPILATEUR_DANS_APK=TCC_A_TESTER"
echo "LLVM_DANS_APK=NON"
echo "CLAIR_STUDIO=INCHANGE"
