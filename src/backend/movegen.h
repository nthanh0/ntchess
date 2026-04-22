#ifndef MOVEGEN_H
#define MOVEGEN_H

#include <vector>
#include "Board.h"
#include "types.h"

std::vector<Move> generate_moves(const Board& board, int square);
bool is_in_check(const Board& board, Color color);
std::vector<Move> generate_legal_moves(Board& board, int square);
std::vector<Move> generate_legal_moves(Board& board, Color color);
bool is_legal_move(Board& board, Move move);
// Returns all squares the piece on `square` can reach by pattern (ignoring
// blocking pieces / legality) – used for premove target highlighting.
std::vector<int> premove_targets(const Board& board, int square);

#endif
