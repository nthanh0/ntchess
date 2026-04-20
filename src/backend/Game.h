#ifndef GAME_H
#define GAME_H

#include "Board.h"
#include "movegen.h"
#include "types.h"
#include <chrono>
#include <string>
#include <vector>

class Game {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    Game();

    // Load a position from a FEN string instead of the standard starting position.
    // Must be called before any moves are made.
    bool set_start_fen(const std::string& fen);

    // Returns the FEN string passed to set_start_fen(), or "" for startpos.
    const std::string& start_fen() const { return start_fen_; }

    const Board&   get_board()  const;
    Color          get_turn()   const;
    GameStatus     get_game_status() const;
    const std::vector<Move>&        get_move_history()     const;
    const std::vector<std::string>& get_move_history_san() const;

    // Returns false if the move is illegal or the game is already over.
    // If clocks are set, automatically ticks the moving side's clock and
    // applies the increment.
    bool make_move(const Move& move);

    void check_game_status();

    // Set clocks for both sides
    // time_ms  – initial time in milliseconds
    // inc_ms   – increment per move in milliseconds
    void set_clocks(long long white_ms, long long black_ms,
                    long long white_inc_ms = 0, long long black_inc_ms = 0);

    // Start the clock for the side to move
    void start_clock();

    // Stop the clock for the currently-running side, deduct elapsed time,
    // and add the increment.  Returns the time used in ms.
    long long stop_clock();

    // Check if either clock is currently running
    bool clock_running() const;

    // Time remaining for a side in milliseconds (-inf if no clock set).
    long long remaining_ms(Color color) const;

    // Check if clocks are set up
    bool has_clocks() const { return clocks_set_; }

    // Poll from the UI timer: if the active clock has expired, set status=TIMEOUT.
    // Returns true if a timeout was just detected.
    bool flag_if_timed_out();

private:
    Board board;
    Color turn = WHITE;
    GameStatus status = ONGOING;
    std::string start_fen_; // empty = standard starting position
    std::vector<Move> move_history;
    std::vector<std::string> move_history_san;

    // Repetition detection
    std::vector<Position> position_history;
    Position make_position() const;

    // Clocks
    bool  clocks_set_ = false;
    Clock white_clock_;
    Clock black_clock_;
    bool  clock_running_ = false;
    TimePoint clock_start_;
    Color clock_owner_ = WHITE; // which side's clock is ticking
};


#endif
