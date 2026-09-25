/* fcperft: count leaf nodes to check the generator and measure its speed.
 *
 *     ./fcperft <depth> [fen]
 *
 * It also serves as a minimal example of walking the move tree from C.
 */
#define _POSIX_C_SOURCE 199309L /* clock_gettime under -std=c11 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "fastchess.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <depth> [fen]\n", argv[0]);
        return 2;
    }
    int depth = atoi(argv[1]);
    const char *fen = argc > 2 ? argv[2] : FC_START_FEN;
    fc_position pos;
    if (fc_set_fen(&pos, fen) != 0) {
        fprintf(stderr, "invalid FEN: %s\n", fen);
        return 2;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    uint64_t nodes = fc_perft(&pos, depth);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double seconds = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("perft(%d) = %llu in %.2fs (%.0f nodes/s)\n", depth, (unsigned long long)nodes,
           seconds, seconds > 0 ? (double)nodes / seconds : 0.0);
    return 0;
}
