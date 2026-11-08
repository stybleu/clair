# Optimisation DCE de Clair

Activation : ./build/clair programme.cl -o programme --dce

Le DCE supprime les fonctions, methodes, classes et certaines variables inutilisees.
Les effets de bord et les erreurs doivent etre preserves.
Validation : V7.6 30/30, V7.15 9/9, suites officielles OK.
Backend : Clang/LLVM.
