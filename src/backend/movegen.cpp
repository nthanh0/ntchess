#include "movegen.h"
#include <vector>
#include "Board.h"
#include "types.h"
#include "misc.h"

std::vector<Move> generate_moves(const Board& board, int square) {
    std::vector<Move> moves;
    switch(get_type(board.get_piece(square))) {
        case KING: {
            // Generate king moves (one square in any direction)
            int file = square % 8;
            int rank = square / 8;
            for (int df : {-1, 0, 1}) {
                for (int dr : {-1, 0, 1}) {
                    if (df == 0 && dr == 0) continue; // Skip current square
                    int new_file = file + df;
                    int new_rank = rank + dr;
                    // Check bounds
                    if (new_file >= 0 && new_file < 8 && new_rank >= 0 && new_rank < 8) {
                        int target_square = get_index(new_file, new_rank);
                        if (board.get_piece(target_square) && get_color(board.get_piece(target_square)) == get_color(board.get_piece(square))) {
                            continue; // Can't move to a square occupied by own piece
                        }
                        moves.emplace_back(square, target_square, board.get_piece(square), board.get_piece(target_square));
                    }
                }
            }
            if (board.get_state_info().can_castle_kingside(get_color(board.get_piece(square)))) {
                // Check if squares between king and rook are empty and not under attack
                if (!board.get_piece(get_index(5, rank)) && !board.get_piece(get_index(6, rank))) {
                    moves.emplace_back(square, get_index(6, rank), board.get_piece(square), NO_PIECE, CASTLING);
                }
            }
            if (board.get_state_info().can_castle_queenside(get_color(board.get_piece(square)))) {
                // Check if squares between king and rook are empty and not under attack
                if (!board.get_piece(get_index(1, rank)) && !board.get_piece(get_index(2, rank)) && !board.get_piece(get_index(3, rank))) {
                    moves.emplace_back(square, get_index(2, rank), board.get_piece(square), NO_PIECE, CASTLING);
                }
            }
            break;
        }
        case QUEEN: {
            int file = square % 8;
            int rank = square / 8;
            // Generate queen moves (combination of rook and bishop)
            for (int df : {-1, 0, 1}) {
                for (int dr : {-1, 0, 1}) {
                    if (df == 0 && dr == 0) continue; // Skip current square
                    int new_file = file + df;
                    int new_rank = rank + dr;
                    while (new_file >= 0 && new_file < 8 && new_rank >= 0 && new_rank < 8) {
                        int target_square = get_index(new_file, new_rank);
                        if (board.get_piece(target_square)) {
                            if (get_color(board.get_piece(target_square)) != get_color(board.get_piece(square))) {
                                moves.emplace_back(square, target_square, board.get_piece(square), board.get_piece(target_square));
                            }
                            break; // Can't move past occupied square
                        }
                        moves.emplace_back(square, target_square, board.get_piece(square), NO_PIECE);
                        new_file += df;
                        new_rank += dr;
                    }
                }
            }
            break;
        }
        case ROOK: {
            // Generate rook moves (horizontal and vertical)
            int file = square % 8;
            int rank = square / 8;
                for (int df : {-1, 0, 1}) {
                    for (int dr : {-1, 0, 1}) {
                        if ((df == 0 && dr == 0) || (df != 0 && dr != 0)) continue; // Skip current square and diagonals
                        int new_file = file + df;
                        int new_rank = rank + dr;
                        while (new_file >= 0 && new_file < 8 && new_rank >= 0 && new_rank < 8) {
                            int target_square = get_index(new_file, new_rank);
                            if (board.get_piece(target_square)) {
                                if (get_color(board.get_piece(target_square)) != get_color(board.get_piece(square))) {
                                    moves.emplace_back(square, target_square, board.get_piece(square), board.get_piece(target_square));
                                }
                                break; // Can't move past occupied square
                            }
                            moves.emplace_back(square, target_square, board.get_piece(square), NO_PIECE);
                            new_file += df;
                            new_rank += dr;
                        }
                    }
                }
                break;
            }
        case BISHOP: {
            // Generate bishop moves (diagonal)
            for (int df : {-1, 1}) {
                for (int dr : {-1, 1}) {
                    int file = square % 8;
                    int rank = square / 8;
                    int new_file = file + df;
                    int new_rank = rank + dr;
                    while (new_file >= 0 && new_file < 8 && new_rank >= 0 && new_rank < 8) {
                        int target_square = get_index(new_file, new_rank);
                        if (board.get_piece(target_square)) {
                            if (get_color(board.get_piece(target_square)) != get_color(board.get_piece(square))) {
                                moves.emplace_back(square, target_square, board.get_piece(square), board.get_piece(target_square));
                            }
                            break; // Can't move past occupied square
                        }
                        moves.emplace_back(square, target_square, board.get_piece(square), NO_PIECE);
                        new_file += df;
                        new_rank += dr;
                    }
                }
            }
            break;
        }
        case KNIGHT: {
            // Generate knight moves (L-shaped)
            int file = square % 8;
            int rank = square / 8;
            std::vector<std::pair<int, int>> knight_moves = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
            for (const auto& [df, dr] : knight_moves) {
                int new_file = file + df;
                int new_rank = rank + dr;
                if (new_file >= 0 && new_file < 8 && new_rank >= 0 && new_rank < 8) {
                    int target_square = get_index(new_file, new_rank);
                    if (board.get_piece(target_square) && get_color(board.get_piece(target_square)) == get_color(board.get_piece(square))) {
                        continue; // Can't move to a square occupied by own piece
                    }
                    moves.emplace_back(square, target_square, board.get_piece(square), board.get_piece(target_square));
                }
            }
            break;
        }
        case PAWN: {
            // Generate pawn moves (one square forward, two squares from starting position, captures)
            int file = square % 8;
            int rank = square / 8;
            int direction = (get_color(board.get_piece(square)) == WHITE) ? 1 : -1;
            // One square forward
            int new_rank = rank + direction;
            if (new_rank >= 0 && new_rank < 8) {
                int target_square = get_index(file, new_rank);
                if (!board.get_piece(target_square)) {
                    if ((new_rank == 7 && get_color(board.get_piece(square)) == WHITE) 
                    || (new_rank == 0 && get_color(board.get_piece(square)) == BLACK)) {
                        // Promotion
                        moves.emplace_back(square, target_square, board.get_piece(square), NO_PIECE, PROMOTION, PieceType::QUEEN);
                        moves.emplace_back(square, target_square, board.get_piece(square), NO_PIECE, PROMOTION, PieceType::ROOK);
                        moves.emplace_back(square, target_square, board.get_piece(square), NO_PIECE, PROMOTION, PieceType::BISHOP);
                        moves.emplace_back(square, target_square, board.get_piece(square), NO_PIECE, PROMOTION, PieceType::KNIGHT);
                    } else {
                        moves.emplace_back(square, target_square, board.get_piece(square), NO_PIECE);
                    }
                    // Two squares from starting position
                    if ((rank == 1 && direction == 1) || (rank == 6 && direction == -1)) {
                        new_rank += direction;
                        if (new_rank >= 0 && new_rank < 8) {
                            target_square = get_index(file, new_rank);
                            if (!board.get_piece(target_square)) {
                                moves.emplace_back(square, target_square, board.get_piece(square), NO_PIECE);
                            }
                        }
                    }
                }
            }
            // Captures
            for (int df : {-1, 1}) {
                int new_file = file + df;
                new_rank = rank + direction;
                if (new_file >= 0 && new_file < 8 && new_rank >= 0 && new_rank < 8) {
                    int target_square = get_index(new_file, new_rank);
                    if (board.get_piece(target_square)) {
                        if (get_color(board.get_piece(target_square)) 
                            != get_color(board.get_piece(square))) {
                            if ((new_rank == 7 && get_color(board.get_piece(square)) == WHITE)
                            || (new_rank == 0 && get_color(board.get_piece(square)) == BLACK)) {
                                // Promotion capture
                                moves.emplace_back(square, target_square, board.get_piece(square), board.get_piece(target_square), PROMOTION, PieceType::QUEEN);
                                moves.emplace_back(square, target_square, board.get_piece(square), board.get_piece(target_square), PROMOTION, PieceType::ROOK);
                                moves.emplace_back(square, target_square, board.get_piece(square), board.get_piece(target_square), PROMOTION, PieceType::BISHOP);
                                moves.emplace_back(square, target_square, board.get_piece(square), board.get_piece(target_square), PROMOTION, PieceType::KNIGHT);
                            } else {
                                moves.emplace_back(square, target_square, board.get_piece(square), board.get_piece(target_square));
                            }
                        }
                    // En passant
                    } else if (new_file == board.get_state_info().en_passant_file 
                               && new_rank == ((get_color(board.get_piece(square)) == WHITE) ? 5 : 2)) {
                        moves.emplace_back(square, target_square, board.get_piece(square), (get_color(board.get_piece(square)) == WHITE) ? B_PAWN : W_PAWN, EN_PASSANT);
                    }
                }
            }
            break;
        }
        default:
            break;
    }
    return moves;
}

