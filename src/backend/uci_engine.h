#pragma once

#include "Game.h"

#include <string>
#include <vector>

#include <QObject>
#include <QProcess>

// ---------------------------------------------------------------------------
// Data types shared with UCIEngine users
// ---------------------------------------------------------------------------

/// Result returned after a search completes (bestmove received).
struct UCISearchResult {
    std::string bestmove;   // e.g. "e2e4"
    std::string ponder;     // engine's predicted reply

    int  score_cp   = 0;    // centipawns (valid when !score_mate)
    bool score_mate = false;
    int  mate_in    = 0;    // positive = engine mates, negative = engine is mated

    int  depth    = 0;
    long nodes    = 0;
    int  seldepth = 0;
    std::string pv; // principal variation (space-separated UCI moves)

    bool valid() const { return !bestmove.empty(); }
};

/// Options forwarded to the "go" command.
struct UCIGoOptions {
    int  movetime  = -1;  // fixed ms per move (-1 = not used)
    int  depth     = -1;  // fixed depth limit (-1 = not used)
    int  wtime     = -1;  // white clock ms  (-1 = not used)
    int  btime     = -1;  // black clock ms  (-1 = not used)
    int  winc      =  0;  // white increment ms
    int  binc      =  0;  // black increment ms
    int  movestogo = -1;  // moves until next time control (-1 = not used)
    bool infinite  = false;
};

/// Parsed "option" line sent by the engine during UCI handshake.
struct UCIEngineOption {
    std::string name;
    std::string type;          // "spin","check","combo","button","string"
    std::string default_val;
    std::string min_val;
    std::string max_val;
    std::vector<std::string> vars; // for type "combo"
};

// ---------------------------------------------------------------------------
// UCIEngine – wraps an external UCI chess engine subprocess.
//
// Typical usage:
//   UCIEngine eng;
//   eng.start("/usr/bin/stockfish");
//   eng.init();          // UCI handshake
//   eng.wait_ready();
//   eng.new_game();
//
//   UCIGoOptions opts;
//   opts.movetime = 1000; // 1 second
//   UCISearchResult result = eng.think(game, opts);
//   std::cout << result.bestmove << '\n';
//
//   eng.quit();
// ---------------------------------------------------------------------------
class UCIEngine {
public:
    UCIEngine();
    ~UCIEngine();

    // Non-copyable
    UCIEngine(const UCIEngine&)            = delete;
    UCIEngine& operator=(const UCIEngine&) = delete;

    // ------------------------------------------------------------------
    // Process management
    // ------------------------------------------------------------------

    /// Launch the engine binary at @p path with optional extra @p args.
    bool start(const std::string& path,
               const std::vector<std::string>& args = {});

    /// Send "quit" and wait for the process to exit (or kill after timeout).
    void quit();

    /// True while the engine process is alive.
    bool is_running() const;

    // ------------------------------------------------------------------
    // UCI protocol
    // ------------------------------------------------------------------

    /// Send "uci" and wait for "uciok".  Populates engine name/author/options.
    bool init(int timeout_ms = 5000);

    /// Send "isready" and block until "readyok".
    bool wait_ready(int timeout_ms = 5000);

    /// Send "ucinewgame".
    void new_game();

    /// Send "setoption name <name> value <value>".
    void set_option(const std::string& name, const std::string& value);

    // ------------------------------------------------------------------
    // Position
    // ------------------------------------------------------------------

    /// "position startpos [moves m1 m2 ...]"
    void set_position_startpos(const std::vector<std::string>& moves = {});

    /// "position fen <fen> [moves m1 m2 ...]"
    void set_position_fen(const std::string& fen,
                          const std::vector<std::string>& moves = {});

    // ------------------------------------------------------------------
    // Search
    // ------------------------------------------------------------------

    /// Send "go" with the given options.
    void go(const UCIGoOptions& opts = UCIGoOptions{});

    /// Send "stop".
    void stop();

    /// Block until the engine emits "bestmove", return the result.
    UCISearchResult wait_for_bestmove(int timeout_ms = 30000);

    // ------------------------------------------------------------------
    // High-level helper
    // ------------------------------------------------------------------

    /// Set position from @p game, call go(), and wait for bestmove.
    UCISearchResult think(const Game& game,
                          const UCIGoOptions& opts = UCIGoOptions{});

    // ------------------------------------------------------------------
    // Engine metadata (populated after init())
    // ------------------------------------------------------------------
    const std::string&                 get_engine_name()   const { return engine_name_;   }
    const std::string&                 get_engine_author() const { return engine_author_; }
    const std::vector<UCIEngineOption>& get_options()       const { return options_;       }

    // ------------------------------------------------------------------
    // Raw I/O (advanced use)
    // ------------------------------------------------------------------

    /// Send a raw command string (newline appended automatically).
    void send(const std::string& cmd);

    /// Read one line from the engine, blocking up to @p timeout_ms ms.
    std::string read_line(int timeout_ms = -1);

private:
    QProcess* process_ = nullptr;

    std::string                  engine_name_;
    std::string                  engine_author_;
    std::vector<UCIEngineOption> options_;

    void parse_option_line(const std::string& line);
    UCISearchResult parse_info_line(const std::string& line,
                                    const UCISearchResult& prev) const;
};
