// Host-side check: is every level beatable? (frame-perfect BFS over the exact game rules)
//   gcc -O2 -o solve test/solve.c source/game.c source/atari_data.c && ./solve [first last]
#include "../source/game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned long long u64;
#define HBITS 25
static u64 *tab;
static int add(u64 v)
{
    u64 h = (v * 0x9E3779B97F4A7C15ull) >> (64 - HBITS);
    v += 1;                                   // 0 = empty
    while (tab[h]) { if (tab[h] == v) return 0; h = (h + 1) & ((1ull << HBITS) - 1); }
    tab[h] = v;
    return 1;
}
int main(int argc, char **argv)
{
    int from = argc > 2 ? atoi(argv[1]) : 0, to = argc > 2 ? atoi(argv[2]) : NUM_LEVELS - 1;
    tab = calloc(1ull << HBITS, sizeof *tab);
    size_t cap = 1 << 24;
    u64 *q = malloc(cap * sizeof *q);
    unsigned *d = malloc(cap * sizeof *d);
    int bad = 0;
    for (int lv = from; lv <= to; lv++) {
        memset(tab, 0, (1ull << HBITS) * sizeof *tab);
        static Game g, n;
        game_load(&g, lv);
        size_t head = 0, tail = 0;
        q[tail] = game_pack(&g); d[tail++] = 0; add(q[0]);
        int best = -1;
        while (head < tail && best < 0) {
            u64 s = q[head]; unsigned dd = d[head++];
            game_unpack(&g, lv, s);
            for (int in = 0; in < 16; in++) {
                if ((in & 1) && (in & 2)) continue;
                n = g;
                game_step(&n, in);
                if (n.state == ST_WIN) { best = dd + 1; break; }
                if (n.state == ST_DEAD || n.gotoLevel >= 0) continue;
                u64 p = game_pack(&n);
                if (!add(p)) continue;
                if (tail >= cap) { fprintf(stderr, "queue overflow\n"); exit(2); }
                q[tail] = p; d[tail++] = dd + 1;
            }
        }
        if (best >= 0) printf("level %2d OK   %5d frames (%.1fs)  %zu states\n", lv + 1, best, best / 60.0, tail);
        else { printf("level %2d *** NOT SOLVED *** (%zu states)\n", lv + 1, tail); bad = 1; }
        fflush(stdout);
    }
    return bad;
}
