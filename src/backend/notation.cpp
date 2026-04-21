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
    Color opponent = (get_color(move.piece_moved) == WHITE) ? BLACK : WHITE;
    if (is_in_check(board, opponent)) {
        // Check if opponent has any legal moves
        std::vector<Move> opponent_moves = generate_legal_moves(board, opponent);
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

// ---------------------------------------------------------------------------
// san_to_move
// ---------------------------------------------------------------------------
Move san_to_move(const std::string& san, Board& board, Color turn)
{
    Move invalid(-1, -1, NO_PIECE);
    if (san.empty()) return invalid;

    std::string s = san;
    // Strip trailing check / checkmate / annotation markers
    while (!s.empty() &&
           (s.back() == '+' || s.back() == '#' ||
            s.back() == '!' || s.back() == '?'))
        s.pop_back();
    if (s.empty()) return invalid;

    std::vector<Move> legal = generate_legal_moves(board, turn);

    // ── Castling ──────────────────────────────────────────────────────────
    if (s == "O-O-O" || s == "0-0-0") {
        for (const Move& m : legal)
            if (m.move_type == CASTLING && m.square_to % 8 == 2) return m;
        return invalid;
    }
    if (s == "O-O" || s == "0-0") {
        for (const Move& m : legal)
            if (m.move_type == CASTLING && m.square_to % 8 == 6) return m;
        return invalid;
    }

    // ── Promotion suffix (=Q / =R / =B / =N) ─────────────────────────────
    PieceType promo = NO_PIECE_TYPE;
    if (s.size() >= 2 && s[s.size() - 2] == '=') {
        char pc = static_cast<char>(std::toupper(static_cast<unsigned char>(s.back())));
        if      (pc == 'Q') promo = QUEEN;
        else if (pc == 'R') promo = ROOK;
        else if (pc == 'B') promo = BISHOP;
        else if (pc == 'N') promo = KNIGHT;
        s = s.substr(0, s.size() - 2);
    }

    // ── Destination square: last two chars ────────────────────────────────
    if (s.size() < 2) return invalid;
    int to_file = s[s.size() - 2] - 'a';
    int to_rank = s[s.size() - 1] - '1';
    if (to_file < 0 || to_file > 7 || to_rank < 0 || to_rank > 7) return invalid;
    int to_sq = get_index(to_file, to_rank);
    s = s.substr(0, s.size() - 2);

    // Remove capture 'x'
    if (!s.empty() && s.back() == 'x') s.pop_back();

    // ── Piece type ────────────────────────────────────────────────────────
    PieceType piece_type = PAWN;
    if (!s.empty() && std::isupper(static_cast<unsigned char>(s[0]))) {
        switch (s[0]) {
            case 'N': piece_type = KNIGHT; break;
            case 'B': piece_type = BISHOP; break;
            case 'R': piece_type = ROOK;   break;
            case 'Q': piece_type = QUEEN;  break;
            case 'K': piece_type = KING;   break;
            default:                       break;
        }
        if (piece_type != PAWN)
            s = s.substr(1);
    }

    // ── Disambiguation: remaining chars are file and/or rank ──────────────
    int dis_file = -1, dis_rank = -1;
    for (char c : s) {
        if (c >= 'a' && c <= 'h')      dis_file = c - 'a';
        else if (c >= '1' && c <= '8') dis_rank = c - '1';
    }

    // ── Find matching legal move ──────────────────────────────────────────
    for (const Move& m : legal) {
        if (m.square_to != to_sq) continue;
        Piece p = board.get_piece(m.square_from);
        if (get_type(p)  != piece_type) continue;
        if (get_color(p) != turn)       continue;
        // For promotions: must match the requested promo piece
        if (promo != NO_PIECE_TYPE) {
            if (m.move_type != PROMOTION)          continue;
            if (m.promotion_piece_type != promo)   continue;
        } else if (piece_type == PAWN && m.move_type == PROMOTION) {
            // No =X specified: skip non-queen promotions (default queen)
            if (m.promotion_piece_type != QUEEN)   continue;
        }
        if (dis_file != -1 && m.square_from % 8 != dis_file) continue;
        if (dis_rank != -1 && m.square_from / 8 != dis_rank) continue;
        return m;
    }
    return invalid;
}

// ---------------------------------------------------------------------------
// parse_pgn
// ---------------------------------------------------------------------------
bool parse_pgn(const std::string& pgn_text, Game& out_game)
{
    // ── Extract optional FEN tag ──────────────────────────────────────────
    std::string start_fen;
    {
        const std::string tag = "[FEN \"";
        auto pos = pgn_text.find(tag);
        if (pos != std::string::npos) {
            auto start = pos + tag.size();
            auto end   = pgn_text.find('"', start);
            if (end != std::string::npos)
                start_fen = pgn_text.substr(start, end - start);
        }
    }

    // ── Strip tag headers (lines beginning with '[') ──────────────────────
    std::string moves_text;
    {
        std::istringstream ss(pgn_text);
        std::string line;
        bool past_headers = false;
        while (std::getline(ss, line)) {
            // Trim leading whitespace
            size_t first = line.find_first_not_of(" \t\r");
            if (first == std::string::npos) {
                if (past_headers) moves_text += '\n';
                continue;
            }
            if (line[first] == '[') continue; // tag line – skip
            past_headers = true;
            moves_text += line + '\n';
        }
    }

    // ── Remove ; comments (rest of line) ─────────────────────────────────
    {
        std::string cleaned;
        cleaned.reserve(moves_text.size());
        for (size_t i = 0; i < moves_text.size(); ++i) {
            if (moves_text[i] == ';') {
                while (i < moves_text.size() && moves_text[i] != '\n') ++i;
            } else {
                cleaned += moves_text[i];
            }
        }
        moves_text = std::move(cleaned);
    }

    // ── Remove { } comments ──────────────────────────────────────────────
    {
        std::string cleaned;
        cleaned.reserve(moves_text.size());
        int depth = 0;
        for (char c : moves_text) {
            if      (c == '{') { ++depth; }
            else if (c == '}') { if (depth > 0) --depth; }
            else if (depth == 0) cleaned += c;
        }
        moves_text = std::move(cleaned);
    }

    // ── Remove ( ) variations ─────────────────────────────────────────────
    {
        std::string cleaned;
        cleaned.reserve(moves_text.size());
        int depth = 0;
        for (char c : moves_text) {
            if      (c == '(') { ++depth; }
            else if (c == ')') { if (depth > 0) --depth; }
            else if (depth == 0) cleaned += c;
        }
        moves_text = std::move(cleaned);
    }

    // ── Tokenise ─────────────────────────────────────────────────────────
    std::istringstream ss(moves_text);
    std::vector<std::string> tokens;
    {
        std::string tok;
        while (ss >> tok) tokens.push_back(tok);
    }

    // ── Build game ────────────────────────────────────────────────────────
    out_game = Game();
    if (!start_fen.empty())
        out_game.set_start_fen(start_fen);

    Color turn = start_fen.empty() ? WHITE
                                   : (start_fen.find(" b ") != std::string::npos ? BLACK : WHITE);

    const std::string results[] = { "1-0", "0-1", "1/2-1/2", "*" };

    for (const std::string& tok : tokens) {
        // Skip result tokens
        bool is_result = false;
        for (const auto& r : results) if (tok == r) { is_result = true; break; }
        if (is_result) break;

        // Skip NAG annotations ($N)
        if (!tok.empty() && tok[0] == '$') continue;

        // Strip a leading move-number prefix like "1.", "12.", "1..." etc.
        // This handles both "1. e4" (separate token "1.") and "1.e4" (merged).
        std::string san = tok;
        {
            size_t i = 0;
            while (i < san.size() && std::isdigit(static_cast<unsigned char>(san[i]))) ++i;
            if (i > 0 && i < san.size() && san[i] == '.') {
                // skip all trailing dots
                while (i < san.size() && san[i] == '.') ++i;
                san = san.substr(i);
            }
        }
        // After stripping, might be empty (was a pure move-number token like "1.")
        if (san.empty()) continue;
        // Skip pure move-number tokens that had no SAN attached
        {
            bool all_digits_dots = true;
            for (char c : san)
                if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.') { all_digits_dots = false; break; }
            if (all_digits_dots) continue;
        }

        // Parse as SAN
        Board cur_board = out_game.get_board(); // take a copy so san_to_move can inspect it
        Move m = san_to_move(san, cur_board, turn);
        if (m.square_from == -1) return false; // illegal / unrecognised

        if (!out_game.make_move(m)) return false;
        turn = (turn == WHITE) ? BLACK : WHITE;
    }

    out_game.check_game_status();
    return true;
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