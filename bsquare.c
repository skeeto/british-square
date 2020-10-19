#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _MSC_VER
#  define POPCOUNT(x) __popcnt(x)
#else
#  define POPCOUNT(x) __builtin_popcountl(x)
#endif

// The full game state is represented by a 56-bit bitboard. The bottom 25 bits
// are the first players pieces, the next 25 bits are the second player's
// peices, and the next 6 bits are the 1-indexed turn number.
//
// XXXXXXXXTTTTTTBBBBBBBBBBBBBBBBBBBBBBBBBAAAAAAAAAAAAAAAAAAAAAAAAA
//
// For optimization purposes, some operations require a "mask" bitboard
// representing valid moves. A mask does not store critical state and
// technically can be derive from the board.
//
// Zero is an invalid board value, representing a "null" board rather than an
// empty board. Instead, INIT is the empty board and mask.

// Empty state for boards and masks.
#define INIT (1ULL<<50)

// Place a piece at the given index, returning the next board.
static uint64_t
place(uint64_t b, int i)
{
    uint64_t turn = b >> 50;
    int who = (turn - 1) % 2;
    uint64_t state = b & 0x3ffffffffffff;
    uint64_t bit = 1ULL << (who*25 + i);
    return (turn+1)<<50 | state | bit;
}

// Pass the current player's turn without placing a piece, returning the next
// board or mask.
static uint64_t
pass(uint64_t b)
{
    uint64_t turn = b >> 50;
    uint64_t state = b & 0x3ffffffffffff;
    return (turn+1)<<50 | state;
}

// Place a piece at the given index, returning the next mask.
static uint64_t
mask(uint64_t m, int i)
{
    static const uint64_t blocks[] = {
        0x0000023, 0x0000047, 0x000008e, 0x000011c, 0x0000218,
        0x0000461, 0x00008e2, 0x00011c4, 0x0002388, 0x0004310,
        0x0008c20, 0x0011c40, 0x0023880, 0x0047100, 0x0086200,
        0x0118400, 0x0238800, 0x0471000, 0x08e2000, 0x10c4000,
        0x0308000, 0x0710000, 0x0e20000, 0x1c40000, 0x1880000,
    };
    uint64_t turn = m >> 50;
    int who = (turn - 1) % 2;
    uint64_t state = m & 0x3ffffffffffff;
    return (turn+1)<<50 | state | blocks[i]<<(!who*25) | 1ULL<<(who*25 + i);
}

// Return the 0-indexed turn number from a board or mask.
static int
turn(uint64_t b)
{
    return (b >> 50) - 1;
}

// Derive a mask from a board.
static uint64_t
derive(uint64_t b)
{
    int ns[2] = {0, 0};
    int moves[2][25];
    for (int i = 0; i < 25; i++) {
        if (b >>  i     & 1) moves[0][ns[0]++] = i;
        if (b >> (i+25) & 1) moves[1][ns[1]++] = i;
    }

    uint64_t m = INIT;
    for (int i = 0; turn(m) < turn(b); i++) {
        m = i/2 < ns[i%2] ? mask(m, moves[i%2][i/2]) : pass(m);
    }
    return m;
}

// Transpose a board or mask along (flip along the diagonal).
static uint64_t
transpose(uint64_t b)
{
    return ((b >> 16) & 0x00000020000010) |
           ((b >> 12) & 0x00000410000208) |
           ((b >>  8) & 0x00008208004104) |
           ((b >>  4) & 0x00104104082082) |
           ((b >>  0) & 0xfe082083041041) |
           ((b <<  4) & 0x01041040820820) |
           ((b <<  8) & 0x00820800410400) |
           ((b << 12) & 0x00410000208000) |
           ((b << 16) & 0x00200000100000);
}

// Flip a board or mask vertically.
static uint64_t
flipv(uint64_t b)
{
    return ((b >> 20) & 0x0000003e00001f) |
           ((b >> 10) & 0x000007c00003e0) |
           ((b >>  0) & 0xfc00f800007c00) |
           ((b << 10) & 0x001f00000f8000) |
           ((b << 20) & 0x03e00001f00000);
}

// Return the canonical board rotation/reflection.
static uint64_t
canonicalize(uint64_t b)
{
    uint64_t c = b;
    b = transpose(b); c = c < b ? c : b;
    b = flipv(b);     c = c < b ? c : b;
    b = transpose(b); c = c < b ? c : b;
    b = flipv(b);     c = c < b ? c : b;
    b = transpose(b); c = c < b ? c : b;
    b = flipv(b);     c = c < b ? c : b;
    b = transpose(b); c = c < b ? c : b;
    return c;
}

