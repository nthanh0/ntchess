#include "notation.h"
#include "movegen.h"
#include "misc.h"
#include <cctype>
#include <ctime>
#include <sstream>

#include "Game.h"

char piece_to_char(Piece piece) {
    char c = ' ';
    switch (piece) {
        case W_PAWN: c = 'P'; break;
        case W_KNIGHT: c = 'N'; break;
        case W_BISHOP: c = 'B'; break;
        case W_ROOK: c = 'R'; break;
        case W_QUEEN: c = 'Q'; break;
        case W_KING: c = 'K'; break;
        case B_PAWN: c = 'p'; break;
        case B_KNIGHT: c = 'n'; break;
        case B_BISHOP: c = 'b'; break;
        case B_ROOK: c = 'r'; break;
        case B_QUEEN: c = 'q'; break;
        case B_KING: c = 'k'; break;
        default: c = ' '; break;
    }
    return c;
}

Piece char_to_piece(char c) {
    switch (c) {
        case 'P': return W_PAWN;
        case 'N': return W_KNIGHT;
        case 'B': return W_BISHOP;
        case 'R': return W_ROOK;
        case 'Q': return W_QUEEN;
        case 'K': return W_KING;
        case 'p': return B_PAWN;
        case 'n': return B_KNIGHT;
        case 'b': return B_BISHOP;
        case 'r': return B_ROOK;
        case 'q': return B_QUEEN;
        case 'k': return B_KING;
        default: return NO_PIECE;
    }
}

std::string piece_to_unicode(Piece piece) {
    switch (piece) {
        case W_PAWN:   return "♙";
        case W_KNIGHT: return "♘";
        case W_BISHOP: return "♗";
        case W_ROOK:   return "♖";
        case W_QUEEN:  return "♕";
        case W_KING:   return "♔";
        case B_PAWN:   return "♟";
        case B_KNIGHT: return "♞";
        case B_BISHOP: return "♝";
        case B_ROOK:   return "♜";
        case B_QUEEN:  return "♛";
        case B_KING:   return "♚";
        default:       return " ";
    }
}

// Convert a move to UCI format. E.g. e2e4, g1f3, etc.
std::string move_to_uci(Move move) {
    std::string result = "";
    // Convert from square index to file and rank
    int from_file = move.square_from % 8;
    int from_rank = move.square_from / 8;
    int to_file = move.square_to % 8;
    int to_rank = move.square_to / 8;
    result += ('a' + from_file);
    result += ('1' + from_rank);
    result += ('a' + to_file);
    result += ('1' + to_rank);
    // Handle promotion
    if (move.move_type == PROMOTION) {
        switch (move.promotion_piece_type) {
            case QUEEN: result += 'q'; break;
            case ROOK: result += 'r'; break;
            case BISHOP: result += 'b'; break;
            case KNIGHT: result += 'n'; break;
            default: break;
        }
    }
    return result;
}

