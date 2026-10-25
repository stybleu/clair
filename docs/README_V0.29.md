# Clair V0.29

## Boucles alignées

Sur le nouveau téléphone (cœur à 3,73 GHz), `axpy` et `while` restaient 12 % derrière le C, même
avec des programmes plus longs. Pourtant la boucle chaude était la même, instruction par
instruction, dans Clair et dans le C (vérifié avec Clang 21 sur le téléphone) :

```text
ldp q2, q3, [x17, #-16]
subs x0, x0, #4
ldp q4, q5, [x16, #-16]
add x17, x17, #32
fmul v2.2d, v2.2d, v0.2d
fmul v3.2d, v3.2d, v0.2d
fadd v2.2d, v2.2d, v4.2d
fadd v3.2d, v3.2d, v5.2d
stp q2, q3, [x16, #-16]
add x16, x16, #32
b.ne .LBB0_17
```

La différence venait de l'endroit où cette boucle tombe dans le programme. Un processeur aussi
large lit les instructions par blocs de 64 octets : une boucle de 44 octets à cheval sur deux
blocs coûte un peu plus à chaque tour. Ni Clair ni le C ne demandaient d'alignement, c'était
donc le hasard de la place du code.

Mesure sur le téléphone (temps médian de 5 lancements sur le même cœur) :

| | sans alignement | boucles alignées |
|---|---|---|
| Clair | 0,202 s | 0,184 s |
| C     | 0,177 s | 0,176 s |

Clair compile maintenant avec `-falign-loops=64` : chaque boucle commence au début d'un bloc de
64 octets. Le rapport compile le C écrit à la main avec la même option, pour que la comparaison
reste juste.
