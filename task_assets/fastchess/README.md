# fastchess

A legal-move generator for chess, written in C, with Python bindings. It is
supplied for you to build on.

It keeps board state, generates legal moves (including castling, en passant
and promotion), makes moves, detects check and attacks, and keeps a Zobrist
hash of the position. **It does not search or evaluate positions**: choosing
moves is up to you.

It is correct against the standard perft suite, and it is fast: `./fcperft 6`
walks the 119,060,324 leaf positions of the start position.

## Build

    make            # libfastchess.a, libfastchess.so, fcperft

The directory is already built; run `make` again after changing the source.

## From C

    #include "fastchess.h"

    fc_position pos;
    fc_set_fen(&pos, FC_START_FEN);

    fc_move moves[FC_MAX_MOVES];
    int n = fc_legal_moves(&pos, moves);
    for (int i = 0; i < n; i++) {
        fc_position next = pos;          /* copy, then make: undo is free */
        fc_make_move(&next, moves[i]);
        /* ... search next ... */
    }

    cc -O2 -o mysearch mysearch.c libfastchess.a

`fastchess.h` documents every function. The ones a search needs most are
`fc_legal_moves`, `fc_legal_captures` (captures and promotions, for
quiescence), `fc_make_move`, `fc_in_check`, `fc_move_to_uci`, `fc_parse_uci`,
and the position's `hash`, `halfmove` and bitboard fields. `perft.c` is a
short complete program.

## From Python

    import sys; sys.path.insert(0, "/workdir/fastchess")
    import fastchess

    board = fastchess.Board()            # or Board(fen)
    for move in board.legal_moves():
        board.push(move)
        # ...
        board.pop()
    board.push_uci("e2e4")
    print(board.fen(), board.is_check(), fastchess.Board.uci(move))

Each call crosses into C, so a Python search is still limited by the Python
interpreter; a search written in C gets the full speed.

## Conventions

- Squares run a1 = 0, b1 = 1, ..., h8 = 63, as in python-chess.
- A move is a 32-bit integer: `from | to << 6 | promotion << 12 | flags << 16`.
  Promotion is 0 for none, else `FC_KNIGHT`..`FC_QUEEN`; flags are
  `FC_CAPTURE`, `FC_EN_PASSANT`, `FC_CASTLE`, `FC_DOUBLE_PUSH`. Castling is the
  king's two-square move, as in UCI (`e1g1`). 0 is never a move.
- `fc_legal_moves` returning 0 means checkmate if `fc_in_check`, else
  stalemate. Repetition and the fifty-move rule are left to the caller: keep
  the hashes of the positions played and read `halfmove`.