std::string move_to_san(Board& board, Move move) {
    std::string result = "";
    int from_file = move.square_from % 8;
    int from_rank = move.square_from / 8;
    int to_file = move.square_to % 8;
    int to_rank = move.square_to / 8;
    // Castling
    if (move.move_type == CASTLING) {
        if (to_file == 6) {
            return "O-O";
        } else if (to_file == 2) {
            return "O-O-O";
        }
    }
    // Pawn moves
    if (board.get_piece(move.square_from) == W_PAWN || board.get_piece(move.square_from) == B_PAWN) {
        if (is_capture(move)) {
            result += ('a' + from_file);
            result += 'x';
        }
        result += ('a' + to_file);
        result += ('1' + to_rank);
        // Promotion
        if (move.move_type == PROMOTION) {
            switch (move.promotion_piece_type) {
                case QUEEN: result += "=Q"; break;
                case ROOK: result += "=R"; break;
                case BISHOP: result += "=B"; break;
                case KNIGHT: result += "=N"; break;
                default: break;
            }
        }
    } else {
        // Piece moves
        result += toupper(piece_to_char(board.get_piece(move.square_from)));
        bool need_file = false;
        bool need_rank = false;
        // Check for disambiguation
        for (int i = 0; i < 64; ++i) {
            if (i == move.square_from) continue;
            if (board.get_piece(i) == board.get_piece(move.square_from)) {
                Move test_move = Move(i, move.square_to, board.get_piece(i), board.get_piece(move.square_to), move.move_type, move.promotion_piece_type);
                if (is_legal_move(board, test_move)) {
                    if (i % 8 == from_file) {
                        need_rank = true;
                    } else if (i / 8 == from_rank) {
                        need_file = true;
                    } else {
                        need_file = true;
                    }
                }
            }
        }
        if (need_file) {
            result += ('a' + from_file);
        }
        if (need_rank) {
            result += ('1' + from_rank);
        }

        if (is_capture(move)) {
            result += 'x';
        }
        result += ('a' + to_file);
        result += ('1' + to_rank);
    }
    // Check or checkmate
    board.make_move(move);
    if (is_in_check(board, get_color(move.piece_moved))) {
        // Check if opponent has any legal moves
        std::vector<Move> opponent_moves = generate_legal_moves(board, (get_color(move.piece_moved) == WHITE) ? BLACK : WHITE);
        if (opponent_moves.empty()) {
            result += '#'; // Checkmate
        } else {
            result += '+'; // Check
        }
    }
    board.unmake_move(move);
    return result;
}

std::string game_to_fen(const Game& game) {
    const Board& board = game.get_board();
    std::string fen = "";
    for (int rank = 7; rank >= 0; rank--) {
        int empty_count = 0;
        for (int file = 0; file < 8; file++) {
            Piece piece = board.get_piece(get_index(file, rank));
            if (piece == NO_PIECE) {
                empty_count++;
            } else {
                if (empty_count > 0) {
                    fen += std::to_string(empty_count);
                    empty_count = 0;
                }
                fen += piece_to_char(piece);
            }
        }
        if (empty_count > 0) {
            fen += std::to_string(empty_count);
        }
        if (rank > 0) {
            fen += '/';
        }
    }
    // Active color
    fen += ' ';
    fen += (game.get_turn() == WHITE) ? 'w' : 'b';
    // Castling rights
    fen += ' ';
    bool can_white_kingside = false, can_white_queenside = false, can_black_kingside = false, can_black_queenside = false;
    if (board.get_state_info().can_castle_kingside(WHITE)) can_white_kingside = true;
    if (board.get_state_info().can_castle_queenside(WHITE)) can_white_queenside = true;
    if (board.get_state_info().can_castle_kingside(BLACK)) can_black_kingside = true;
    if (board.get_state_info().can_castle_queenside(BLACK)) can_black_queenside = true;
    
    if (can_white_kingside) {
        fen += 'K';
    }    
    if (can_white_queenside) {
        fen += 'Q';
    }    
    if (can_black_kingside) {
        fen += 'k';
    }    
    if (can_black_queenside) {
        fen += 'q';
    }
    if (!can_white_kingside && !can_white_queenside && !can_black_kingside && !can_black_queenside) {
        fen += '-';
    }
    // En passant target square
    fen += ' ';
    if (board.get_state_info().en_passant_file != -1) {
        int ep_rank = (game.get_turn() == WHITE) ? 5 : 2;
        fen += ('a' + board.get_state_info().en_passant_file);
        fen += ('1' + ep_rank);
    } else {
        fen += '-';
    }
    // Halfmove clock and fullmove number
    fen += ' ';
    fen += std::to_string(board.get_state_info().halfmove_clock);
    fen += ' ';
    fen += std::to_string(game.get_move_history().size() / 2 + 1);
    return fen;
}

