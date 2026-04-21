#include "uci_engine.h"
#include "notation.h"

#include <algorithm>
#include <sstream>

#include <QStringList>
#include <QDebug>
#include <QElapsedTimer>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

// Split a string on whitespace into tokens.
std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream ss(s);
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
}

// Return the value that follows a named token in a token list, or "".
std::string token_after(const std::vector<std::string>& toks,
                        const std::string& key) {
    for (size_t i = 0; i + 1 < toks.size(); ++i)
        if (toks[i] == key) return toks[i + 1];
    return {};
}

// Collect all tokens that follow a named token until the next known keyword.
std::string tokens_after(const std::vector<std::string>& toks,
                         const std::string& key,
                         const std::vector<std::string>& stop_keys) {
    size_t start = toks.size();
    for (size_t i = 0; i < toks.size(); ++i)
        if (toks[i] == key) { start = i + 1; break; }
    if (start >= toks.size()) return {};
    std::string result;
    for (size_t i = start; i < toks.size(); ++i) {
        if (std::find(stop_keys.begin(), stop_keys.end(), toks[i]) !=
            stop_keys.end()) break;
        if (!result.empty()) result += ' ';
        result += toks[i];
    }
    return result;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Ctor / Dtor
// ---------------------------------------------------------------------------
UCIEngine::UCIEngine() = default;

UCIEngine::~UCIEngine() {
    if (is_running()) quit();
    delete process_;
}

// ---------------------------------------------------------------------------
// Process management
// ---------------------------------------------------------------------------
bool UCIEngine::start(const std::string& path,
                      const std::vector<std::string>& args) {
    if (is_running()) return false;

    delete process_;
    process_ = new QProcess();

    QStringList qargs;
    for (const auto& a : args)
        qargs << QString::fromStdString(a);

    process_->setReadChannel(QProcess::StandardOutput);
    process_->start(QString::fromStdString(path), qargs);

    if (!process_->waitForStarted(5000)) {
        delete process_;
        process_ = nullptr;
        return false;
    }
    return true;
}

void UCIEngine::quit() {
    if (!process_) return;

    send("quit");

    if (!process_->waitForFinished(2000))
        process_->kill();

    process_->waitForFinished(1000);
    delete process_;
    process_ = nullptr;
}

bool UCIEngine::is_running() const {
    return process_ && process_->state() != QProcess::NotRunning;
}

// ---------------------------------------------------------------------------
// Raw I/O
// ---------------------------------------------------------------------------
void UCIEngine::send(const std::string& cmd) {
    if (!process_) return;
    std::string msg = cmd + '\n';
    process_->write(msg.c_str(), static_cast<qint64>(msg.size()));
}

std::string UCIEngine::read_line(int timeout_ms) {
    if (!process_) return {};

    // Wait until a full line is available.
    // waitForReadyRead may return with partial data, so loop until canReadLine.
    QElapsedTimer timer;
    timer.start();

    while (!process_->canReadLine()) {
        int remaining = (timeout_ms < 0)
            ? -1
            : static_cast<int>(timeout_ms - timer.elapsed());
        if (remaining == 0) return {};
        if (!process_->waitForReadyRead(remaining))
            return {}; // timeout or error
    }

    return process_->readLine().trimmed().toStdString();
}

// ---------------------------------------------------------------------------
// UCI protocol
// ---------------------------------------------------------------------------
bool UCIEngine::init(int timeout_ms) {
    send("uci");

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeout_ms) {
        int remaining = static_cast<int>(timeout_ms - timer.elapsed());
        std::string line = read_line(remaining);
        if (line.empty()) {
            if (!is_running()) break;  // engine exited
            continue;                  // skip blank lines
        }

        if (line.rfind("id name ", 0) == 0)
            engine_name_ = line.substr(8);
        else if (line.rfind("id author ", 0) == 0)
            engine_author_ = line.substr(10);
        else if (line.rfind("option ", 0) == 0)
            parse_option_line(line);
        else if (line == "uciok")
            return true;
    }
    return false;
}

bool UCIEngine::wait_ready(int timeout_ms) {
    qDebug() << "[UCIEngine] >> isready";
    send("isready");

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeout_ms) {
        int remaining = static_cast<int>(timeout_ms - timer.elapsed());
        std::string line = read_line(remaining);
        if (line.empty()) {
            if (!is_running()) break;
            continue;
        }
        if (line == "readyok") { qDebug() << "[UCIEngine] << readyok"; return true; }
    }
    qDebug() << "[UCIEngine] wait_ready TIMED OUT";
    return false;
}

void UCIEngine::new_game() {
    send("ucinewgame");
}

void UCIEngine::set_option(const std::string& name, const std::string& value) {
    std::string cmd = "setoption name " + name + " value " + value;
    qDebug() << "[UCIEngine] >>" << QString::fromStdString(cmd);
    send(cmd);
}

