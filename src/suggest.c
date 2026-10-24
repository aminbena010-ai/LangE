#include "suggest.h"

#include <stdlib.h>
#include <string.h>


/* =========================================================
 * UMBRAL
 *
 * Palabras cortas admiten 1 error; largas, 2. Por encima
 * no se sugiere nada (evita ruido: "a" no sugiere "b").
 * ========================================================= */

static int suggest_threshold(int length)
{
    if (length <= 4)
        return 1;

    if (length <= 10)
        return 2;

    return 3;
}


/* =========================================================
 * LEVENSHTEIN
 *
 * Dos filas de matriz; O(n*m) con poda por umbral.
 * ========================================================= */

int suggest_distance(
    const char *word,
    const char *candidate
)
{
    if (word == NULL || candidate == NULL)
        return -1;

    int n = (int)strlen(word);
    int m = (int)strlen(candidate);

    if (n == 0 || m == 0)
        return -1;

    int threshold = suggest_threshold(n > m ? n : m);

    /* Diferencia de longitud ya supera el umbral. */
    if ((n > m ? n - m : m - n) > threshold)
        return -1;

    int *previous = malloc(sizeof(int) * (m + 1));
    int *current = malloc(sizeof(int) * (m + 1));

    if (previous == NULL || current == NULL) {
        free(previous);
        free(current);
        return -1;
    }

    for (int j = 0; j <= m; j++)
        previous[j] = j;

    for (int i = 1; i <= n; i++) {

        current[0] = i;
        int row_min = current[0];

        for (int j = 1; j <= m; j++) {

            int cost =
                word[i - 1] == candidate[j - 1]
                    ? 0
                    : 1;

            int deletion = previous[j] + 1;
            int insertion = current[j - 1] + 1;
            int replace = previous[j - 1] + cost;

            int best = deletion;

            if (insertion < best)
                best = insertion;

            if (replace < best)
                best = replace;

            current[j] = best;

            if (best < row_min)
                row_min = best;
        }

        /* Poda: la fila ya no puede bajar del umbral. */
        if (row_min > threshold) {
            free(previous);
            free(current);
            return -1;
        }

        int *swap = previous;
        previous = current;
        current = swap;
    }

    int distance = previous[m];

    free(previous);
    free(current);

    if (distance > threshold)
        return -1;

    return distance;
}


/* =========================================================
 * MEJOR CANDIDATO
 * ========================================================= */

const char *suggest_best(
    const char *word,
    const char *const *candidates,
    int count
)
{
    if (word == NULL || candidates == NULL || count <= 0)
        return NULL;

    int word_length = (int)strlen(word);

    if (word_length < 2)
        return NULL;

    const char *best = NULL;
    int best_distance = -1;

    for (int i = 0; i < count; i++) {

        const char *candidate = candidates[i];

        if (candidate == NULL)
            continue;

        /* El candidato exacto no se sugiere. */
        if (strcmp(word, candidate) == 0)
            continue;

        /*
         * Prefijo largo: "impor" -> "import" gana por
         * emparejar aunque la distancia sea 2.
         */
        if (word_length >= 3 &&
            strncmp(word, candidate, (size_t)word_length) == 0)
        {
            if (best == NULL) {
                best = candidate;
                best_distance = 0;
            }

            continue;
        }

        int distance = suggest_distance(word, candidate);

        if (distance < 0)
            continue;

        if (best == NULL || distance < best_distance) {
            best = candidate;
            best_distance = distance;
        }
    }

    return best;
}
