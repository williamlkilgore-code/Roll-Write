/*
 * Roll-Write PRNG - Standalone Random Number Generator
 * Cross-compiled with MinGW for Windows 10/11 compatibility
 * Also works under Wine and Proton on Linux
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VERSION "1.1.0"

/* ============================================================
 * PRNG Algorithms
 * ============================================================ */

/* --- Xorshift128+ --- */
typedef struct {
    unsigned long long s0;
    unsigned long long s1;
} Xorshift128;

void xorshift_seed(Xorshift128 *rng, unsigned long long seed) {
    rng->s0 = seed;
    rng->s1 = seed ^ 0x6C3F2A1B9E7D5C4AULL;
    /* Warm up */
    int i;
    for (i = 0; i < 20; i++) {
        unsigned long long s1 = rng->s0;
        unsigned long long s0 = rng->s1;
        rng->s0 = s0;
        s1 ^= s1 << 23;
        rng->s1 = s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26);
    }
}

unsigned long long xorshift_next(Xorshift128 *rng) {
    unsigned long long s1 = rng->s0;
    unsigned long long s0 = rng->s1;
    rng->s0 = s0;
    s1 ^= s1 << 23;
    rng->s1 = s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26);
    return rng->s1 + s0;
}

/* --- PCG32 --- */
typedef struct {
    unsigned long long state;
    unsigned long long inc;
} PCG32;

void pcg_seed(PCG32 *rng, unsigned long long seed) {
    rng->state = 0;
    rng->inc = (seed << 1) | 1;
    /* Advance once */
    rng->state = rng->state * 6364136223846793005ULL + rng->inc;
    rng->state += seed;
    rng->state = rng->state * 6364136223846793005ULL + rng->inc;
}

unsigned int pcg_next(PCG32 *rng) {
    unsigned long long old = rng->state;
    rng->state = old * 6364136223846793005ULL + rng->inc;
    unsigned int xorshifted = (unsigned int)(((old >> 18) ^ old) >> 27);
    unsigned int rot = (unsigned int)(old >> 59);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

/* --- LCG (Numerical Recipes) --- */
typedef struct {
    unsigned long long state;
} LCG;

void lcg_seed(LCG *rng, unsigned long long seed) {
    rng->state = seed;
}

unsigned int lcg_next(LCG *rng) {
    rng->state = rng->state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned int)(rng->state >> 32);
}

/* ============================================================
 * Unified PRNG interface
 * ============================================================ */

typedef enum { ALG_XORSHIFT, ALG_PCG, ALG_LCG } Algorithm;

typedef struct {
    Algorithm alg;
    Xorshift128 xor_state;
    PCG32       pcg_state;
    LCG         lcg_state;
    unsigned long long seed;
} PRNG;

void prng_init(PRNG *rng, Algorithm alg, unsigned long long seed) {
    rng->alg = alg;
    rng->seed = seed;
    switch (alg) {
        case ALG_XORSHIFT: xorshift_seed(&rng->xor_state, seed); break;
        case ALG_PCG:      pcg_seed(&rng->pcg_state, seed);      break;
        case ALG_LCG:      lcg_seed(&rng->lcg_state, seed);      break;
    }
}

unsigned int prng_next(PRNG *rng) {
    switch (rng->alg) {
        case ALG_XORSHIFT: return (unsigned int)xorshift_next(&rng->xor_state);
        case ALG_PCG:      return pcg_next(&rng->pcg_state);
        case ALG_LCG:      return lcg_next(&rng->lcg_state);
    }
    return 0;
}

/* Return random int in [0, n) */
int prng_intn(PRNG *rng, int n) {
    if (n <= 0) return 0;
    return (int)(prng_next(rng) % (unsigned int)n);
}

const char *prng_name(PRNG *rng) {
    switch (rng->alg) {
        case ALG_XORSHIFT: return "Xorshift128+";
        case ALG_PCG:      return "PCG32";
        case ALG_LCG:      return "LCG";
    }
    return "Unknown";
}

/* ============================================================
 * Features
 * ============================================================ */

void generate_numbers(PRNG *rng, int count, int lo, int hi) {
    int range = hi - lo + 1;
    int i;
    for (i = 0; i < count; i++) {
        printf("%d\n", lo + prng_intn(rng, range));
    }
}

