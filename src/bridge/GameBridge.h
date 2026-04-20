#ifndef GAMEBRIDGE_H
#define GAMEBRIDGE_H

#include <QObject>
#include <QDebug>
#include <QSettings>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqml.h>
#include <thread>
#include "../backend/Game.h"
#include "../backend/uci_engine.h"

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

    Q_PROPERTY(bool inCheck             READ inCheck             NOTIFY positionChanged)
    Q_PROPERTY(int  checkedKingSquare   READ checkedKingSquare   NOTIFY positionChanged)
    Q_PROPERTY(int  lastMoveFrom        READ lastMoveFrom        NOTIFY positionChanged)
    Q_PROPERTY(int  lastMoveTo          READ lastMoveTo          NOTIFY positionChanged)

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

    bool undoAllowed()  const { return m_undoAllowed; }
    void setUndoAllowed(bool v) { m_undoAllowed = v; }

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

    // lastMoveWasCapture, isGameOver
    void movePlayed(bool wasCapture, bool isGameOver);
    // emitted when a side resigns: 0=white, 1=black
    void playerResigned(int loserColor);

private:
    void buildCapturedLists();
    void maybeStartEngineMove();
    void applyEngineMove(const std::string& uciMove);
    void buildViewGame();

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
};

#endif // GAMEBRIDGE_H
