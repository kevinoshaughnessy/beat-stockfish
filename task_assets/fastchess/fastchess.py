"""Python bindings for fastchess (ctypes). See README.md.

Moves are the C library's packed integers; ``Board.uci`` turns one into text.
Squares match python-chess (a1 = 0, h8 = 63).
"""

from __future__ import annotations

import ctypes
import os

_lib = ctypes.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)), "libfastchess.so"))

_P = ctypes.c_void_p
for _name, _res, _args in (
    ("fc_set_fen", ctypes.c_int, [_P, ctypes.c_char_p]),
    ("fc_get_fen", None, [_P, ctypes.c_char_p]),
    ("fc_legal_moves", ctypes.c_int, [_P, _P]),
    ("fc_legal_captures", ctypes.c_int, [_P, _P]),
    ("fc_make_move", None, [_P, ctypes.c_uint32]),
    ("fc_in_check", ctypes.c_int, [_P]),
    ("fc_is_attacked", ctypes.c_int, [_P, ctypes.c_int, ctypes.c_int]),
    ("fc_attacks", ctypes.c_uint64, [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_uint64]),
    ("fc_piece_at", ctypes.c_int, [_P, ctypes.c_int]),
    ("fc_move_to_uci", None, [ctypes.c_uint32, ctypes.c_char_p]),
    ("fc_parse_uci", ctypes.c_uint32, [_P, ctypes.c_char_p]),
    ("fc_perft", ctypes.c_uint64, [_P, ctypes.c_int]),
    ("fc_position_size", ctypes.c_size_t, []),
    ("fc_side", ctypes.c_int, [_P]),
    ("fc_hash", ctypes.c_uint64, [_P]),
    ("fc_halfmove", ctypes.c_int, [_P]),
    ("fc_fullmove", ctypes.c_int, [_P]),
    ("fc_bitboard", ctypes.c_uint64, [_P, ctypes.c_int, ctypes.c_int]),
):
    _fn = getattr(_lib, _name)
    _fn.restype = _res
    _fn.argtypes = _args

WHITE, BLACK = 0, 1
PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING = range(6)
CAPTURE, EN_PASSANT, CASTLE, DOUBLE_PUSH = 1, 2, 4, 8
START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
MAX_MOVES = 256

_SIZE = _lib.fc_position_size()
_WORDS = (_SIZE + 7) // 8  # held as uint64 words so the struct stays aligned


def move_from(move: int) -> int:
    return move & 63


def move_to(move: int) -> int:
    return (move >> 6) & 63


def move_promotion(move: int) -> int:
    """0 for none, else KNIGHT..QUEEN."""
    return (move >> 12) & 7


def move_flags(move: int) -> int:
    return (move >> 16) & 15


def attacks(piece_type: int, color: int, square: int, occupied: int) -> int:
    """Bitboard of squares a piece on *square* attacks, given the occupancy."""
    return _lib.fc_attacks(piece_type, color, square, occupied)


class Board:
    """A position with a move stack. push()/pop() are cheap: pop restores a copy."""

    def __init__(self, fen: str = START_FEN) -> None:
        self._pos = (ctypes.c_uint64 * _WORDS)()
        self._moves = (ctypes.c_uint32 * MAX_MOVES)()
        self._stack: list[bytes] = []
        if _lib.fc_set_fen(self._pos, fen.encode()) != 0:
            raise ValueError(f"invalid FEN: {fen!r}")

    def copy(self) -> Board:
        other = Board.__new__(Board)
        other._pos = (ctypes.c_uint64 * _WORDS).from_buffer_copy(self._pos)
        other._moves = (ctypes.c_uint32 * MAX_MOVES)()
        other._stack = list(self._stack)
        return other

    def legal_moves(self) -> list[int]:
        return self._moves[: _lib.fc_legal_moves(self._pos, self._moves)]

    def legal_captures(self) -> list[int]:
        """Captures, en-passant captures and promotions."""
        return self._moves[: _lib.fc_legal_captures(self._pos, self._moves)]

    def push(self, move: int) -> None:
        """Play a move from legal_moves()."""
        self._stack.append(bytes(self._pos))
        _lib.fc_make_move(self._pos, move)

    def push_uci(self, uci: str) -> int:
        move = _lib.fc_parse_uci(self._pos, uci.encode())
        if not move:
            raise ValueError(f"illegal move: {uci}")
        self.push(move)
        return move

    def pop(self) -> None:
        ctypes.memmove(self._pos, self._stack.pop(), _SIZE)

    def is_check(self) -> bool:
        return bool(_lib.fc_in_check(self._pos))

    def is_attacked(self, square: int, by_color: int) -> bool:
        return bool(_lib.fc_is_attacked(self._pos, square, by_color))

    def piece_at(self, square: int) -> tuple[int, int] | None:
        """(color, piece_type), or None if the square is empty."""
        pc = _lib.fc_piece_at(self._pos, square)
        return None if pc < 0 else divmod(pc, 6)

    def bitboard(self, color: int, piece_type: int) -> int:
        return _lib.fc_bitboard(self._pos, color, piece_type)

    def fen(self) -> str:
        out = ctypes.create_string_buffer(100)
        _lib.fc_get_fen(self._pos, out)
        return out.value.decode()

    def perft(self, depth: int) -> int:
        return _lib.fc_perft(self._pos, depth)

    @property
    def turn(self) -> int:
        return _lib.fc_side(self._pos)

    @property
    def hash(self) -> int:
        return _lib.fc_hash(self._pos)

    @property
    def halfmove_clock(self) -> int:
        return _lib.fc_halfmove(self._pos)

    @property
    def fullmove_number(self) -> int:
        return _lib.fc_fullmove(self._pos)

    @staticmethod
    def uci(move: int) -> str:
        out = ctypes.create_string_buffer(6)
        _lib.fc_move_to_uci(move, out)
        return out.value.decode()