bool is_in_check(const Board& board, Color color) {
    int king_square = -1;
    for (int i = 0; i < 64; i++) {
        if (board.get_piece(i) != NO_PIECE && get_color(board.get_piece(i)) == color && get_type(board.get_piece(i)) == KING) {
            king_square = i;
            break;
        }
    }
    if (king_square == -1) {
        return false; // No king found, should not happen in a valid game
    }
    Color opponent_color = (color == WHITE) ? BLACK : WHITE;
    for (int i = 0; i < 64; i++) {
        if (board.get_piece(i) != NO_PIECE && get_color(board.get_piece(i)) == opponent_color) {
            auto moves = generate_moves(board, i);
            for (const auto& move : moves) {
                if (move.square_to == king_square) {
                    return true; // King is in check
                }
            }
        }
    }
    return false; // King is not in check
}

std::vector<Move> generate_legal_moves(Board& board, int square) {
    std::vector<Move> legal_moves;
    std::vector<Move> moves = generate_moves(board, square);
    for (const Move& move : moves) {
        if (move.move_type == CASTLING) {
            // Can't castle out of check
            if (is_in_check(board, get_color(move.piece_moved))) {
                continue;
            }
            int file_to = move.square_to % 8;
            if (file_to == 6) {
                Move temp_move = move; // Create a temporary move to check the move
                temp_move.square_to = get_index(5, move.square_to / 8); // Move king to f1/f8
                temp_move.move_type = NORMAL; // Temporarily treat as normal move for check detection
                board.make_move(temp_move);
                if (is_in_check(board, get_color(move.piece_moved))) {
                    board.unmake_move(temp_move);
                    continue; // Can't castle through check
                }
                board.unmake_move(temp_move);
            } else if (file_to == 2) {
                // Queenside castling: check if d1/d8 is attacked
                Move temp_move1 = move; // Create a temporary move to check the move
                temp_move1.square_to = get_index(3, move.square_to / 8); // Move king to d1/d8
                temp_move1.move_type = NORMAL; // Temporarily treat as normal move for check detection
                board.make_move(temp_move1);
                if (is_in_check(board, get_color(move.piece_moved))) {
                    board.unmake_move(temp_move1);
                    continue; // Can't castle through check
                }
                board.unmake_move(temp_move1);
                Move temp_move2 = move; // Create a temporary move to check the move
                temp_move2.square_to = get_index(2, move.square_to / 8); // Move king to c1/c8
                temp_move2.move_type = NORMAL; // Temporarily treat as normal move for check detection
                board.make_move(temp_move2);
                if (is_in_check(board, get_color(move.piece_moved))) {
                    board.unmake_move(temp_move2);
                    continue; // Can't castle through check
                }
                board.unmake_move(temp_move2);
            }
        }
        board.make_move(move);
        if (!is_in_check(board, get_color(move.piece_moved))) {
            legal_moves.push_back(move);
        }
        board.unmake_move(move);
    }
    return legal_moves;
}