void roll_dice(PRNG *rng, int num_dice, int sides, int modifier, int drop_lowest) {
    int *rolls;
    int i, j, total = 0, tmp;

    rolls = (int *)malloc(num_dice * sizeof(int));
    for (i = 0; i < num_dice; i++) {
        rolls[i] = prng_intn(rng, sides) + 1;
    }

    printf("Rolls: [");
    for (i = 0; i < num_dice; i++) {
        if (i > 0) printf(" ");
        printf("%d", rolls[i]);
    }
    printf("]");

    /* Sort for drop lowest */
    if (drop_lowest > 0 && drop_lowest < num_dice) {
        for (i = 0; i < num_dice - 1; i++) {
            for (j = 0; j < num_dice - i - 1; j++) {
                if (rolls[j] > rolls[j+1]) {
                    tmp = rolls[j];
                    rolls[j] = rolls[j+1];
                    rolls[j+1] = tmp;
                }
            }
        }
        printf(" (dropped %d lowest)", drop_lowest);
        for (i = drop_lowest; i < num_dice; i++) total += rolls[i];
    } else {
        for (i = 0; i < num_dice; i++) total += rolls[i];
    }

    total += modifier;

    if (modifier > 0) printf(" + %d", modifier);
    else if (modifier < 0) printf(" - %d", -modifier);

    printf(" = %d\n", total);
    free(rolls);
}

int parse_dice(const char *notation, int *num_dice, int *sides, int *modifier, int *drop_lowest) {
    const char *p = notation;
    char buf[32];
    int idx = 0;

    *num_dice = 0;
    *sides = 0;
    *modifier = 0;
    *drop_lowest = 0;

    /* Parse number of dice */
    while (*p >= '0' && *p <= '9') buf[idx++] = *p++;
    buf[idx] = '\0';
    *num_dice = idx > 0 ? atoi(buf) : 1;

    if (*p != 'd' && *p != 'D') return -1;
    p++;

    /* Parse sides */
    idx = 0;
    while (*p >= '0' && *p <= '9') buf[idx++] = *p++;
    buf[idx] = '\0';
    if (idx == 0) return -1;
    *sides = atoi(buf);

    /* Check for drop lowest */
    if ((*p == 'd' || *p == 'D') && (*(p+1) == 'l' || *(p+1) == 'L')) {
        p += 2;
        idx = 0;
        while (*p >= '0' && *p <= '9') buf[idx++] = *p++;
        buf[idx] = '\0';
        *drop_lowest = idx > 0 ? atoi(buf) : 1;
    }

    /* Check for modifier */
    if (*p == '+' || *p == '-') {
        int sign = (*p == '+') ? 1 : -1;
        p++;
        idx = 0;
        while (*p >= '0' && *p <= '9') buf[idx++] = *p++;
        buf[idx] = '\0';
        *modifier = atoi(buf) * sign;
    }

    return 0;
}

static const char *suits[] = {"Hearts", "Diamonds", "Clubs", "Spades"};
static const char *ranks[] = {"A","2","3","4","5","6","7","8","9","10","J","Q","K"};

void draw_cards(PRNG *rng, int n) {
    int deck[52], i, j, tmp;
    for (i = 0; i < 52; i++) deck[i] = i;

    /* Fisher-Yates shuffle */
    for (i = 51; i > 0; i--) {
        j = prng_intn(rng, i + 1);
        tmp = deck[i]; deck[i] = deck[j]; deck[j] = tmp;
    }

    if (n > 52) n = 52;
    printf("Cards drawn:\n");
    for (i = 0; i < n; i++) {
        printf("  %s of %s\n", ranks[deck[i] % 13], suits[deck[i] / 13]);
    }
}

void flip_coins(PRNG *rng, int n) {
    int heads = 0, tails = 0, i;
    printf("Flips: ");
    for (i = 0; i < n; i++) {
        if (prng_intn(rng, 2) == 0) {
            printf("H ");
            heads++;
        } else {
            printf("T ");
            tails++;
        }
    }
    printf("\nHeads: %d, Tails: %d\n", heads, tails);
}

