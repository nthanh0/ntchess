#ifndef GAMEBRIDGE_H
#define GAMEBRIDGE_H

#include <QObject>
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>
#include <atomic>
#include <sstream>
#include <thread>
#include "../backend/Game.h"
#include "../backend/notation.h"
#include "../backend/uci_engine.h"

// ---------------------------------------------------------------------------
// Self-contained settings: store an INI file beside the executable instead of
// writing to the registry (Windows) or ~/.config (Linux/macOS).
inline QSettings makeSettings()
{
    return QSettings(
        QCoreApplication::applicationDirPath() + "/settings.ini",
        QSettings::IniFormat);
}

// ---------------------------------------------------------------------------
// EngineWorker – owns UCIEngine and lives on m_engineThread.
// Inputs are set from the main thread (safe when !thinking), then doThink()
// is invoked via QueuedConnection so it runs on the worker thread.
// ---------------------------------------------------------------------------
class EngineWorker : public QObject
{
    Q_OBJECT
public:
    explicit EngineWorker(QObject* parent = nullptr) : QObject(parent) {}

    bool startEngine(const std::string& path) {
        bool ok = m_engine.start(path);
        if (ok) {
            m_engine.init();
            m_engine.wait_ready();
            unsigned int threads = (m_threads > 0)
                ? static_cast<unsigned int>(m_threads)
                : std::thread::hardware_concurrency();
            m_engine.set_option("Threads", std::to_string(std::max(1u, threads)));
            if (m_elo > 0) {
                m_engine.set_option("UCI_LimitStrength", "true");
                m_engine.set_option("UCI_Elo", std::to_string(m_elo));
            } else {
                m_engine.set_option("UCI_LimitStrength", "false");
            }
            m_engine.wait_ready();
        }
        return ok;
    }
    void setThreads(int n) {
        m_threads = n;
        unsigned int threads = (n > 0)
            ? static_cast<unsigned int>(n)
            : std::thread::hardware_concurrency();
        qDebug() << "[EngineWorker] setThreads called, applying" << threads << "threads";
        m_engine.set_option("Threads", std::to_string(std::max(1u, threads)));
        m_engine.send("ucinewgame"); // force Stockfish to rebuild thread pool
        m_engine.wait_ready();       // wait until acknowledged
    }
    int  m_threads = 0;  // 0 = use hardware_concurrency
    int  m_elo     = 0;  // 0 = no strength limit
    void setElo(int elo) {
        m_elo = elo;
        if (elo > 0) {
            m_engine.set_option("UCI_LimitStrength", "true");
            m_engine.set_option("UCI_Elo", std::to_string(elo));
        } else {
            m_engine.set_option("UCI_LimitStrength", "false");
        }
        m_engine.wait_ready();
    }
    void newGame()  { m_engine.new_game(); }
    QString engineName() const { return QString::fromStdString(m_engine.get_engine_name()); }

    // Called from main thread before invoking doThink() via queued connection
    void setInputs(const Game& game, const UCIGoOptions& opts, int pendingElo = -1) {
        m_pendingGame = game;
        m_pendingOpts = opts;
        m_pendingElo  = pendingElo;
    }

    // Safe to call from any thread – sends "stop" so the engine exits think()
    // immediately, unblocking the worker thread.
    void stopSearch() { m_engine.stop(); }

public slots:
    void doThink() {
        // Apply per-move elo (needed for CvC where each side may differ)
        if (m_pendingElo >= 0 && m_pendingElo != m_elo)
            setElo(m_pendingElo);
        UCISearchResult r = m_engine.think(m_pendingGame, m_pendingOpts);
        emit thinkFinished(r);
    }

signals:
    void thinkFinished(UCISearchResult result);

private:
    UCIEngine    m_engine;
    Game         m_pendingGame;
    UCIGoOptions m_pendingOpts;
    int          m_pendingElo = -1; // -1 = no change
};

Q_DECLARE_METATYPE(UCISearchResult)

// ---------------------------------------------------------------------------
// AnalysisWorker – dedicated engine for continuous background analysis.
// Lives on m_analysisThread.  doAnalyze() is invoked via QueuedConnection
// with the position encoded as arguments, so there is no shared mutable state
// between threads.  Each call does ONE fixed-time search then emits signals:
//   evalUpdate   – carries the analysis result (connected to GameBridge)
//   searchDone   – GameBridge re-queues the next doAnalyze if still running
// To cancel simply stop re-queuing (no queued stop call needed).
// ---------------------------------------------------------------------------
class AnalysisWorker : public QObject
{
    Q_OBJECT
public:
    explicit AnalysisWorker(QObject* parent = nullptr) : QObject(parent) {}