// ---------------------------------------------------------------------------
// uci_to_move
// ---------------------------------------------------------------------------
Move uci_to_move(const std::string& uci, Board& board, Color turn) {
    // Sentinel for "invalid"
    Move invalid(-1, -1, NO_PIECE);

    if (uci.size() < 4) return invalid;

    int from_file = uci[0] - 'a';
    int from_rank = uci[1] - '1';
    int to_file   = uci[2] - 'a';
    int to_rank   = uci[3] - '1';

    if (from_file < 0 || from_file > 7) return invalid;
    if (from_rank < 0 || from_rank > 7) return invalid;
    if (to_file   < 0 || to_file   > 7) return invalid;
    if (to_rank   < 0 || to_rank   > 7) return invalid;

    int from_sq = get_index(from_file, from_rank);
    int to_sq   = get_index(to_file,   to_rank);

    // Optional promotion piece
    PieceType promo = NO_PIECE_TYPE;
    if (uci.size() >= 5) {
        switch (std::tolower(static_cast<unsigned char>(uci[4]))) {
            case 'q': promo = QUEEN;  break;
            case 'r': promo = ROOK;   break;
            case 'b': promo = BISHOP; break;
            case 'n': promo = KNIGHT; break;
        }
    }

    // Find a matching legal move
    std::vector<Move> legal = generate_legal_moves(board, turn);
    for (const Move& m : legal) {
        if (m.square_from != from_sq) continue;
        if (m.square_to   != to_sq)   continue;
        if (promo != NO_PIECE_TYPE && m.promotion_piece_type != promo) continue;
        return m;
    }
    return invalid;
}

std::string game_to_pgn(const Game& game,
                         const std::string& whitePlayer,
                         const std::string& blackPlayer,
                         const std::string& eventName,
                         const std::string& siteName) {
    // ── result string ────────────────────────────────────────────────────
    std::string result;
    switch (game.get_game_status()) {
    case CHECKMATE:
    case TIMEOUT:
        // currentTurn() is the side that lost (was mated / ran out of time)
        result = (game.get_turn() == BLACK) ? "1-0" : "0-1";
        break;
    case STALEMATE:
    case DRAW_BY_REPETITION:
    case DRAW_BY_FIFTY_MOVE_RULE:
    case DRAW_BY_INSUFFICIENT_MATERIAL:
        result = "1/2-1/2";
        break;
    default:
        result = "*";
        break;
    }

    // ── date (YYYY.MM.DD) ────────────────────────────────────────────────
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    char dateBuf[16];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y.%m.%d", tm);

    // ── seven-tag roster ─────────────────────────────────────────────────
    std::string pgn;
    pgn += "[Event \"" + eventName   + "\"]\n";
    pgn += "[Site \""  + siteName    + "\"]\n";
    pgn += "[Date \""  + std::string(dateBuf) + "\"]\n";
    pgn += "[Round \"?\"]\n";
    pgn += "[White \"" + whitePlayer + "\"]\n";
    pgn += "[Black \"" + blackPlayer + "\"]\n";
    pgn += "[Result \"" + result + "\"]\n";

    // optional SetUp / FEN tags for non-standard starting positions
    if (!game.start_fen().empty()) {
        pgn += "[SetUp \"1\"]\n";
        pgn += "[FEN \"" + game.start_fen() + "\"]\n";
    }

    pgn += '\n';

    // ── move text ────────────────────────────────────────────────────────
    const std::vector<std::string>& san_moves = game.get_move_history_san();
    int col = 0;
    for (size_t i = 0; i < san_moves.size(); i++) {
        std::string token;
        if (i % 2 == 0)
            token = std::to_string(i / 2 + 1) + ". " + san_moves[i];
        else
            token = san_moves[i];

        // Wrap at ~80 chars
        if (col > 0 && col + 1 + (int)token.size() > 80) {
            pgn += '\n';
            col = 0;
        } else if (col > 0) {
            pgn += ' ';
            col++;
        }
        pgn += token;
        col += (int)token.size();
    }

    if (!san_moves.empty()) { pgn += ' '; col++; }
    if (col + (int)result.size() > 80) pgn += '\n';
    pgn += result + '\n';

    return pgn;
}