// Return true if the given move index is valid.
static int
valid(uint64_t m, int i)
{
    uint64_t turn = m >> 50;
    int who = (turn - 1) % 2;
    return (turn == 1) ? i != 12 : !(m>>(who*25 + i) & 1);
}

// Return true if current player has no legal moves.
static int
nomoves(uint64_t b, uint64_t m)
{
    int who = ((b >> 50) - 1) % 2;
    return ((b >> (who*25) | m >> (who*25)) & 0x1ffffff) == 0x1ffffff;
}

// Return true if the game has ended: no more legal moves.
static int
iscomplete(uint64_t b, uint64_t m)
{
    return ((b >> 25 | m >> 25) & 0x1ffffff) == 0x1ffffff &&
           ((b >>  0 | m >>  0) & 0x1ffffff) == 0x1ffffff;
}

// Print a board with mask to the terminal. The mask is optional.
static void
print(uint64_t b, uint64_t m)
{
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            int i = y*5 + x;
            int p0 = b>>i      & 1;
            int p1 = b>>(i+25) & 1;
            int x0 = m>>(i+25) & 1;
            int x1 = m>>i      & 1;
            char *c = ".";
            if (p0) {
                c = "\x1b[94mX\x1b[0m";
            } else if (p1) {
                c = "\x1b[91mX\x1b[0m";
            } else if (x0 && x1) {
                c = "\x1b[93m~\x1b[0m";
            } else if (x0) {
                c = "\x1b[94m~\x1b[0m";
            } else if (x1) {
                c = "\x1b[91m~\x1b[0m";
            }
            fputs(c, stdout);
        }
        putchar('\n');
    }
    putchar('\n');
}

// Game tree exploration / evaluation

#if defined(TALLY)

// This slot_t variant tallies the total wins / losses / ties rooted at the
// given node. It's not effective for finding the best move.
typedef struct {
    uint64_t board;
    int64_t p1_wins;
    int64_t p2_wins;
    int64_t ties;
} slot_t;

static slot_t
init(uint64_t b)
{
    slot_t s = {
        .board = b,
        .p1_wins = 0,
        .p2_wins = 0,
        .ties = 0,
    };
    return s;
}

static uint64_t
getboard(slot_t s)
{
    return s.board;
}

static slot_t
setboard(slot_t s, uint64_t b)
{
    s.board = b;
    return s;
}

static slot_t
score(uint64_t b)
{
    int p0 = POPCOUNT(b     & 0x1ffffff);
    int p1 = POPCOUNT(b>>25 & 0x1ffffff);
    slot_t s = {
        .board = b,
        .p1_wins = p0 > p1,
        .p2_wins = p1 > p0,
        .ties    = p1 == p0,
    };
    return s;
}

static slot_t
combine(slot_t s0, slot_t s1)
{
    s0.p1_wins += s1.p1_wins;
    s0.p2_wins += s1.p2_wins;
    s0.ties    += s1.ties;
    return s0;
}

static int
compare(slot_t s0, slot_t s1)
{
    // No reason to compare tally slots.
    (void)s0; (void)s1;
    return 0;
}

static void
display(slot_t s)
{
    long long p1 = s.p1_wins;
    long long p2 = s.p2_wins;
    long long ts = s.ties;
    long long total = p1 + p2 + ts;
    printf("  P1  =% 17lld (%.17f %%)\n", p1, p1*100.0/total);
    printf("  P2  =% 17lld (%.17f %%)\n", p2, p2*100.0/total);
    printf("  TIE =% 17lld (%.17f %%)\n", ts, ts*100.0/total);
}

#else // MINIMAX

// This slot_t variant implements the minimax algorithm, tracking only the
// propagated minimax score. Using this minimax table allows for perfect play.
//
// The score is stored in 6 bits just above the 56-bit bitboard. It is biased
// by 25 in order to simplify storing negative values.
typedef uint64_t slot_t;

static slot_t
init(uint64_t b)
{
    int score = turn(b)%2 ? +25 : -25;
    return (uint64_t)(score + 25)<<56 | b;
}

static uint64_t
getboard(slot_t s)
{
    return s & 0xffffffffffffff;
}

static slot_t
setboard(slot_t s, uint64_t b)
{
    return (s & 0x3f00000000000000) | b;
}

static int
getscore(slot_t s)
{
    return (int)(s>>56) - 25;
}

static slot_t
score(uint64_t b)
{
    int p0 = POPCOUNT(b     & 0x1ffffff);
    int p1 = POPCOUNT(b>>25 & 0x1ffffff);
    int score = p0 - p1;
    return (uint64_t)(score + 25)<<56 | b;
}