    bool startEngine(const std::string& path, int threads) {
        bool ok = m_engine.start(path);
        if (ok) {
            m_engine.init();
            m_engine.wait_ready();
            unsigned int t = (threads > 0)
                ? static_cast<unsigned int>(threads)
                : std::thread::hardware_concurrency();
            m_engine.set_option("Threads", std::to_string(std::max(1u, t)));
            m_engine.set_option("UCI_LimitStrength", "false");
            m_engine.set_option("MultiPV", "3");
            m_engine.wait_ready();
        }
        return ok;
    }

    // Safe to call from any thread – sends "stop" to abort the current search.
    void stopSearch() { m_engine.stop(); }

    // Incremented each time the position changes so stale results from the
    // previous search can be discarded inside doAnalyze.
    std::atomic<int> generation{0};

    // Maximum search depth (0 = unlimited / go infinite).
    // Written from the main thread before any doAnalyze call; read inside doAnalyze
    // on the worker thread.  Atomic so no lock is needed.
    std::atomic<int> m_maxEngineDepth{0};

    QString engineName() const { return QString::fromStdString(m_engine.get_engine_name()); }

public slots:
    // Each call does one movetime search then exits, returning control to the
    // event loop so pending queued invocations can be processed promptly.
    void doAnalyze(QString fen, QStringList moves, int turn) {
        std::vector<std::string> mvs;
        mvs.reserve(moves.size());
        for (const QString& mv : moves)
            mvs.push_back(mv.toStdString());

        if (fen.isEmpty())
            m_engine.set_position_startpos(mvs);
        else
            m_engine.set_position_fen(fen.toStdString(), mvs);

        UCIGoOptions opts;
        int maxD = m_maxEngineDepth.load(std::memory_order_acquire);
        if (maxD > 0)
            opts.depth  = maxD;
        else
            opts.infinite = true;
        m_engine.go(opts);

        // Capture the generation counter BEFORE we start streaming.  If the
        // position changes while we are searching, restartAnalysisIfRunning()
        // calls stopSearch() AND increments generation, so every emit inside
        // this invocation will be discarded as stale.
        int myGen = generation.load(std::memory_order_acquire);

        // Rebuild the game at the analysis position for UCI→SAN conversion.
        Game posGame;
        if (!fen.isEmpty())
            posGame.set_start_fen(fen.toStdString());
        for (const std::string& uci : mvs) {
            Board b = posGame.get_board();
            Move m = uci_to_move(uci, b, posGame.get_turn());
            if (m.square_from == -1) break;
            posGame.make_move(m);
        }

        // Helper: convert a raw lines vector to a QVariantList of SAN lines
        // and emit evalUpdate + topLinesUpdate.
        auto emitLines = [&](const std::vector<UCISearchResult>& rawLines) {
            // Drop results if a new position was set after this search started.
            if (generation.load(std::memory_order_acquire) != myGen) return;

            QVariantList topLines;
            bool emittedEval = false;
            for (const auto& r : rawLines) {
                if (r.pv.empty() && r.depth == 0) continue;

                UCISearchResult scored = r;
                if (!scored.score_mate) {
                    if (turn == 1) scored.score_cp = -scored.score_cp;
                } else {
                    if (turn == 1) scored.mate_in = -scored.mate_in;
                }

                if (!emittedEval) {
                    emit evalUpdate(scored);
                    emittedEval = true;
                }

                // Convert PV UCI moves to SAN (up to 8 ply).
                std::istringstream pvSs(r.pv);
                std::string uci;
                Game pvGame = posGame;
                std::string sanLine;
                int pvPly = 0;
                int firstMoveFrom = -1, firstMoveTo = -1;
                while (pvSs >> uci && pvPly < 8) {
                    Board b = pvGame.get_board();
                    Color c = pvGame.get_turn();
                    Move m = uci_to_move(uci, b, c);
                    if (m.square_from == -1) break;

                    if (pvPly == 0) {
                        firstMoveFrom = m.square_from;
                        firstMoveTo   = m.square_to;
                    }

                    if (c == WHITE) {
                        int mn = (int)pvGame.get_move_history().size() / 2 + 1;
                        if (!sanLine.empty()) sanLine += ' ';
                        sanLine += std::to_string(mn) + '.';
                    } else if (pvPly == 0) {
                        int mn = (int)pvGame.get_move_history().size() / 2 + 1;
                        sanLine += std::to_string(mn) + "...";
                    }

                    Board bCopy = b;
                    std::string san = move_to_san(bCopy, m);
                    if (!sanLine.empty() && sanLine.back() != '.') sanLine += ' ';
                    sanLine += san;

                    pvGame.make_move(m);
                    ++pvPly;
                }

                QVariantMap line;
                line[QStringLiteral("score_cp")]   = scored.score_cp;
                line[QStringLiteral("score_mate")] = scored.score_mate;
                line[QStringLiteral("mate_in")]    = scored.mate_in;
                line[QStringLiteral("depth")]      = r.depth;
                line[QStringLiteral("pv_san")]     = QString::fromStdString(sanLine);
                line[QStringLiteral("move_from")]  = firstMoveFrom;
                line[QStringLiteral("move_to")]    = firstMoveTo;
                topLines.append(line);
            }
            if (!topLines.isEmpty())
                emit topLinesUpdate(topLines);
        };

        // Stream: fire emitLines on every completed depth, block until bestmove.
        std::vector<UCISearchResult> finalLines =
            m_engine.stream_multipv(3, 600000,
                [&](const std::vector<UCISearchResult>& lines) { emitLines(lines); });

        // Emit once more for the final state (in case the last depth was never
        // completed cleanly, e.g. stopped mid-depth).
        emitLines(finalLines);

        emit searchDone(fen, moves, turn);
    }

signals:
    void evalUpdate(UCISearchResult result);
    void topLinesUpdate(QVariantList lines);
    void searchDone(QString fen, QStringList moves, int turn);

private:
    UCIEngine m_engine;
};

class GameBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList pieces        READ pieces        NOTIFY positionChanged)
    Q_PROPERTY(int          currentTurn   READ currentTurn   NOTIFY positionChanged)
    Q_PROPERTY(QString      gameStatus    READ gameStatus    NOTIFY positionChanged)
    Q_PROPERTY(QStringList  moveHistorySan READ moveHistorySan NOTIFY positionChanged)
    Q_PROPERTY(QVariantList capturedByWhite READ capturedByWhite NOTIFY positionChanged)
    Q_PROPERTY(QVariantList capturedByBlack READ capturedByBlack NOTIFY positionChanged)

    Q_PROPERTY(QString boardTheme READ boardTheme WRITE setBoardTheme NOTIFY boardThemeChanged)
    Q_PROPERTY(QString pieceTheme READ pieceTheme WRITE setPieceTheme NOTIFY pieceThemeChanged)
    Q_PROPERTY(int     pieceSize  READ pieceSize  WRITE setPieceSize  NOTIFY pieceSizeChanged)

    Q_PROPERTY(QString enginePath      READ enginePath      NOTIFY enginePathChanged)
    Q_PROPERTY(QString enginePathBlack READ enginePathBlack NOTIFY enginePathBlackChanged)
    Q_PROPERTY(QString engineName      READ engineName      NOTIFY engineNameChanged)
    Q_PROPERTY(QString engineNameBlack READ engineNameBlack NOTIFY engineNameBlackChanged)
    Q_PROPERTY(QString enginePathError READ enginePathError NOTIFY enginePathErrorChanged)
    Q_PROPERTY(QString enginePathWhiteGame READ enginePathWhiteGame NOTIFY enginePathWhiteGameChanged)
    Q_PROPERTY(int     engineThreads   READ engineThreads   NOTIFY engineThreadsChanged)
    Q_PROPERTY(int     engineEloWhite  READ engineEloWhite  NOTIFY engineEloChanged)
    Q_PROPERTY(int     engineEloBlack  READ engineEloBlack  NOTIFY engineEloChanged)

    Q_PROPERTY(bool hasClock            READ hasClock            NOTIFY clockChanged)
    Q_PROPERTY(int  whiteRemainingMs    READ whiteRemainingMs    NOTIFY clockChanged)
    Q_PROPERTY(int  blackRemainingMs    READ blackRemainingMs    NOTIFY clockChanged)

    Q_PROPERTY(bool hasSavedGame        READ hasSavedGame        NOTIFY hasSavedGameChanged)

    Q_PROPERTY(int  analysisEvalCp      READ analysisEvalCp      NOTIFY analysisEvalChanged)
    Q_PROPERTY(bool analysisEvalMate    READ analysisEvalMate    NOTIFY analysisEvalChanged)
    Q_PROPERTY(int  analysisEvalMateIn  READ analysisEvalMateIn  NOTIFY analysisEvalChanged)
    Q_PROPERTY(QVariantList analysisTopLines    READ analysisTopLines    NOTIFY analysisTopLinesChanged)
    Q_PROPERTY(int          analysisDepth        READ analysisDepth       NOTIFY analysisDepthChanged)
    Q_PROPERTY(QString      analysisEngineName   READ analysisEngineName  NOTIFY analysisEngineNameChanged)

    Q_PROPERTY(bool inCheck             READ inCheck             NOTIFY positionChanged)
    Q_PROPERTY(int  checkedKingSquare   READ checkedKingSquare   NOTIFY positionChanged)
    Q_PROPERTY(int  lastMoveFrom        READ lastMoveFrom        NOTIFY positionChanged)
    Q_PROPERTY(int  lastMoveTo          READ lastMoveTo          NOTIFY positionChanged)
    Q_PROPERTY(bool resigned            READ resigned            NOTIFY positionChanged)

    Q_PROPERTY(bool atLatestMove   READ atLatestMove   NOTIFY positionChanged)
    Q_PROPERTY(int  viewMoveIndex  READ viewMoveIndex  NOTIFY positionChanged)
    Q_PROPERTY(bool undoAllowed    READ undoAllowed    WRITE setUndoAllowed)

