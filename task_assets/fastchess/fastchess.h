/* fastchess: a legal-move generator for standard chess.
 *
 * It keeps board state, generates legal moves, makes moves, detects check and
 * hashes positions. It does not search or evaluate: choosing moves is up to
 * the caller. See README.md for the conventions and an example.
 */
#ifndef FASTCHESS_H
#define FASTCHESS_H

#include <stddef.h>
#include <stdint.h>

enum { FC_WHITE, FC_BLACK };
enum { FC_PAWN, FC_KNIGHT, FC_BISHOP, FC_ROOK, FC_QUEEN, FC_KING };

/* Castling-rights bits. */
enum { FC_WK = 1, FC_WQ = 2, FC_BK = 4, FC_BQ = 8 };

/* Move flags. */
enum { FC_CAPTURE = 1, FC_EN_PASSANT = 2, FC_CASTLE = 4, FC_DOUBLE_PUSH = 8 };

#define FC_MAX_MOVES 256
#define FC_START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

/* Squares run a1 = 0, b1 = 1, ..., h1 = 7, a2 = 8, ..., h8 = 63.
 * A move packs from, to, promotion piece (0 for none, else FC_KNIGHT..FC_QUEEN)
 * and flags. Castling is the king's two-square move (e1g1). 0 is never a move. */
typedef uint32_t fc_move;
#define FC_FROM(m) ((int)((m) & 63))
#define FC_TO(m) ((int)(((m) >> 6) & 63))
#define FC_PROMO(m) ((int)(((m) >> 12) & 7))
#define FC_FLAGS(m) ((int)(((m) >> 16) & 15))

typedef struct {
    uint64_t pieces[2][6];  /* bitboards by color and piece type */
    uint64_t occupied[2];   /* by color */
    uint64_t all;
    uint64_t hash;          /* Zobrist key, updated incrementally */
    int8_t board[64];       /* -1 if empty, else color * 6 + type */
    int side;               /* side to move */
    int castling;           /* FC_WK | FC_WQ | FC_BK | FC_BQ */
    int ep;                 /* en-passant target square, or -1 */
    int halfmove;           /* plies since the last capture or pawn move */
    int fullmove;
} fc_position;

/* Build the attack and hash tables. fc_set_fen calls it; it is idempotent. */
void fc_init(void);

/* Returns 0 on success, -1 if the FEN is invalid. */
int fc_set_fen(fc_position *p, const char *fen);
/* Writes a FEN of at most 100 bytes, including the terminator. */
void fc_get_fen(const fc_position *p, char *out);

/* Fill out (room for FC_MAX_MOVES) and return the count. */
int fc_legal_moves(const fc_position *p, fc_move *out);
/* Legal captures, en-passant captures and promotions only (for quiescence). */
int fc_legal_captures(const fc_position *p, fc_move *out);

/* Play a legal move in place. To undo, copy the position first:
 *     fc_position saved = *p; fc_make_move(p, m); ... *p = saved; */
void fc_make_move(fc_position *p, fc_move m);

int fc_in_check(const fc_position *p);
int fc_is_attacked(const fc_position *p, int sq, int by_color);
/* Squares a piece of this type and color on sq attacks, given the occupancy. */
uint64_t fc_attacks(int type, int color, int sq, uint64_t occupied);

/* -1 if empty, else color * 6 + type. */
int fc_piece_at(const fc_position *p, int sq);

/* Writes at most 6 bytes, including the terminator. */
void fc_move_to_uci(fc_move m, char *out);
/* The legal move matching a UCI string, or 0 if there is none. */
fc_move fc_parse_uci(const fc_position *p, const char *uci);

/* Leaf nodes at the given depth: the standard correctness and speed check. */
uint64_t fc_perft(const fc_position *p, int depth);

/* Accessors, for callers (such as the Python bindings) that hold the
 * position as opaque memory. */
size_t fc_position_size(void);
int fc_side(const fc_position *p);
uint64_t fc_hash(const fc_position *p);
int fc_halfmove(const fc_position *p);
int fc_fullmove(const fc_position *p);
uint64_t fc_bitboard(const fc_position *p, int color, int type);

#endif