static slot_t
combine(slot_t s0, slot_t s1)
{
    int v0 = getscore(s0);
    int v1 = getscore(s1);
    if (turn(getboard(s0)) % 2) {
        return v0 < v1 ? s0 : s1; // min
    } else {
        return v0 > v1 ? s0 : s1; // max
    }
}

static int
compare(slot_t s0, slot_t s1)
{
    int v0 = getscore(s0);
    int v1 = getscore(s1);
    if (turn(getboard(s0)) % 2) {
        return v0 - v1;
    } else {
        return v1 - v0;
    }
}

static void
display(slot_t s)
{
    // Nothing to display.
    (void)s;
}

#endif

// Initialize a slot from a canonical board.
static slot_t init(uint64_t);

// Get the board belonging to this slot.
static uint64_t getboard(slot_t);

// Set the board in the slot to a particular canonical board.
static slot_t setboard(slot_t, uint64_t);

// Score a completed board.
static slot_t score(uint64_t);

// Combine (add, etc.) two results into a single result.
static slot_t combine(slot_t, slot_t);

// Compare results at the given board, returning less than zero, zero, or
// greater than zero.
static int compare(slot_t, slot_t);

// Display the result in a meaningful way to the user.
static void display(slot_t r);

// Hash table containing the entire canonical game tree.
static slot_t table[1L<<24];
static size_t table_len = 0;

// Lookup the slot for a canonical board.
static slot_t *
lookup(uint64_t b)
{
    size_t n = sizeof(table) / sizeof(*table);
    uint64_t h = b;
    h *= 0xcca1cee435c5048f;
    h ^= h >> 40;
    for (size_t i = h % n; ; i = (i + 1) % n) {
        uint64_t k = getboard(table[i]);
        if (!k || k == b) {
            return table + i;
        }
    }
}

// Tally all outcomes rooted at the given board with mask. Note: The board
// embedded in the returned slot is the canonical board.
static slot_t
eval(uint64_t b, uint64_t m)
{
    uint64_t b0 = canonicalize(b);
    slot_t *slot = lookup(b0);
    if (!getboard(*slot)) {
        table_len++;
        assert(table_len < sizeof(table)/sizeof(*table)); // too small?
        *slot = init(b0);
    } else {
        return *slot;
    }

    if (iscomplete(b, m)) {
        return (*slot = score(b0));
    }

    if (nomoves(b, m)) {
        slot_t s = eval(pass(b), pass(m));
        s = setboard(s, b0);
        return (*slot = s);
    }

    slot_t s = init(b0);
    for (int i = 0; i < 5*5; i++) {
        if (valid(m, i)) {
            slot_t tmp = eval(place(b, i), mask(m, i));
            tmp = setboard(tmp, b0);
            s = combine(s, tmp);
        }
    }
    return (*slot = s);
}

static int
suggest(uint64_t b, uint64_t m, int *moves)
{
    int n = 0;
    slot_t best = init(b);
    for (int i = 0; i < 5*5; i++) {
        if (valid(m, i)) {
            slot_t s = eval(place(b, i), mask(m, i));
            s = setboard(s, b);
            int c = compare(best, s);
            if (c > 0) {
                best = s;
                moves[0] = i;
                n = 1;
            } else if (c == 0) {
                moves[n++] = i;
            }
        }
    }
    return n;
}

#ifndef TALLY
static void
minimax_show(uint64_t b, uint64_t m)
{
    for (int i = 0; i < 5*5; i++) {
        if (valid(m, i)) {
            int s = getscore(eval(place(b, i), mask(m, i)));
            if (s > 0) {
                printf("\x1b[94m%x\x1b[0m", s);
            } else if (s < 0) {
                printf("\x1b[91m%x\x1b[0m", -s);
            } else {
                putchar('0');
            }
        } else {
            putchar('-');
        }
        if (i % 5 == 4) {
            putchar('\n');
        }
    }
    putchar('\n');
}
#endif

static void
enable_color(void)
{
#ifdef _WIN32
    { /* Set stdin/stdout to binary mode. */
        int _setmode(int, int);
        _setmode(0, 0x8000);
        _setmode(1, 0x8000);
    }
    { /* Best effort enable ANSI escape processing. */
        void *GetStdHandle(unsigned);
        int GetConsoleMode(void *, unsigned *);
        int SetConsoleMode(void *, unsigned);
        void *handle;
        unsigned mode;
        handle = GetStdHandle(-11); /* STD_OUTPUT_HANDLE */
        if (GetConsoleMode(handle, &mode)) {
            mode |= 0x0004; /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */
            SetConsoleMode(handle, mode); /* ignore errors */
        }
    }
#endif
}