public:
    explicit GameBridge(QObject* parent = nullptr);

    // Board state
    QVariantList pieces() const;
    int          currentTurn() const;   // 0=White, 1=Black
    QString      gameStatus() const;
    QStringList  moveHistorySan() const;
    QVariantList capturedByWhite() const;
    QVariantList capturedByBlack() const;

    // Clock
    bool hasClock()         const { return m_game.has_clocks(); }
    int  whiteRemainingMs() const;
    int  blackRemainingMs() const;

    // Check state
    bool inCheck()          const;
    int  checkedKingSquare() const;

    // Last move
    int  lastMoveFrom() const {
        if (m_viewIndex >= 0) {
            const auto& h = m_viewGame.get_move_history();
            return h.empty() ? -1 : h.back().square_from;
        }
        return m_lastMoveFrom;
    }
    int  lastMoveTo() const {
        if (m_viewIndex >= 0) {
            const auto& h = m_viewGame.get_move_history();
            return h.empty() ? -1 : h.back().square_to;
        }
        return m_lastMoveTo;
    }

    // Theme
    QString boardTheme() const { return m_boardTheme; }
    void    setBoardTheme(const QString& theme);

    // Engine path
    QString enginePath()           const { return m_enginePath; }
    QString enginePathBlack()      const { return m_enginePathBlack; }
    QString enginePathWhiteGame()  const { return m_enginePathWhiteGame; }
    QString engineName()           const { return m_engineName; }
    QString engineNameBlack()      const { return m_engineNameBlack; }
    QString enginePathError()      const { return m_enginePathError; }
    int     engineThreads()        const { return m_engineThreads; }
    int     engineEloWhite()       const { return m_whiteComputerElo; }
    int     engineEloBlack()       const { return m_blackComputerElo; }
    Q_INVOKABLE void setEnginePath(const QString& path);
    Q_INVOKABLE void setEnginePathBlack(const QString& path);
    Q_INVOKABLE void setEnginePathWhiteGame(const QString& path);
    Q_INVOKABLE void setEngineThreads(int n);
    Q_INVOKABLE void setEngineElos(int whiteElo, int blackElo);

    QString pieceTheme() const { return m_pieceTheme; }
    void    setPieceTheme(const QString& theme);

    int  pieceSize() const { return m_pieceSize; }
    void setPieceSize(int size);

    // Invokable from QML
    Q_INVOKABLE QVariantList legalMovesFrom(int square) const;
    Q_INVOKABLE bool         makeMove(int from, int to, int promotionPiece = 2 /*QUEEN*/);
    Q_INVOKABLE void         newGame(const QString& fen = QString());
    Q_INVOKABLE bool         loadPgn(const QString& pgnText);
    Q_INVOKABLE QString      gamePgn(const QString& whiteName = QStringLiteral("?"),
                                     const QString& blackName = QStringLiteral("?")) const;
    Q_INVOKABLE void         copyToClipboard(const QString& text) const;
    // Configure clocks: pass 0 for no time control
    Q_INVOKABLE void         configureClock(int whiteMs, int blackMs, int wIncMs = 0, int bIncMs = 0);
    Q_INVOKABLE void         resign(); // current side resigns
    // Set which side the engine plays: 0=white, 1=black, -1=none (human vs human)
    Q_INVOKABLE void         setComputerSide(int color);
    // Move navigation (history replay)
    Q_INVOKABLE void         stepBack();
    Q_INVOKABLE void         stepForward();
    Q_INVOKABLE void         takeBack();

    // Persistent game save / restore
    // saveGame() – serialises the current in-progress game to QSettings.
    // Call this after every move (from QML) to keep the save fresh.
    Q_INVOKABLE void         saveGame(const QString& whiteName,
                                      const QString& blackName,
                                      int computerSide,
                                      int whiteElo,
                                      int blackElo);
    // hasSavedGame() – true when a non-empty saved game exists.
    bool                     hasSavedGame() const;
    // loadSavedGame() – restores the saved game into m_game and returns a
    // QVariantMap with keys: whiteName, blackName, computerSide,
    // whiteElo, blackElo, whiteRemainingMs, blackRemainingMs, wIncMs, bIncMs.
    // Returns an empty map if no saved game exists.
    Q_INVOKABLE QVariantMap  loadSavedGame();
    // clearSavedGame() – removes the saved game from QSettings.
    Q_INVOKABLE void         clearSavedGame();
    // pauseForMenu() – stops the clock and engine without resetting the board
    // so the position stays visible in the menu background.
    Q_INVOKABLE void         pauseForMenu();
    // Return persisted theme names for startup initialisation.
    Q_INVOKABLE QString      savedBoardTheme() const;
    Q_INVOKABLE QString      savedPieceTheme() const;

    // ── Analysis-board helpers ─────────────────────────────────────────────
    // Call once from QML (Component.onCompleted) on the analysis ChessBoard's
    // bridge to:
    //   • prevent it from sharing the playing game's save slot
    //   • load the last analysis position from "analysisgame" (if any)
    Q_INVOKABLE void initAsAnalysisBridge() {
        m_isAnalysis = true;
        // Reset to starting position – discard whatever the constructor loaded
        // from the playing game's "savedgame" slot.
        m_game = Game();
        m_viewIndex    = -1;
        m_lastMoveFrom = -1;
        m_lastMoveTo   = -1;
        buildCapturedLists();
        // Load last analysis position if one was saved.
        auto s = makeSettings();
        const QString pgn = s.value(QStringLiteral("analysisgame/pgn")).toString();
        if (!pgn.isEmpty()) {
            Game loaded;
            if (parse_pgn(pgn.toStdString(), loaded)) {
                m_game = loaded;
                if (!m_game.get_move_history().empty()) {
                    m_lastMoveFrom = m_game.get_move_history().back().square_from;
                    m_lastMoveTo   = m_game.get_move_history().back().square_to;
                }
                buildCapturedLists();
            }
        }
        emit positionChanged();
    }

    // Call when the analysis view is hidden (user navigates away) to persist
    // the current analysis position for the next session.
    Q_INVOKABLE void saveAnalysisPosition() {
        if (!m_isAnalysis) return;
        auto s = makeSettings();
        s.setValue(QStringLiteral("analysisgame/pgn"),
                   gamePgn(QStringLiteral("?"), QStringLiteral("?")));
    }

    bool undoAllowed()  const { return m_undoAllowed; }
    void setUndoAllowed(bool v) { m_undoAllowed = v; }

    bool resigned() const { return m_resigned; }

    // Analysis eval (background engine)
    int  analysisEvalCp()      const { return m_analysisEvalCp; }
    bool analysisEvalMate()    const { return m_analysisEvalMate; }
    int  analysisEvalMateIn()  const { return m_analysisEvalMateIn; }
    QVariantList analysisTopLines()  const { return m_analysisTopLines; }
    int          analysisDepth()     const { return m_analysisDepth; }
    QString      analysisEngineName()const { return m_analysisEngineName; }
    Q_INVOKABLE void startAnalysis();
    Q_INVOKABLE void stopAnalysis();
    Q_INVOKABLE void setAnalysisMaxDepth(int d) {
        if (m_maxEngineDepth == d) return;
        m_maxEngineDepth = d;
        if (m_analysisWorker)
            m_analysisWorker->m_maxEngineDepth.store(d, std::memory_order_release);
        // Restart the running search so it picks up the new depth limit immediately.
        restartAnalysisIfRunning();
    }

    bool atLatestMove()  const { return m_viewIndex < 0; }
    int  viewMoveIndex() const { return m_viewIndex < 0 ? (int)m_game.get_move_history().size() : m_viewIndex; }