void shuffle_list(PRNG *rng, const char *input) {
    char buf[4096];
    char *items[256];
    int count = 0, i, j;
    char *p, *tmp;

    strncpy(buf, input, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    p = strtok(buf, ",");
    while (p && count < 256) {
        while (*p == ' ') p++;
        items[count++] = p;
        p = strtok(NULL, ",");
    }

    /* Fisher-Yates */
    for (i = count - 1; i > 0; i--) {
        j = prng_intn(rng, i + 1);
        tmp = items[i]; items[i] = items[j]; items[j] = tmp;
    }

    printf("Shuffled order:\n");
    for (i = 0; i < count; i++) {
        printf("  %d. %s\n", i + 1, items[i]);
    }
}

static const char pw_chars[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "!@#$%^&*()_+-=[]{}|;:,.<>?";

void generate_password(PRNG *rng, int length) {
    int i, nchars = (int)strlen(pw_chars);
    for (i = 0; i < length; i++) {
        putchar(pw_chars[prng_intn(rng, nchars)]);
    }
    putchar('\n');
}

void generate_uuid(PRNG *rng) {
    unsigned int a = prng_next(rng);
    unsigned int b = prng_next(rng);
    unsigned int c = prng_next(rng);
    unsigned int d = prng_next(rng);
    printf("%08x-%04x-4%03x-%04x-%08x%04x\n",
           a, b & 0xFFFF, c & 0x0FFF,
           (d & 0x3FFF) | 0x8000,
           prng_next(rng), prng_next(rng) & 0xFFFF);
}

/* ============================================================
 * Interactive mode
 * ============================================================ */

void interactive_mode(PRNG *rng) {
    char line[512], cmd[64], arg[256];
    int n, lo, hi;

    printf("Roll-Write Interactive Mode\n");
    printf("Using %s algorithm (seed: %llu)\n", prng_name(rng), rng->seed);
    printf("Type 'help' for commands, 'quit' to exit\n\n");

    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;

        /* Strip newline */
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        if (sscanf(line, "%63s %255[^\n]", cmd, arg) < 1) continue;

        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "q") == 0) {
            printf("Goodbye!\n");
            break;
        }

        if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
            printf("Commands:\n"
                   "  roll NdS[+/-M]    Roll dice (e.g., roll 2d6, roll 1d20+5)\n"
                   "  number [min] [max] Random number in range (default 1-100)\n"
                   "  flip [n]          Flip n coins (default 1)\n"
                   "  card [n]          Draw n cards (default 1)\n"
                   "  password [len]    Generate a password (default 16)\n"
                   "  uuid              Generate a UUID\n"
                   "  algo [name]       Switch algorithm (xorshift, pcg, lcg)\n"
                   "  help              Show this help\n"
                   "  quit              Exit\n");
            continue;
        }

        if (strcmp(cmd, "roll") == 0 || strcmp(cmd, "r") == 0) {
            int nd, sd, mod, dl;
            if (parse_dice(arg, &nd, &sd, &mod, &dl) == 0)
                roll_dice(rng, nd, sd, mod, dl);
            else
                printf("Usage: roll NdS (e.g., roll 2d6)\n");
            continue;
        }

        if (strcmp(cmd, "number") == 0 || strcmp(cmd, "num") == 0 || strcmp(cmd, "n") == 0) {
            lo = 1; hi = 100;
            sscanf(arg, "%d %d", &lo, &hi);
            generate_numbers(rng, 1, lo, hi);
            continue;
        }

        if (strcmp(cmd, "flip") == 0 || strcmp(cmd, "coin") == 0 || strcmp(cmd, "f") == 0) {
            n = 1;
            sscanf(arg, "%d", &n);
            flip_coins(rng, n > 0 ? n : 1);
            continue;
        }

        if (strcmp(cmd, "card") == 0 || strcmp(cmd, "cards") == 0 || strcmp(cmd, "c") == 0) {
            n = 1;
            sscanf(arg, "%d", &n);
            draw_cards(rng, n > 0 ? n : 1);
            continue;
        }

        if (strcmp(cmd, "password") == 0 || strcmp(cmd, "pass") == 0 || strcmp(cmd, "pw") == 0) {
            n = 16;
            sscanf(arg, "%d", &n);
            generate_password(rng, n > 0 ? n : 16);
            continue;
        }

        if (strcmp(cmd, "uuid") == 0) {
            generate_uuid(rng);
            continue;
        }

        if (strcmp(cmd, "algo") == 0 || strcmp(cmd, "algorithm") == 0) {
            if (strcmp(arg, "xorshift") == 0)      prng_init(rng, ALG_XORSHIFT, rng->seed);
            else if (strcmp(arg, "pcg") == 0)       prng_init(rng, ALG_PCG, rng->seed);
            else if (strcmp(arg, "lcg") == 0)       prng_init(rng, ALG_LCG, rng->seed);
            else { printf("Available: xorshift, pcg, lcg\n"); continue; }
            printf("Switched to %s\n", prng_name(rng));
            continue;
        }

        /* Try parsing as dice notation directly */
        {
            int nd, sd, mod, dl;
            if (parse_dice(cmd, &nd, &sd, &mod, &dl) == 0) {
                roll_dice(rng, nd, sd, mod, dl);
                continue;
            }
        }

        printf("Unknown command. Type 'help' for available commands.\n");
    }
}

/* ============================================================
 * Argument parsing and main
 * ============================================================ */