// Use a simple heuristic to guess at good moves.
static int
heuristic(uint64_t b, uint64_t m, int *moves)
{
    // This heuristic returns the moves that claim the most space.
    // Note: This is not actually a good strategy.
    int n = 0;
    int best = 0;
    int who = turn(b) % 2;
    for (int i = 0; i < 25; i++) {
        if (valid(m, i)) {
            uint64_t t = mask(m, i);
            long a = m>>(!who*25) & 0x1ffffff;
            long b = t>>(!who*25) & 0x1ffffff;
            int v = POPCOUNT(b) - POPCOUNT(a);
            if (v > best) {
                n = 1;
                moves[0] = i;
                best = v;
            } else if (v == best) {
                moves[n++] = i;
            }
        }
    }
    return n;
}

// Convert a list of positions to a bitboard.
static uint32_t
moves2bits(int *moves, int n)
{
    uint32_t b = 0;
    for (int i = 0; i < n; i++) {
        b |= 1L << moves[i];
    }
    return b;
}

// Test the heuristic() function against perfect play.
static long
test_heuristic(void)
{
    long fails = 0;
    size_t n = sizeof(table)/sizeof(*table);
    for (size_t i = 0; i < n; i++) {
        uint64_t b = getboard(table[i]);
        if (!b) continue;

        uint64_t m = derive(b);
        if (nomoves(b, m)) continue;

        int smoves[25];
        int sn = suggest(b, m, smoves);
        int hmoves[25];
        int hn = heuristic(b, m, hmoves);
        uint32_t goal = moves2bits(smoves, sn);
        uint32_t heur = moves2bits(hmoves, hn);
        if ((goal | heur) != goal) {
            fputs("perfect:", stdout);
            for (int j = 0; j < sn; j++) printf(" %d", smoves[j]+1);
            putchar('\n');
            fputs("heuristic:", stdout);
            for (int j = 0; j < hn; j++) printf(" %d", hmoves[j]+1);
            putchar('\n');
            print(b, m);
            fails++;
        }
    }
    return fails;
}

int
main(void)
{
    enable_color();

    display(eval(INIT, INIT));
    printf("Table entries: %zu (%.3f MB)\n",
           table_len, sizeof(*table)*table_len/(1024.0*1024.0));

#if defined(TEST_HEURISTIC)
    long fails = test_heuristic();
    if (fails) {
        printf("heuristic fails in %ld cases\n", fails);
    }
#endif

#if !defined(TALLY) && !defined(BENCHMARK)
    long p1_wins = 0;
    long p2_wins = 0;
    long ties = 0;
    for (size_t i = 0; i < sizeof(table)/sizeof(*table); i++) {
        uint64_t b = getboard(table[i]);
        if (b) {
            uint64_t m = derive(b);
            if (iscomplete(b, m)) {
                int s = getscore(table[i]);
                if (s > 0) {
                    p1_wins++;
                } else if (s < 0) {
                    p2_wins++;
                } else {
                    ties++;
                }
            }
        }
    }
    printf("Total endings: %ld\n", p1_wins + p2_wins + ties);
    printf("Player 1 wins: %ld\n", p1_wins);
    printf("Player 2 wins: %ld\n", p2_wins);

    uint64_t b = INIT;
    uint64_t m = INIT;
    for (int turn = 0; ; turn++) {
        minimax_show(b, m);
        display(eval(b, m));
        print(b, m);

        if (turn == 0) {
            puts("(Positions are 1-25, 0 passes, -1 restarts.)");
        }

        if (iscomplete(b, m)) {
            printf("Game over! Score: %d\n", getscore(score(b)));
        } else {
            int moves[25];
            int n = suggest(b, m, moves);
            if (n > 0) {
                printf("Suggestion%s:", n == 1 ? "" : "s");
                for (int i = 0; i < n; i++) {
                    printf(" %d", moves[i] + 1);
                }
                putchar('\n');
            } else {
                printf("Suggestion: 0 (pass)\n");
            }
        }

        for (;;) {
            int i;
            fputs(">>> ", stdout); fflush(stdout);
            if (scanf("%d", &i) != 1) {
                return 1;
            }
            if (i == -1) {
                b = INIT;
                m = INIT;
                turn = -1;
                break;
            } else if (i == 0) {
                b = pass(b);
                m = pass(m);
                break;
            } else if (valid(m, i-1)) {
                b = place(b, i-1);
                m = mask(m, i-1);
                break;
            }
            puts("INVALID");
        }
    }
#endif
}