signals:
    void positionChanged();
    void boardThemeChanged();
    void pieceThemeChanged();
    void pieceSizeChanged();
    void clockChanged();
    void enginePathChanged();
    void enginePathBlackChanged();
    void enginePathWhiteGameChanged();
    void engineNameChanged();
    void engineNameBlackChanged();
    void enginePathErrorChanged();
    void engineThreadsChanged();
    void engineEloChanged();
    void hasSavedGameChanged();

    // emitted whenever the background analysis eval updates
    void analysisEvalChanged();
    void analysisTopLinesChanged();
    void analysisDepthChanged();
    void analysisEngineNameChanged();

    // lastMoveWasCapture, isGameOver
    void movePlayed(bool wasCapture, bool isGameOver);
    // emitted when a side resigns: 0=white, 1=black
    void playerResigned(int loserColor);

private:
    void buildCapturedLists();
    void maybeStartEngineMove();
    void applyEngineMove(const std::string& uciMove);
    void buildViewGame();
    void restartAnalysisIfRunning();

    Game    m_game;
    int     m_viewIndex = -1;   // -1 = live head; >= 0 = browsing history
    Game    m_viewGame;         // valid only when m_viewIndex >= 0
    QString m_boardTheme  = "green";
    QString m_pieceTheme  = "cburnett";
    int     m_pieceSize   = 64;

    QString m_enginePath;             // persisted settings path for white
    QString m_enginePathWhiteGame;    // per-game override for white (not persisted)
    QString m_enginePathBlack;        // per-game path for black (not persisted)
    QString m_engineName;             // populated after white engine starts
    QString m_engineNameBlack;        // populated after black engine starts (CvC only)
    QString m_enginePathError;        // non-empty when path is invalid
    int     m_engineThreads    = 0; // 0 = auto (hardware_concurrency)
    int     m_whiteComputerElo = 0; // 0 = no strength limit
    int     m_blackComputerElo = 0;

    QVariantList m_capturedByWhite;
    QVariantList m_capturedByBlack;

    int     m_lastMoveFrom = -1;
    int     m_lastMoveTo   = -1;
    bool    m_undoAllowed  = true;
    bool    m_resigned     = false;
    bool    m_isAnalysis   = false;   // true for the analysis board; skips saved-game slot

    QTimer* m_clockTimer  = nullptr;
    int     m_wIncMs      = 0;
    int     m_bIncMs      = 0;

    // Engine / computer side
    QThread*      m_engineThread      = nullptr;
    EngineWorker* m_engineWorker      = nullptr;
    bool          m_engineReady       = false;
    QThread*      m_engineThreadBlack = nullptr;  // non-null only in CvC with separate black engine
    EngineWorker* m_engineWorkerBlack = nullptr;
    bool          m_engineReadyBlack  = false;
    int           m_computerSide      = -1;  // -1=none, 0=white, 1=black, 2=both
    bool          m_engineThinking    = false;

    // Background analysis engine
    QThread*        m_analysisThread      = nullptr;
    AnalysisWorker* m_analysisWorker      = nullptr;
    bool            m_analysisRunning     = false;   // user intent: should analysis run?
    bool            m_analysisLoopActive  = false;   // is a doAnalyze currently queued/running?
    bool            m_analysisEngineReady = false;
    // Current position queued for analysis (main thread only)
    QString         m_analysisFen;
    QStringList     m_analysisMoves;
    int             m_analysisTurn        = 0;
    // "dirty" flag: position changed while a search was in flight
    bool            m_analysisPending     = false;
    int             m_analysisEvalCp      = 0;
    bool            m_analysisEvalMate    = false;
    int             m_analysisEvalMateIn  = 0;
    QVariantList    m_analysisTopLines;
    int             m_analysisDepth       = 0;
    QString         m_analysisEngineName;
    int             m_maxEngineDepth      = 0;
};

#endif // GAMEBRIDGE_H
