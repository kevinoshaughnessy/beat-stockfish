/* fastchess: bitboard legal-move generation. See fastchess.h. */
#include "fastchess.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { N, NE, E, SE, S, SW, W, NW };
static const int DIR_RANK[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int DIR_FILE[8] = {0, 1, 1, 1, 0, -1, -1, -1};
/* Directions whose square index increases: their nearest blocker is the lowest bit. */
static const int DIR_POSITIVE[8] = {1, 1, 1, 0, 0, 0, 0, 1};

static uint64_t KNIGHT_ATT[64], KING_ATT[64], PAWN_ATT[2][64], RAYS[8][64];
static uint64_t Z_PIECE[12][64], Z_CASTLE[16], Z_EP[8], Z_SIDE;
static int CASTLE_MASK[64];
static int initialized;

static inline int lsb(uint64_t b) { return __builtin_ctzll(b); }
static inline int msb(uint64_t b) { return 63 - __builtin_clzll(b); }
static inline int pop_lsb(uint64_t *b) {
    int sq = lsb(*b);
    *b &= *b - 1;
    return sq;
}
static inline int on_board(int rank, int file) {
    return rank >= 0 && rank < 8 && file >= 0 && file < 8;
}

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void fc_init(void) {
    static const int KNIGHT_STEPS[8][2] = {
        {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
    if (initialized) return;
    for (int sq = 0; sq < 64; sq++) {
        int r = sq / 8, f = sq % 8;
        for (int i = 0; i < 8; i++) {
            int rr = r + KNIGHT_STEPS[i][0], ff = f + KNIGHT_STEPS[i][1];
            if (on_board(rr, ff)) KNIGHT_ATT[sq] |= 1ULL << (rr * 8 + ff);
        }
        for (int dr = -1; dr <= 1; dr++)
            for (int df = -1; df <= 1; df++)
                if ((dr || df) && on_board(r + dr, f + df))
                    KING_ATT[sq] |= 1ULL << ((r + dr) * 8 + f + df);
        for (int df = -1; df <= 1; df += 2) {
            if (on_board(r + 1, f + df)) PAWN_ATT[FC_WHITE][sq] |= 1ULL << ((r + 1) * 8 + f + df);
            if (on_board(r - 1, f + df)) PAWN_ATT[FC_BLACK][sq] |= 1ULL << ((r - 1) * 8 + f + df);
        }
        for (int d = 0; d < 8; d++)
            for (int rr = r + DIR_RANK[d], ff = f + DIR_FILE[d]; on_board(rr, ff);
                 rr += DIR_RANK[d], ff += DIR_FILE[d])
                RAYS[d][sq] |= 1ULL << (rr * 8 + ff);
        CASTLE_MASK[sq] = FC_WK | FC_WQ | FC_BK | FC_BQ;
    }
    CASTLE_MASK[0] &= ~FC_WQ;
    CASTLE_MASK[7] &= ~FC_WK;
    CASTLE_MASK[4] &= ~(FC_WK | FC_WQ);
    CASTLE_MASK[56] &= ~FC_BQ;
    CASTLE_MASK[63] &= ~FC_BK;
    CASTLE_MASK[60] &= ~(FC_BK | FC_BQ);

    uint64_t seed = 0x5EEDC0FFEEULL;
    for (int pc = 0; pc < 12; pc++)
        for (int sq = 0; sq < 64; sq++) Z_PIECE[pc][sq] = splitmix64(&seed);
    for (int i = 0; i < 16; i++) Z_CASTLE[i] = splitmix64(&seed);
    for (int i = 0; i < 8; i++) Z_EP[i] = splitmix64(&seed);
    Z_SIDE = splitmix64(&seed);
    initialized = 1;
}

static inline uint64_t ray(int d, int sq, uint64_t occ) {
    uint64_t att = RAYS[d][sq], blockers = att & occ;
    if (blockers) att ^= RAYS[d][DIR_POSITIVE[d] ? lsb(blockers) : msb(blockers)];
    return att;
}
static inline uint64_t bishop_att(int sq, uint64_t occ) {
    return ray(NE, sq, occ) | ray(SE, sq, occ) | ray(SW, sq, occ) | ray(NW, sq, occ);
}
static inline uint64_t rook_att(int sq, uint64_t occ) {
    return ray(N, sq, occ) | ray(E, sq, occ) | ray(S, sq, occ) | ray(W, sq, occ);
}

uint64_t fc_attacks(int type, int color, int sq, uint64_t occupied) {
    fc_init();
    switch (type) {
    case FC_PAWN: return PAWN_ATT[color][sq];
    case FC_KNIGHT: return KNIGHT_ATT[sq];
    case FC_BISHOP: return bishop_att(sq, occupied);
    case FC_ROOK: return rook_att(sq, occupied);
    case FC_QUEEN: return bishop_att(sq, occupied) | rook_att(sq, occupied);
    case FC_KING: return KING_ATT[sq];
    default: return 0;
    }
}

int fc_is_attacked(const fc_position *p, int sq, int by) {
    const uint64_t *b = p->pieces[by];
    return (PAWN_ATT[by ^ 1][sq] & b[FC_PAWN])
        || (KNIGHT_ATT[sq] & b[FC_KNIGHT])
        || (KING_ATT[sq] & b[FC_KING])
        || (bishop_att(sq, p->all) & (b[FC_BISHOP] | b[FC_QUEEN]))
        || (rook_att(sq, p->all) & (b[FC_ROOK] | b[FC_QUEEN]));
}

int fc_in_check(const fc_position *p) {
    return fc_is_attacked(p, lsb(p->pieces[p->side][FC_KING]), p->side ^ 1);
}

static inline void put(fc_position *p, int pc, int sq) {
    uint64_t bit = 1ULL << sq;
    p->pieces[pc / 6][pc % 6] |= bit;
    p->occupied[pc / 6] |= bit;
    p->all |= bit;
    p->board[sq] = (int8_t)pc;
    p->hash ^= Z_PIECE[pc][sq];
}

static inline void take(fc_position *p, int sq) {
    int pc = p->board[sq];
    uint64_t bit = 1ULL << sq;
    p->pieces[pc / 6][pc % 6] &= ~bit;
    p->occupied[pc / 6] &= ~bit;
    p->all &= ~bit;
    p->board[sq] = -1;
    p->hash ^= Z_PIECE[pc][sq];
}

/* The en-passant square enters the hash only when the side to move has a pawn
 * that could capture there, so transpositions that differ in nothing else match. */
static inline uint64_t ep_key(const fc_position *p) {
    if (p->ep >= 0 && (PAWN_ATT[p->side ^ 1][p->ep] & p->pieces[p->side][FC_PAWN]))
        return Z_EP[p->ep & 7];
    return 0;
}

static uint64_t compute_hash(const fc_position *p) {
    uint64_t h = Z_CASTLE[p->castling] ^ ep_key(p);
    for (int sq = 0; sq < 64; sq++)
        if (p->board[sq] >= 0) h ^= Z_PIECE[p->board[sq]][sq];
    if (p->side == FC_BLACK) h ^= Z_SIDE;
    return h;
}

void fc_make_move(fc_position *p, fc_move m) {
    int from = FC_FROM(m), to = FC_TO(m), promo = FC_PROMO(m), flags = FC_FLAGS(m);
    int us = p->side, pc = p->board[from], capture = 0;

    p->hash ^= Z_CASTLE[p->castling] ^ ep_key(p);
    if (flags & FC_EN_PASSANT) {
        take(p, us == FC_WHITE ? to - 8 : to + 8);
        capture = 1;
    } else if (p->board[to] >= 0) {
        take(p, to);
        capture = 1;
    }
    take(p, from);
    put(p, promo ? us * 6 + promo : pc, to);
    if (flags & FC_CASTLE) {
        int rook_from, rook_to;
        switch (to) {
        case 6: rook_from = 7; rook_to = 5; break;
        case 2: rook_from = 0; rook_to = 3; break;
        case 62: rook_from = 63; rook_to = 61; break;
        default: rook_from = 56; rook_to = 59; break;
        }
        int rook = p->board[rook_from];
        take(p, rook_from);
        put(p, rook, rook_to);
    }
    p->castling &= CASTLE_MASK[from] & CASTLE_MASK[to];
    p->ep = (flags & FC_DOUBLE_PUSH) ? (from + to) / 2 : -1;
    p->halfmove = (capture || pc % 6 == FC_PAWN) ? 0 : p->halfmove + 1;
    if (us == FC_BLACK) p->fullmove++;
    p->side ^= 1;
    p->hash ^= Z_SIDE ^ Z_CASTLE[p->castling] ^ ep_key(p);
}

static inline int add(fc_move *out, int n, int from, int to, int promo, int flags) {
    out[n] = (fc_move)(from | (to << 6) | (promo << 12) | (flags << 16));
    return n + 1;
}

static int add_promotions(fc_move *out, int n, int from, int to, int flags) {
    for (int pr = FC_QUEEN; pr >= FC_KNIGHT; pr--) n = add(out, n, from, to, pr, flags);
    return n;
}

static int gen_pseudo(const fc_position *p, fc_move *out, int captures_only) {
    int us = p->side, them = us ^ 1, n = 0;
    int forward = us == FC_WHITE ? 8 : -8;
    uint64_t own = p->occupied[us], enemy = p->occupied[them];
    uint64_t last_rank = us == FC_WHITE ? 0xFF00000000000000ULL : 0xFFULL;
    uint64_t start_rank = us == FC_WHITE ? 0xFF00ULL : 0x00FF000000000000ULL;
    const uint64_t *b = p->pieces[us];

    uint64_t pawns = b[FC_PAWN];
    while (pawns) {
        int from = pop_lsb(&pawns), to = from + forward;
        if (!(p->all & (1ULL << to))) {
            if ((1ULL << to) & last_rank) {
                n = add_promotions(out, n, from, to, 0);
            } else if (!captures_only) {
                n = add(out, n, from, to, 0, 0);
                if (((1ULL << from) & start_rank) && !(p->all & (1ULL << (to + forward))))
                    n = add(out, n, from, to + forward, 0, FC_DOUBLE_PUSH);
            }
        }
        uint64_t caps = PAWN_ATT[us][from] & enemy;
        while (caps) {
            int t = pop_lsb(&caps);
            if ((1ULL << t) & last_rank) n = add_promotions(out, n, from, t, FC_CAPTURE);
            else n = add(out, n, from, t, 0, FC_CAPTURE);
        }
        if (p->ep >= 0 && (PAWN_ATT[us][from] & (1ULL << p->ep)))
            n = add(out, n, from, p->ep, 0, FC_CAPTURE | FC_EN_PASSANT);
    }

    uint64_t targets = captures_only ? enemy : ~own;
    for (int type = FC_KNIGHT; type <= FC_KING; type++) {
        uint64_t pieces = b[type];
        while (pieces) {
            int from = pop_lsb(&pieces);
            uint64_t att = fc_attacks(type, us, from, p->all) & targets;
            while (att) {
                int t = pop_lsb(&att);
                n = add(out, n, from, t, 0, (enemy >> t) & 1 ? FC_CAPTURE : 0);
            }
        }
    }

    if (!captures_only) {
        int base = us == FC_WHITE ? 0 : 56, king = us * 6 + FC_KING, rook = us * 6 + FC_ROOK;
        int ks = us == FC_WHITE ? FC_WK : FC_BK, qs = us == FC_WHITE ? FC_WQ : FC_BQ;
        if ((p->castling & ks) && p->board[base + 4] == king && p->board[base + 7] == rook
            && !(p->all & (0x60ULL << base))
            && !fc_is_attacked(p, base + 4, them) && !fc_is_attacked(p, base + 5, them)
            && !fc_is_attacked(p, base + 6, them))
            n = add(out, n, base + 4, base + 6, 0, FC_CASTLE);
        if ((p->castling & qs) && p->board[base + 4] == king && p->board[base] == rook
            && !(p->all & (0x0EULL << base))
            && !fc_is_attacked(p, base + 4, them) && !fc_is_attacked(p, base + 3, them)
            && !fc_is_attacked(p, base + 2, them))
            n = add(out, n, base + 4, base + 2, 0, FC_CASTLE);
    }
    return n;
}

static int keep_legal(const fc_position *p, fc_move *moves, int n) {
    int kept = 0, us = p->side;
    for (int i = 0; i < n; i++) {
        fc_position next = *p;
        fc_make_move(&next, moves[i]);
        if (!fc_is_attacked(&next, lsb(next.pieces[us][FC_KING]), us ^ 1)) moves[kept++] = moves[i];
    }
    return kept;
}

int fc_legal_moves(const fc_position *p, fc_move *out) {
    return keep_legal(p, out, gen_pseudo(p, out, 0));
}

int fc_legal_captures(const fc_position *p, fc_move *out) {
    return keep_legal(p, out, gen_pseudo(p, out, 1));
}

uint64_t fc_perft(const fc_position *p, int depth) {
    fc_move moves[FC_MAX_MOVES];
    if (depth <= 0) return 1;
    int n = fc_legal_moves(p, moves);
    if (depth == 1) return (uint64_t)n;
    uint64_t total = 0;
    for (int i = 0; i < n; i++) {
        fc_position next = *p;
        fc_make_move(&next, moves[i]);
        total += fc_perft(&next, depth - 1);
    }
    return total;
}

int fc_set_fen(fc_position *p, const char *fen) {
    static const char PIECES[] = "PNBRQKpnbrqk";
    const char *s = fen;
    int rank = 7, file = 0;

    fc_init();
    memset(p, 0, sizeof *p);
    memset(p->board, -1, sizeof p->board);
    p->ep = -1;
    p->fullmove = 1;

    for (; *s && *s != ' '; s++) {
        if (*s == '/') {
            if (file != 8 || rank == 0) return -1;
            rank--;
            file = 0;
        } else if (*s >= '1' && *s <= '8') {
            file += *s - '0';
            if (file > 8) return -1;
        } else {
            const char *pc = strchr(PIECES, *s);
            if (!pc || file > 7) return -1;
            put(p, (int)(pc - PIECES), rank * 8 + file++);
        }
    }
    if (rank != 0 || file != 8) return -1;
    if (__builtin_popcountll(p->pieces[FC_WHITE][FC_KING]) != 1
        || __builtin_popcountll(p->pieces[FC_BLACK][FC_KING]) != 1)
        return -1;

    while (*s == ' ') s++;
    if (*s == 'w') p->side = FC_WHITE;
    else if (*s == 'b') p->side = FC_BLACK;
    else return -1;
    s++;

    while (*s == ' ') s++;
    if (*s == '-') {
        s++;
    } else {
        for (; *s && *s != ' '; s++) {
            switch (*s) {
            case 'K': p->castling |= FC_WK; break;
            case 'Q': p->castling |= FC_WQ; break;
            case 'k': p->castling |= FC_BK; break;
            case 'q': p->castling |= FC_BQ; break;
            default: return -1;
            }
        }
    }
    /* Drop rights the king and rooks are no longer home for. */
    if (p->board[4] != FC_KING) p->castling &= ~(FC_WK | FC_WQ);
    if (p->board[7] != FC_ROOK) p->castling &= ~FC_WK;
    if (p->board[0] != FC_ROOK) p->castling &= ~FC_WQ;
    if (p->board[60] != 6 + FC_KING) p->castling &= ~(FC_BK | FC_BQ);
    if (p->board[63] != 6 + FC_ROOK) p->castling &= ~FC_BK;
    if (p->board[56] != 6 + FC_ROOK) p->castling &= ~FC_BQ;

    while (*s == ' ') s++;
    if (*s == '-') {
        s++;
    } else if (*s >= 'a' && *s <= 'h' && s[1] >= '1' && s[1] <= '8') {
        int ep = (s[1] - '1') * 8 + (s[0] - 'a');
        int pawn_sq = p->side == FC_WHITE ? ep - 8 : ep + 8;
        int expected_rank = p->side == FC_WHITE ? 5 : 2;
        /* Keep it only if a pawn really just made the double push. */
        if (ep / 8 == expected_rank && p->board[pawn_sq] == (p->side ^ 1) * 6 + FC_PAWN
            && p->board[ep] < 0)
            p->ep = ep;
        s += 2;
    } else if (*s) {
        return -1;
    }

    char *end;
    while (*s == ' ') s++;
    if (*s) {
        p->halfmove = (int)strtol(s, &end, 10);
        s = end;
        while (*s == ' ') s++;
        if (*s) p->fullmove = (int)strtol(s, &end, 10);
    }
    p->hash = compute_hash(p);
    return 0;
}

void fc_get_fen(const fc_position *p, char *out) {
    static const char PIECES[] = "PNBRQKpnbrqk";
    char *o = out;
    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            int pc = p->board[rank * 8 + file];
            if (pc < 0) {
                empty++;
                continue;
            }
            if (empty) *o++ = (char)('0' + empty);
            empty = 0;
            *o++ = PIECES[pc];
        }
        if (empty) *o++ = (char)('0' + empty);
        if (rank) *o++ = '/';
    }
    *o++ = ' ';
    *o++ = p->side == FC_WHITE ? 'w' : 'b';
    *o++ = ' ';
    if (!p->castling) *o++ = '-';
    if (p->castling & FC_WK) *o++ = 'K';
    if (p->castling & FC_WQ) *o++ = 'Q';
    if (p->castling & FC_BK) *o++ = 'k';
    if (p->castling & FC_BQ) *o++ = 'q';
    *o++ = ' ';
    if (p->ep >= 0) {
        *o++ = (char)('a' + p->ep % 8);
        *o++ = (char)('1' + p->ep / 8);
    } else {
        *o++ = '-';
    }
    sprintf(o, " %d %d", p->halfmove, p->fullmove);
}

int fc_piece_at(const fc_position *p, int sq) { return p->board[sq]; }

void fc_move_to_uci(fc_move m, char *out) {
    static const char PROMO[] = " nbrq";
    int from = FC_FROM(m), to = FC_TO(m), promo = FC_PROMO(m);
    out[0] = (char)('a' + from % 8);
    out[1] = (char)('1' + from / 8);
    out[2] = (char)('a' + to % 8);
    out[3] = (char)('1' + to / 8);
    out[4] = promo ? PROMO[promo] : '\0';
    out[5] = '\0';
}

fc_move fc_parse_uci(const fc_position *p, const char *uci) {
    fc_move moves[FC_MAX_MOVES];
    char text[6];
    int n = fc_legal_moves(p, moves);
    for (int i = 0; i < n; i++) {
        fc_move_to_uci(moves[i], text);
        if (strcmp(text, uci) == 0) return moves[i];
    }
    return 0;
}

size_t fc_position_size(void) { return sizeof(fc_position); }
int fc_side(const fc_position *p) { return p->side; }
uint64_t fc_hash(const fc_position *p) { return p->hash; }
int fc_halfmove(const fc_position *p) { return p->halfmove; }
int fc_fullmove(const fc_position *p) { return p->fullmove; }
uint64_t fc_bitboard(const fc_position *p, int color, int type) { return p->pieces[color][type]; }
