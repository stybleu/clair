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
