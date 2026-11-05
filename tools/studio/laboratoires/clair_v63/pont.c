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
