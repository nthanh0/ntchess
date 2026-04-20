#ifndef TYPES_H
#define TYPES_H

#include <array>
#include <chrono>
#include <cstdint>

enum Piece {
    NO_PIECE,
    W_KING, W_QUEEN, W_ROOK, W_BISHOP, W_KNIGHT, W_PAWN,
    B_KING, B_QUEEN, B_ROOK, B_BISHOP, B_KNIGHT, B_PAWN
};

enum PieceType {
    NO_PIECE_TYPE, KING, QUEEN, ROOK, BISHOP, KNIGHT, PAWN
};

enum Color {
    WHITE,
    BLACK
};

enum MoveType { NORMAL, CASTLING, EN_PASSANT, PROMOTION };

struct Move {
    int square_from;
    int square_to;
    Piece piece_moved;
    Piece piece_captured = NO_PIECE ;
    MoveType move_type = NORMAL;

    PieceType promotion_piece_type = NO_PIECE_TYPE; // Only used for promotion moves
    Move(int from, int to, Piece moved, Piece captured = NO_PIECE, MoveType type = NORMAL, PieceType promotion_type = NO_PIECE_TYPE)
        : square_from(from), square_to(to), piece_moved(moved), piece_captured(captured), move_type(type), promotion_piece_type(promotion_type) {}
};

struct StateInfo {
    bool white_can_castle_kingside = true;
    bool white_can_castle_queenside = true;
    bool black_can_castle_kingside = true;
    bool black_can_castle_queenside = true;
    int en_passant_file = -1;
    int halfmove_clock = 0;
    bool can_castle_kingside(Color color) const {
        return (color == WHITE) ? white_can_castle_kingside : black_can_castle_kingside;
    }
    bool can_castle_queenside(Color color) const {
        return (color == WHITE) ? white_can_castle_queenside : black_can_castle_queenside;
    }
};

// Compact snapshot of a chess position used for repetition detection.
// Two positions are identical (for repetition purposes) when piece placement,
// side to move, castling rights, and en-passant file all match.
struct Position {
    std::array<Piece, 64> pieces;
    Color  side_to_move;
    bool   white_castle_ks;
    bool   white_castle_qs;
    bool   black_castle_ks;
    bool   black_castle_qs;
    int    ep_file; // -1 if no en-passant square

    bool operator==(const Position& o) const {
        return pieces        == o.pieces
            && side_to_move  == o.side_to_move
            && white_castle_ks == o.white_castle_ks
            && white_castle_qs == o.white_castle_qs
            && black_castle_ks == o.black_castle_ks
            && black_castle_qs == o.black_castle_qs
            && ep_file        == o.ep_file;
    }
};

enum GameStatus {
    ONGOING,
    CHECKMATE,
    STALEMATE,
    DRAW_BY_REPETITION,
    DRAW_BY_FIFTY_MOVE_RULE,
    DRAW_BY_INSUFFICIENT_MATERIAL,
    TIMEOUT
};

// ---------------------------------------------------------------------------
// Clock – tracks remaining time and increment for one side.
// ---------------------------------------------------------------------------
struct Clock {
    long long remaining_ms = 0;   // time left in milliseconds
    long long increment_ms = 0;   // added after each move

    Clock() = default;
    Clock(long long time_ms, long long inc_ms = 0)
        : remaining_ms(time_ms), increment_ms(inc_ms) {}

    bool is_flagged() const { return remaining_ms <= 0; }
};

#endif