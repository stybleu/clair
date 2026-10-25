# Clair V0.41

## `absolue { }` : la mémoire comme en C, sans vérification

```text
absolue def axpy(a: float32, x: ptr[float32], y: ptr[float32], n: int) -> int {
    for i in range(n) {
        y[i] += a * x[i]
    }
    return 0
}

def main() -> int {
    let xs: list[float32] = [1.0, 2.0, 3.0, 4.0]
    absolue {
        let p: ptr[float32] = address(xs)        // les nombres de la liste, sans copie
        let tampon: ptr[float32] = alloc(1024, 64)  // 1024 nombres à zéro, alignés sur 64 octets
        fill(tampon, 1.0, 1024)
        axpy(0.5, p, tampon, 4)
        copy(p, tampon, 4)
        free(tampon)
    }
    print(xs[0])                                 // 1.5
    return 0
}
```

Partout ailleurs, Clair vérifie chaque indice. Dans un bloc `absolue { }`, on peut prendre la
main comme en C : un `ptr[T]` est l'adresse d'un nombre en mémoire, `p[i]` lit et écrit sans
aucune vérification. C'est fait pour les noyaux de calcul déjà testés, les tampons alignés,
les octets d'un fichier ou d'une image.

### Ce qu'on peut écrire dans `absolue { }`

| écriture                    | ce que ça fait                                                      |
|-----------------------------|---------------------------------------------------------------------|
| `let p: ptr[float] = ...`   | une adresse de `float` (aussi `int`, `float32`, `u8` .. `u64`, `i8` .. `i32`) |
| `p[i]`, `p[i] = v`, `p[i] += v` | lire, écrire, sans vérification                                 |
| `p + k`, `p - k`            | l'adresse `k` nombres plus loin (ou plus tôt)                       |
| `p == q`, `p != q`          | deux adresses identiques ?                                          |
| `address(xs)`               | les nombres d'une liste ou d'une matrice (rangée après rangée), sans copie |
| `alloc(n)`                  | `n` nombres à zéro ; le type vient de la variable : `let p: ptr[u8] = alloc(n)` |
| `alloc(n, 64)`              | pareil, la première adresse alignée sur 64 octets (une puissance de 2) |
| `free(p)`                   | rend la mémoire d'un `alloc`                                        |
| `copy(dst, src, n)`         | copie `n` nombres, même quand les deux zones se recouvrent          |
| `fill(p, v, n)`             | met `v` dans `n` nombres                                            |
| `bytes(p)`                  | les mêmes octets vus comme des `u8` : `ptr[u8]`                     |

Une fonction qui prend ou rend un `ptr` s'écrit `absolue def` ; elle ne peut être appelée que
dans un bloc `absolue { }` (ou une autre `absolue def`).

### Ce que Clair vérifie encore

Clair refuse à la compilation ce qui ne peut pas bien finir :

- un `ptr` ou un `address()` hors d'un bloc `absolue` :
  `a ptr variable is only allowed inside an absolue { } block` ;
- une `absolue def` appelée dehors, une fonction ordinaire ou un champ de classe de type `ptr` ;
- `xs.append(...)` ou `xs = ...` dans le bloc qui a pris `address(xs)` : la liste pourrait
  changer de place en mémoire et `p` montrerait l'ancienne ;
- `alloc()` sans type à donner, `p + 1.5`, `copy` entre deux types différents, `fill` avec une
  valeur d'un autre type.

Et au moment du calcul : `alloc()` d'une taille négative ou trop grande, un alignement qui n'est
pas une puissance de 2, `copy()` d'un nombre négatif, plus de mémoire. Tout le reste est à toi :
comme en C, `p[1000]` dans un tampon de 10 nombres, un `free` oublié ou fait deux fois, un `ptr`
vers une liste qui n'existe plus ne sont pas détectés.

### Le C généré

C'est le C qu'on écrirait à la main :

```c
static int64_t clair_fn_axpy(float a, float * x, float * y, int64_t n) {
    for (int64_t i = 0; i < n; i++) {
        y[i] = y[i] + a * x[i];
    }
    return 0;
}
...
    {   /* absolue: no checks in here */
        float * p = ((float *)xs->data);
        float * tampon = (float *)clair_rt_alloc(1024, sizeof(float), 64);
```

Clang vectorise ces boucles comme du C écrit à la main. Les calculs gardent leur ordre : une
boucle `absolue` donne le même résultat, au bit près, que la même boucle écrite avec des listes.

## `float32` : `print` écrit `500` et non plus `5e+02`

`print` d'un `float32` rond écrivait `5e+02` : il écrit maintenant `500`, comme pour un `float`.

## Tests

- `absolue.cl` : `address()` d'une liste et d'une matrice, lecture et écriture par `p[i]`,
  `p + k`, `p - k`, `==`, `bytes()`, `alloc` aligné, `fill`, `copy` qui se recouvrent dans les
  deux sens, entiers machine, `absolue def` qui rend un `ptr`, produit de matrices et couche
  dense écrits avec des `ptr` ; sortie identique à Python (NumPy pour les `float32`) ;
- `alloc_negative_runtime.cl`, `alloc_alignment_runtime.cl`, `copy_negative_runtime.cl` ;
- `invalid_ptr_outside.cl`, `invalid_absolue_call.cl`, `invalid_absolue_append.cl`,
  `invalid_absolue_replace.cl`, `invalid_alloc_type.cl`, `invalid_ptr_param.cl`,
  `invalid_ptr_field.cl`, `invalid_ptr_float_offset.cl` ;
- 400 programmes tirés au hasard (`int`, `float`, `float32`, `u8`, `u16`, `i32` ; `address`
  ou `alloc` aligné, écritures, `copy` qui se recouvrent, `fill`, `p + k`, `bytes`) : sortie
  identique à Python ; un tiers passés sans erreur d'AddressSanitizer,
  d'UndefinedBehaviorSanitizer ni de fuite de mémoire.
