#ifndef BOARD_H
#define BOARD_H

#include <string>
#include <vector>
#include "types.h"

class Board {
    private:
        Piece board[64]; // board[0] = a1, board[1] = b1... board[63] = h8
        std::vector<StateInfo> state_history;
    public:
        Board();
        void setup_pieces();
        bool setup_fen(const std::string& fen); // returns true if black to move
        Piece get_piece(int file, int rank) const;
        Piece get_piece(int square) const;
        void set_piece(int file, int rank, Piece piece);
        void update_state_info(const Move& move);
        void make_move(const Move& move);
        void unmake_move(const Move& move);
        void clear_board();
        StateInfo& get_state_info() { return state_history.back(); }
        const StateInfo& get_state_info() const { return state_history.back(); }
};

#endif