std::vector<Move> generate_legal_moves(Board& board, Color color) {
    std::vector<Move> legal_moves;
    for (int i = 0; i < 64; i++) {
        if (board.get_piece(i) != NO_PIECE && get_color(board.get_piece(i)) == color) {
            auto moves = generate_legal_moves(board, i);
            legal_moves.insert(legal_moves.end(), moves.begin(), moves.end());
        }
    }
    return legal_moves;
}

bool is_legal_move(Board &board, Move move) {
    std::vector<Move> legal_moves = generate_legal_moves(board, get_color(move.piece_moved));
    for (const Move& legal_move : legal_moves) {
        if (legal_move.square_from == move.square_from && legal_move.square_to == move.square_to) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// premove_targets
// Returns all squares a piece on `square` could ever reach by its movement
// pattern, regardless of blocking pieces, checks, or castling rights.
// Used to determine valid premove highlight squares.
// ---------------------------------------------------------------------------
std::vector<int> premove_targets(const Board& board, int square)
{
    std::vector<int> targets;
    Piece piece = board.get_piece(square);
    if (piece == NO_PIECE) return targets;

    PieceType pt    = get_type(piece);
    Color     color = get_color(piece);
    int file = square % 8;
    int rank = square / 8;

    auto add = [&](int f, int r) {
        if (f >= 0 && f < 8 && r >= 0 && r < 8)
            targets.push_back(get_index(f, r));
    };
    auto slide = [&](int df, int dr) {
        int f = file + df, r = rank + dr;
        while (f >= 0 && f < 8 && r >= 0 && r < 8) {
            targets.push_back(get_index(f, r));
            f += df; r += dr;
        }
    };

    switch (pt) {
        case KING:
            for (int df : {-1,0,1})
                for (int dr : {-1,0,1})
                    if (df || dr) add(file+df, rank+dr);
            // Castling target squares (always show regardless of rights)
            if (file == 4) { add(6, rank); add(2, rank); }
            break;
        case QUEEN:
            for (int df : {-1,0,1})
                for (int dr : {-1,0,1})
                    if (df || dr) slide(df, dr);
            break;
        case ROOK:
            slide(1,0); slide(-1,0); slide(0,1); slide(0,-1);
            break;
        case BISHOP:
            slide(1,1); slide(1,-1); slide(-1,1); slide(-1,-1);
            break;
        case KNIGHT:
            for (int df : {-2,-1,1,2})
                for (int dr : {-2,-1,1,2})
                    if (std::abs(df) + std::abs(dr) == 3) add(file+df, rank+dr);
            break;
        case PAWN: {
            int dir = (color == WHITE) ? 1 : -1;
            // Forward 1
            add(file, rank + dir);
            // Forward 2 from starting rank
            int startRank = (color == WHITE) ? 1 : 6;
            if (rank == startRank) add(file, rank + 2*dir);
            // Diagonal captures
            add(file-1, rank + dir);
            add(file+1, rank + dir);
            break;
        }
        default: break;
    }
    return targets;
}