// ---------------------------------------------------------------------------
// Position
// ---------------------------------------------------------------------------
void UCIEngine::set_position_startpos(const std::vector<std::string>& moves) {
    std::string cmd = "position startpos";
    if (!moves.empty()) {
        cmd += " moves";
        for (const auto& m : moves) { cmd += ' '; cmd += m; }
    }
    send(cmd);
}

void UCIEngine::set_position_fen(const std::string& fen,
                                  const std::vector<std::string>& moves) {
    std::string cmd = "position fen " + fen;
    if (!moves.empty()) {
        cmd += " moves";
        for (const auto& m : moves) { cmd += ' '; cmd += m; }
    }
    send(cmd);
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------
void UCIEngine::go(const UCIGoOptions& opts) {
    std::string cmd = "go";

    if (opts.infinite) {
        cmd += " infinite";
    } else {
        if (opts.movetime  >= 0) cmd += " movetime "  + std::to_string(opts.movetime);
        if (opts.depth     >= 0) cmd += " depth "     + std::to_string(opts.depth);
        if (opts.wtime     >= 0) cmd += " wtime "     + std::to_string(opts.wtime);
        if (opts.btime     >= 0) cmd += " btime "     + std::to_string(opts.btime);
        if (opts.winc      >  0) cmd += " winc "      + std::to_string(opts.winc);
        if (opts.binc      >  0) cmd += " binc "      + std::to_string(opts.binc);
        if (opts.movestogo >= 0) cmd += " movestogo " + std::to_string(opts.movestogo);
    }
    send(cmd);
}

void UCIEngine::stop() {
    send("stop");
}

// ---------------------------------------------------------------------------
// Wait for bestmove
// ---------------------------------------------------------------------------
UCISearchResult UCIEngine::wait_for_bestmove(int timeout_ms) {
    UCISearchResult result;

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeout_ms) {
        int remaining = static_cast<int>(timeout_ms - timer.elapsed());
        std::string line = read_line(remaining);
        if (line.empty()) break;

        auto toks = tokenize(line);
        if (toks.empty()) continue;

        if (toks[0] == "info") {
            result = parse_info_line(line, result);
        } else if (toks[0] == "bestmove") {
            if (toks.size() >= 2) result.bestmove = toks[1];
            if (toks.size() >= 4 && toks[2] == "ponder") result.ponder = toks[3];
            return result;
        }
    }
    return result; // timeout — bestmove is empty
}

// ---------------------------------------------------------------------------
// wait_for_bestmove_multi – collect up to n MultiPV lines
// ---------------------------------------------------------------------------
std::vector<UCISearchResult> UCIEngine::wait_for_bestmove_multi(int n, int timeout_ms)
{
    std::vector<UCISearchResult> results(n);

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeout_ms) {
        int remaining = static_cast<int>(timeout_ms - timer.elapsed());
        std::string line = read_line(remaining);
        if (line.empty()) break;

        auto toks = tokenize(line);
        if (toks.empty()) continue;

        if (toks[0] == "info") {
            // Determine multipv index from line (default to 1 if not present)
            int pvIdx = 0;
            for (size_t i = 1; i + 1 < toks.size(); ++i) {
                if (toks[i] == "multipv") {
                    pvIdx = std::stoi(toks[i + 1]) - 1; // convert 1-based to 0-based
                    break;
                }
            }
            if (pvIdx >= 0 && pvIdx < n)
                results[pvIdx] = parse_info_line(line, results[pvIdx]);
        } else if (toks[0] == "bestmove") {
            if (toks.size() >= 2) results[0].bestmove = toks[1];
            return results;
        }
    }
    return results; // timeout
}

// ---------------------------------------------------------------------------
// stream_multipv – live MultiPV updates via callback
// ---------------------------------------------------------------------------
std::vector<UCISearchResult> UCIEngine::stream_multipv(
    int n, int timeout_ms,
    std::function<void(const std::vector<UCISearchResult>&)> on_update)
{
    std::vector<UCISearchResult> current(n);
    int lastCallbackDepth = -1;
    int currentSearchDepth = -1; // depth Stockfish is currently computing

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeout_ms) {
        int remaining = static_cast<int>(timeout_ms - timer.elapsed());
        std::string line = read_line(remaining);
        if (line.empty()) break;

        auto toks = tokenize(line);
        if (toks.empty()) continue;

        if (toks[0] == "info") {
            // Determine multipv index (0-based), default 0
            int pvIdx = 0;
            for (size_t i = 1; i + 1 < toks.size(); ++i) {
                if (toks[i] == "multipv") { pvIdx = std::stoi(toks[i+1]) - 1; break; }
            }
            if (pvIdx >= n) pvIdx = n - 1;
            current[pvIdx] = parse_info_line(line, current[pvIdx]);

            int depth = current[0].depth;

            // When pvIdx == 0 transitions to a new depth, the previous depth completed
            // with fewer than n PVs (common in check positions with limited legal moves).
            // Fire the callback now so those positions still get live eval updates.
            if (pvIdx == 0 && depth != currentSearchDepth) {
                if (currentSearchDepth > lastCallbackDepth && currentSearchDepth > 0) {
                    lastCallbackDepth = currentSearchDepth;
                    on_update(current);
                }
                currentSearchDepth = depth;
            }

            // Fire callback whenever we receive the last PV slot at a new depth.
            // pvIdx == n-1 (or pvIdx == 0 when n==1) means we have a full set.
            if (pvIdx == n - 1 && depth > lastCallbackDepth && depth > 0) {
                lastCallbackDepth = depth;
                on_update(current);
            }
        } else if (toks[0] == "bestmove") {
            // Fire for any last depth that completed without a full n-PV set.
            if (currentSearchDepth > lastCallbackDepth && currentSearchDepth > 0)
                on_update(current);
            if (toks.size() >= 2) current[0].bestmove = toks[1];
            return current;
        }
    }
    return current; // timeout
}

