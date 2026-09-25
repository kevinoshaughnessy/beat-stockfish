"""Check fastchess against the perft suite and against python-chess.

Runs where fastchess is built and python-chess is installed, e.g. against the
copy the image builds:

    docker run --rm -v "$PWD/tests:/tests:ro" beat-stockfish:local \
        python3 /tests/test_fastchess.py /opt/match/fastchess
"""

from __future__ import annotations

import random
import sys

import chess

sys.path.insert(0, sys.argv[1] if len(sys.argv) > 1 else "task_assets/fastchess")
import fastchess  # noqa: E402

# (FEN, depth, leaf count) from the Chess Programming Wiki's perft results,
# at depths that stay quick under x86 emulation.
PERFT = [
    (chess.STARTING_FEN, 5, 4_865_609),
    ("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4_085_603),
    ("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674_624),
    ("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422_333),
    ("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2_103_487),
    ("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4, 3_894_594),
]

RANDOM_GAMES = 300


def check_perft() -> None:
    for fen, depth, expected in PERFT:
        got = fastchess.Board(fen).perft(depth)
        assert got == expected, f"perft({depth}) of {fen}: {got}, expected {expected}"


def check_against_python_chess() -> None:
    """Random games, comparing every position with python-chess."""
    rng = random.Random(0)
    positions = 0
    for _ in range(RANDOM_GAMES):
        ref = chess.Board()
        fast = fastchess.Board()
        while not ref.is_game_over(claim_draw=False):
            fen = ref.fen(en_passant="fen")
            assert fast.fen() == fen, f"{fast.fen()} != {fen}"
            assert fast.is_check() == ref.is_check(), fen
            moves = {fastchess.Board.uci(m): m for m in fast.legal_moves()}
            assert sorted(moves) == sorted(m.uci() for m in ref.legal_moves), fen
            captures = {fastchess.Board.uci(m) for m in fast.legal_captures()}
            expected = {
                m.uci() for m in ref.legal_moves if ref.is_capture(m) or m.promotion
            }
            assert captures == expected, fen
            # The incremental hash must equal the hash of the same position read fresh.
            assert fast.hash == fastchess.Board(fen).hash, fen
            positions += 1

            move = rng.choice(list(ref.legal_moves))
            ref.push(move)
            fast.push(moves[move.uci()])
            if rng.random() < 0.1:  # exercise pop()
                fast.pop()
                fast.push(moves[move.uci()])
    print(f"python-chess agrees on {positions} positions from {RANDOM_GAMES} games")


def check_transposition_hash() -> None:
    a, b = fastchess.Board(), fastchess.Board()
    for uci in ("g1f3", "g8f6", "b1c3", "b8c6"):
        a.push_uci(uci)
    for uci in ("b1c3", "b8c6", "g1f3", "g8f6"):
        b.push_uci(uci)
    assert a.hash == b.hash and a.fen() == b.fen()


if __name__ == "__main__":
    check_perft()
    check_transposition_hash()
    check_against_python_chess()
    print("fastchess: all checks passed")