void print_help(void) {
    printf(
        "Roll-Write PRNG v%s - Standalone Random Number Generator\n"
        "=========================================================\n\n"
        "Usage: rollwrite [options]\n\n"
        "Options:\n"
        "  --algo NAME    Algorithm: xorshift (default), pcg, lcg\n"
        "  --seed N       Seed value (0 = use current time)\n"
        "  --n N          Number of values to generate (default 1)\n"
        "  --min N        Minimum value (default 1)\n"
        "  --max N        Maximum value (default 100)\n\n"
        "Special Modes:\n"
        "  --dice EXPR    Roll dice (e.g., 2d6, 1d20+5, 4d6dl1)\n"
        "  --cards N      Draw N cards from a deck\n"
        "  --shuffle LIST Shuffle comma-separated list\n"
        "  --coins N      Flip N coins\n"
        "  --uuid N       Generate N UUID-like strings\n"
        "  --password N   Generate N passwords\n"
        "  --passlen N    Password length (default 16)\n"
        "  -i             Interactive mode\n\n"
        "Examples:\n"
        "  rollwrite --n 5                     Five random numbers 1-100\n"
        "  rollwrite --dice 2d6+3              Roll 2d6 and add 3\n"
        "  rollwrite --dice 4d6dl1             Roll 4d6, drop lowest\n"
        "  rollwrite --cards 5                 Draw 5 cards\n"
        "  rollwrite --coins 10                Flip 10 coins\n"
        "  rollwrite --password 3 --passlen 20 Three 20-char passwords\n"
        "  rollwrite --algo pcg --seed 42      Use PCG with seed 42\n"
        "  rollwrite -i                        Interactive mode\n",
        VERSION
    );
}

int main(int argc, char **argv) {
    Algorithm alg = ALG_XORSHIFT;
    unsigned long long seed_val = 0;
    int count = 1, lo = 1, hi = 100, passlen = 16;
    int do_interactive = 0;
    char *dice_expr = NULL;
    int card_count = 0, coin_count = 0;
    char *shuffle_str = NULL;
    int uuid_count = 0, pass_count = 0;
    PRNG rng;
    int i;

    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("Roll-Write PRNG v%s\n", VERSION);
            return 0;
        }
        if (strcmp(argv[i], "-i") == 0) { do_interactive = 1; continue; }

        if (i + 1 < argc) {
            if (strcmp(argv[i], "--algo") == 0) {
                i++;
                if (strcmp(argv[i], "pcg") == 0) alg = ALG_PCG;
                else if (strcmp(argv[i], "lcg") == 0) alg = ALG_LCG;
                else alg = ALG_XORSHIFT;
                continue;
            }
            if (strcmp(argv[i], "--seed") == 0)    { seed_val = strtoull(argv[++i], NULL, 10); continue; }
            if (strcmp(argv[i], "--n") == 0)        { count = atoi(argv[++i]); continue; }
            if (strcmp(argv[i], "--min") == 0)      { lo = atoi(argv[++i]); continue; }
            if (strcmp(argv[i], "--max") == 0)      { hi = atoi(argv[++i]); continue; }
            if (strcmp(argv[i], "--dice") == 0)     { dice_expr = argv[++i]; continue; }
            if (strcmp(argv[i], "--cards") == 0)    { card_count = atoi(argv[++i]); continue; }
            if (strcmp(argv[i], "--shuffle") == 0)  { shuffle_str = argv[++i]; continue; }
            if (strcmp(argv[i], "--coins") == 0)    { coin_count = atoi(argv[++i]); continue; }
            if (strcmp(argv[i], "--uuid") == 0)     { uuid_count = atoi(argv[++i]); continue; }
            if (strcmp(argv[i], "--password") == 0) { pass_count = atoi(argv[++i]); continue; }
            if (strcmp(argv[i], "--passlen") == 0)  { passlen = atoi(argv[++i]); continue; }
        }
    }

    /* Seed */
    if (seed_val == 0) seed_val = (unsigned long long)time(NULL);
    prng_init(&rng, alg, seed_val);

    /* Dispatch */
    if (do_interactive) { interactive_mode(&rng); return 0; }

    if (dice_expr) {
        int nd, sd, mod, dl;
        for (i = 0; i < count; i++) {
            if (parse_dice(dice_expr, &nd, &sd, &mod, &dl) == 0)
                roll_dice(&rng, nd, sd, mod, dl);
            else
                printf("Invalid dice notation: %s\n", dice_expr);
        }
        return 0;
    }
    if (card_count > 0)    { draw_cards(&rng, card_count); return 0; }
    if (shuffle_str)       { shuffle_list(&rng, shuffle_str); return 0; }
    if (coin_count > 0)    { flip_coins(&rng, coin_count); return 0; }
    if (uuid_count > 0)    { for (i = 0; i < uuid_count; i++) generate_uuid(&rng); return 0; }
    if (pass_count > 0)    { for (i = 0; i < pass_count; i++) generate_password(&rng, passlen); return 0; }

    /* Default: generate numbers */
    if (lo > hi) { int t = lo; lo = hi; hi = t; }
    generate_numbers(&rng, count, lo, hi);

    return 0;
}