// ---------------------------------------------------------------------------
// High-level helper
// ---------------------------------------------------------------------------
UCISearchResult UCIEngine::think(const Game& game, const UCIGoOptions& opts) {
    // Build moves list from game history
    std::vector<std::string> moves;
    for (const Move& m : game.get_move_history())
        moves.push_back(move_to_uci(m));

    if (game.start_fen().empty())
        set_position_startpos(moves);
    else
        set_position_fen(game.start_fen(), moves);
    go(opts);

    // Calculate a generous timeout so wait_for_bestmove never fires before
    // Stockfish finishes thinking:
    //   - movetime: give 3x the requested time as safety margin
    //   - wtime/btime: Stockfish will use at most ~85% of the remaining time
    //     on one move, so remaining_time + a fixed 10 s buffer is enough
    //   - fallback: 30 s
    int timeout;
    if (opts.movetime > 0)
        timeout = opts.movetime * 3;
    else if (opts.wtime > 0 || opts.btime > 0)
        // Stockfish uses at most ~85% of remaining time on one move; add 10 s
        // buffer but cap at 2 minutes so a crashed engine doesn't stall the UI.
        timeout = std::min(std::max(opts.wtime, opts.btime) + 10000, 120000);
    else
        timeout = 30000;

    return wait_for_bestmove(timeout);
}

// ---------------------------------------------------------------------------
// Parsing helpers
// ---------------------------------------------------------------------------
void UCIEngine::parse_option_line(const std::string& line) {
    // option name <id> type <t> [default <x>] [min <x>] [max <x>] [var <x>]*
    auto toks = tokenize(line);
    if (toks.size() < 5) return;

    static const std::vector<std::string> kw =
        {"name","type","default","min","max","var"};

    UCIEngineOption opt;
    opt.name        = tokens_after(toks, "name",    kw);
    opt.type        = token_after (toks, "type");
    opt.default_val = token_after (toks, "default");
    opt.min_val     = token_after (toks, "min");
    opt.max_val     = token_after (toks, "max");

    // Collect all "var" values
    for (size_t i = 0; i + 1 < toks.size(); ++i)
        if (toks[i] == "var") opt.vars.push_back(toks[i + 1]);

    options_.push_back(std::move(opt));
}

UCISearchResult UCIEngine::parse_info_line(const std::string& line,
                                         const UCISearchResult& prev) const {
    UCISearchResult r = prev; // carry forward previous info
    auto toks = tokenize(line);
    if (toks.empty() || toks[0] != "info") return r;

    for (size_t i = 1; i < toks.size(); ++i) {
        if (toks[i] == "depth" && i + 1 < toks.size()) {
            r.depth = std::stoi(toks[++i]);
        } else if (toks[i] == "seldepth" && i + 1 < toks.size()) {
            r.seldepth = std::stoi(toks[++i]);
        } else if (toks[i] == "nodes" && i + 1 < toks.size()) {
            r.nodes = std::stol(toks[++i]);
        } else if (toks[i] == "score") {
            ++i;
            if (i < toks.size() && toks[i] == "cp" && i + 1 < toks.size()) {
                r.score_mate = false;
                r.score_cp   = std::stoi(toks[++i]);
            } else if (i < toks.size() && toks[i] == "mate" && i + 1 < toks.size()) {
                r.score_mate = true;
                r.mate_in    = std::stoi(toks[++i]);
            }
        } else if (toks[i] == "multipv" && i + 1 < toks.size()) {
            r.multipv = std::stoi(toks[++i]);
        } else if (toks[i] == "pv") {
            // Everything from "pv" to end of line is the PV
            std::string pv;
            for (size_t j = i + 1; j < toks.size(); ++j) {
                if (!pv.empty()) pv += ' ';
                pv += toks[j];
            }
            r.pv = std::move(pv);
            break; // pv is always last
        }
    }
    return r;
}
