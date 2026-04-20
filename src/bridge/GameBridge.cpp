#include "GameBridge.h"
#include "../backend/movegen.h"
#include "../backend/notation.h"
#include "../backend/types.h"
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QClipboard>
#include <QSettings>
#include <QTimer>
#include <QVariantList>

// Initial piece counts on a fresh board (index = Piece enum value)
static const int kInitialCount[13] = {
    0, // NO_PIECE
    1, // W_KING
    1, // W_QUEEN
    2, // W_ROOK
    2, // W_BISHOP
    2, // W_KNIGHT
    8, // W_PAWN
    1, // B_KING
    1, // B_QUEEN
    2, // B_ROOK
    2, // B_BISHOP
    2, // B_KNIGHT
    8  // B_PAWN
};

GameBridge::GameBridge(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<UCISearchResult>();

    // Load persisted engine path
    QSettings settings;
    m_enginePath        = settings.value("engine/path",       QString()).toString();
    // m_enginePathBlack is intentionally not persisted – it is temporary (per-game only)
    m_engineThreads     = settings.value("engine/threads", 0).toInt();
    m_whiteComputerElo  = settings.value("engine/elo/white", 0).toInt();
    m_blackComputerElo  = settings.value("engine/elo/black", 0).toInt();

    m_clockTimer = new QTimer(this);
    m_clockTimer->setInterval(100);
    connect(m_clockTimer, &QTimer::timeout, this, [this]() {
        // Detect flag before emitting so QML sees the updated status
        bool justFlagged = m_game.flag_if_timed_out();
        emit clockChanged();
        if (justFlagged)
            emit positionChanged();
        // Stop ticking if game is over
        if (m_game.get_game_status() != ONGOING)
            m_clockTimer->stop();
    });

    newGame();
}

void GameBridge::newGame(const QString& fen)
{
    m_clockTimer->stop();
    m_engineThinking = false;
    m_lastMoveFrom = -1;
    m_lastMoveTo   = -1;
    m_viewIndex    = -1;
    m_game = Game();
    if (!fen.isEmpty())
        m_game.set_start_fen(fen.toStdString());
    buildCapturedLists();
    emit positionChanged();
    emit clockChanged();
    // If white had a per-game engine override, tear it down so setEnginePathWhiteGame()
    // always starts fresh next game regardless of whether the path changed.
    if (!m_enginePathWhiteGame.isEmpty()) {
        m_enginePathWhiteGame.clear();
        emit enginePathWhiteGameChanged();
        if (m_engineReady) {
            m_engineReady = false;
            m_engineName.clear();
            if (m_engineThread) {
                m_engineThread->quit();
                m_engineThread->wait();
                m_engineThread = nullptr;
                m_engineWorker = nullptr;
            }
            emit engineNameChanged();
        }
    } else if (m_engineReady) {
        QMetaObject::invokeMethod(m_engineWorker, &EngineWorker::newGame, Qt::QueuedConnection);
    }
    // Black engine is per-game: tear it down so the upcoming setEnginePathBlack()
    // call always starts fresh regardless of whether the path changed.
    if (m_engineReadyBlack) {
        m_engineReadyBlack = false;
        if (m_engineThreadBlack) {
            m_engineThreadBlack->quit();
            m_engineThreadBlack->wait();
            m_engineThreadBlack = nullptr;
            m_engineWorkerBlack = nullptr;
        }
    }
    m_enginePathBlack.clear();
    if (!m_engineNameBlack.isEmpty()) {
        m_engineNameBlack.clear();
        emit engineNameBlackChanged();
    }
    // Let engine move immediately if it plays white (or if FEN says black just moved)
    QTimer::singleShot(0, this, &GameBridge::maybeStartEngineMove);
}

void GameBridge::configureClock(int whiteMs, int blackMs, int wIncMs, int bIncMs)
{
    m_wIncMs = wIncMs;
    m_bIncMs = bIncMs;
    m_game.set_clocks(whiteMs, blackMs, wIncMs, bIncMs);
    if (whiteMs > 0) {
        m_game.start_clock();
        m_clockTimer->start();
    }
    emit clockChanged();
}

QVariantList GameBridge::pieces() const
{
    QVariantList list;
    list.reserve(64);
    const Board& board = (m_viewIndex >= 0) ? m_viewGame.get_board() : m_game.get_board();
    for (int sq = 0; sq < 64; ++sq)
        list.append(static_cast<int>(board.get_piece(sq)));
    return list;
}

int GameBridge::currentTurn() const
{
    return static_cast<int>((m_viewIndex >= 0) ? m_viewGame.get_turn() : m_game.get_turn());
}

QString GameBridge::gameStatus() const
{
    switch ((m_viewIndex >= 0 ? m_viewGame : m_game).get_game_status()) {
    case CHECKMATE:               return QStringLiteral("checkmate");
    case STALEMATE:               return QStringLiteral("stalemate");
    case DRAW_BY_REPETITION:      return QStringLiteral("draw_repetition");
    case DRAW_BY_FIFTY_MOVE_RULE:          return QStringLiteral("draw_fifty");
    case DRAW_BY_INSUFFICIENT_MATERIAL:    return QStringLiteral("draw_material");
    case TIMEOUT:                          return QStringLiteral("timeout");
    default:                      return QStringLiteral("ongoing");
    }
}

QStringList GameBridge::moveHistorySan() const
{
    const auto& hist = m_game.get_move_history_san();
    QStringList out;
    out.reserve((int)hist.size());
    for (const auto& s : hist)
        out.append(QString::fromStdString(s));
    return out;
}

void GameBridge::buildCapturedLists()
{
    // Count pieces currently on the board (always reflects live game)
    int count[13] = {};
    const Board& board = m_game.get_board();
    for (int sq = 0; sq < 64; ++sq)
        count[static_cast<int>(board.get_piece(sq))]++;

    m_capturedByWhite.clear();
    m_capturedByBlack.clear();

    // Black pieces captured by white (B_KING not counted)
    for (int p = B_QUEEN; p <= B_PAWN; ++p) {
        int missing = kInitialCount[p] - count[p];
        for (int i = 0; i < missing; ++i)
            m_capturedByWhite.append(p);
    }
    // White pieces captured by black (W_KING not counted)
    for (int p = W_QUEEN; p <= W_PAWN; ++p) {
        int missing = kInitialCount[p] - count[p];
        for (int i = 0; i < missing; ++i)
            m_capturedByBlack.append(p);
    }
}

QVariantList GameBridge::capturedByWhite() const { return m_capturedByWhite; }
QVariantList GameBridge::capturedByBlack() const { return m_capturedByBlack; }

int GameBridge::whiteRemainingMs() const
{
    return m_game.has_clocks()
        ? static_cast<int>(m_game.remaining_ms(WHITE))
        : -1;
}

int GameBridge::blackRemainingMs() const
{
    return m_game.has_clocks()
        ? static_cast<int>(m_game.remaining_ms(BLACK))
        : -1;
}

QVariantList GameBridge::legalMovesFrom(int square) const
{
    QVariantList targets;
    Board boardCopy = m_game.get_board();
    const std::vector<Move> moves = generate_legal_moves(boardCopy, square);
    for (const Move& m : moves)
        targets.append(m.square_to);
    return targets;
}

bool GameBridge::makeMove(int from, int to, int promotionPiece)
{
    // Snap back to live position if browsing history
    if (m_viewIndex >= 0) {
        m_viewIndex = -1;
    }

    Board boardCopy = m_game.get_board();
    const std::vector<Move> moves = generate_legal_moves(boardCopy, from);

    for (const Move& m : moves) {
        if (m.square_to != to) continue;
        if (m.move_type == PROMOTION) {
            PieceType pt = static_cast<PieceType>(promotionPiece);
            if (m.promotion_piece_type != pt) continue;
        }

        bool wasCapture = (m.piece_captured != NO_PIECE) || (m.move_type == EN_PASSANT);
        bool ok         = m_game.make_move(m);
        if (!ok) return false;

        m_lastMoveFrom = from;
        m_lastMoveTo   = to;
        m_game.check_game_status();
        buildCapturedLists();
        bool gameOver = (m_game.get_game_status() != ONGOING);

        if (gameOver)
            m_clockTimer->stop();
        else if (m_game.has_clocks() && !m_clockTimer->isActive())
            m_clockTimer->start();

        emit positionChanged();
        emit clockChanged();
        emit movePlayed(wasCapture, gameOver);
        maybeStartEngineMove();
        return true;
    }
    return false;
}

void GameBridge::resign()
{
    if (m_game.get_game_status() != ONGOING) return;
    m_clockTimer->stop();
    int loser = currentTurn();
    emit playerResigned(loser);
    emit positionChanged();
}

QString GameBridge::gamePgn(const QString& whiteName, const QString& blackName) const
{
    std::string pgn = game_to_pgn(
        m_game,
        whiteName.toStdString(),
        blackName.toStdString(),
        "ntchess",
        "?"
    );
    return QString::fromStdString(pgn);
}

void GameBridge::copyToClipboard(const QString& text) const
{
    QGuiApplication::clipboard()->setText(text);
}

void GameBridge::setEngineThreads(int n)
{
    if (n == m_engineThreads) return;
    m_engineThreads = n;

    // Persist
    QSettings settings;
    if (n == 0)
        settings.remove("engine/threads");
    else
        settings.setValue("engine/threads", n);

    // Apply to running worker if available
    if (m_engineWorker) {
        qDebug() << "[GameBridge] setEngineThreads: forwarding" << n << "to worker";
        QMetaObject::invokeMethod(m_engineWorker, [this, n]() {
            m_engineWorker->setThreads(n);
        }, Qt::QueuedConnection);
    }

    emit engineThreadsChanged();
}

void GameBridge::setEngineElos(int whiteElo, int blackElo)
{
    m_whiteComputerElo = whiteElo;
    m_blackComputerElo = blackElo;

    QSettings settings;
    if (whiteElo == 0) settings.remove("engine/elo/white"); else settings.setValue("engine/elo/white", whiteElo);
    if (blackElo == 0) settings.remove("engine/elo/black"); else settings.setValue("engine/elo/black", blackElo);

    emit engineEloChanged();
}

void GameBridge::setEnginePath(const QString& path)
{
    if (path == m_enginePath) return;

    // Basic validation: if non-empty, the file must exist and be executable
    QString error;
    if (!path.isEmpty()) {
        QFileInfo fi(path);
        if (!fi.exists())
            error = "File not found: " + path;
        else if (!fi.isExecutable())
            error = "File is not executable: " + path;
    }

    m_enginePath = path;
    m_enginePathError = error;

    // Reset engine so it will be re-initialised on the next game
    if (m_engineReady) {
        m_engineReady = false;
        m_engineName  = QString();
        if (m_engineThread) {
            m_engineThread->quit();
            m_engineThread->wait();
            m_engineThread = nullptr;
            m_engineWorker = nullptr;
        }
        emit engineNameChanged();
    }

    // Persist to settings
    QSettings settings;
    if (path.isEmpty())
        settings.remove("engine/path");
    else
        settings.setValue("engine/path", path);

    emit enginePathChanged();
    emit enginePathErrorChanged();
}

void GameBridge::setEnginePathBlack(const QString& path)
{
    qDebug() << "setEnginePathBlack:" << path << "(was:" << m_enginePathBlack << ")";
    if (path == m_enginePathBlack) {
        qDebug() << "setEnginePathBlack: same path, skipping";
        return;
    }

    m_enginePathBlack = path;

    // Tear down the dedicated black engine so it restarts with the new path next game
    if (m_engineReadyBlack) {
        m_engineReadyBlack = false;
        if (m_engineThreadBlack) {
            m_engineThreadBlack->quit();
            m_engineThreadBlack->wait();
            m_engineThreadBlack = nullptr;
            m_engineWorkerBlack = nullptr;
        }
    }

    emit enginePathBlackChanged();
}

void GameBridge::setEnginePathWhiteGame(const QString& path)
{
    qDebug() << "setEnginePathWhiteGame:" << path << "(was:" << m_enginePathWhiteGame << ")";
    if (path == m_enginePathWhiteGame) return;

    m_enginePathWhiteGame = path;

    // Tear down white engine so it restarts with the new path when setComputerSide is called
    if (m_engineReady) {
        m_engineReady = false;
        m_engineName.clear();
        if (m_engineThread) {
            m_engineThread->quit();
            m_engineThread->wait();
            m_engineThread = nullptr;
            m_engineWorker = nullptr;
        }
        emit engineNameChanged();
    }

    emit enginePathWhiteGameChanged();
}

void GameBridge::setBoardTheme(const QString& theme)
{
    if (m_boardTheme == theme) return;
    m_boardTheme = theme;
    emit boardThemeChanged();
}

void GameBridge::setPieceTheme(const QString& theme)
{
    if (m_pieceTheme == theme) return;
    m_pieceTheme = theme;
    emit pieceThemeChanged();
}

void GameBridge::setPieceSize(int size)
{
    if (m_pieceSize == size) return;
    m_pieceSize = size;
    emit pieceSizeChanged();
}

bool GameBridge::inCheck() const
{
    const Game& g = (m_viewIndex >= 0) ? m_viewGame : m_game;
    Color side = g.get_turn();
    return is_in_check(g.get_board(), side);
}

int GameBridge::checkedKingSquare() const
{
    if (!inCheck()) return -1;
    const Game& g = (m_viewIndex >= 0) ? m_viewGame : m_game;
    Piece kingPiece = (g.get_turn() == WHITE) ? W_KING : B_KING;
    const Board& b = g.get_board();
    for (int sq = 0; sq < 64; ++sq)
        if (b.get_piece(sq) == kingPiece) return sq;
    return -1;
}

void GameBridge::setComputerSide(int color)
{
    qDebug() << "setComputerSide:" << color << "engineReady:" << m_engineReady;
    m_computerSide   = color;
    m_engineThinking = false;

    if (color < 0) return; // human vs human – no engine needed

    if (!m_engineReady) {
        // Determine which path to try first
        const QString defaultPath = QString::fromUtf8(ASSETS_PATH "/stockfish/stockfish-ubuntu-x86-64-avx2");
        QStringList candidates;
        if (!m_enginePathWhiteGame.isEmpty())
            candidates << m_enginePathWhiteGame;
        else if (!m_enginePath.isEmpty())
            candidates << m_enginePath;
        candidates << defaultPath;

        std::string chosenPath;
        bool started = false;

        // Validate user path first; if it fails fall back to bundled engine
        const QString userWhitePath = m_enginePathWhiteGame.isEmpty() ? m_enginePath : m_enginePathWhiteGame;
        for (const QString& candidate : candidates) {
            QFileInfo fi(candidate);
            if (!fi.exists() || !fi.isExecutable()) {
                if (candidate == userWhitePath) {
                    m_enginePathError = "Engine not found – using built-in Stockfish.";
                    emit enginePathErrorChanged();
                }
                continue;
            }
            chosenPath = candidate.toStdString();

            m_engineThread = new QThread(this);
            m_engineWorker = new EngineWorker();
            m_engineWorker->m_threads = m_engineThreads;
            m_engineWorker->m_elo     = m_whiteComputerElo; // white moves first
            m_engineWorker->moveToThread(m_engineThread);
            connect(m_engineThread, &QThread::finished, m_engineWorker, &QObject::deleteLater);
            connect(m_engineWorker, &EngineWorker::thinkFinished, this, [this](UCISearchResult result) {
                m_engineThinking = false;
                qDebug() << "Engine result: valid=" << result.valid() << "move=" << QString::fromStdString(result.bestmove);
                if (result.valid() && m_game.get_game_status() == ONGOING) {
                    applyEngineMove(result.bestmove);
                } else if (m_game.get_game_status() == ONGOING) {
                    qDebug() << "Engine returned no move, retrying...";
                    QTimer::singleShot(100, this, &GameBridge::maybeStartEngineMove);
                }
            });
            m_engineThread->start();

            bool ok = false;
            QMetaObject::invokeMethod(m_engineWorker, [this, chosenPath, &ok]() {
                ok = m_engineWorker->startEngine(chosenPath);
            }, Qt::BlockingQueuedConnection);

            qDebug() << "Engine start (" << QString::fromStdString(chosenPath) << "):" << ok;
            if (ok) {
                m_engineReady = true;
                // Retrieve engine name from UCI handshake
                QString name;
                QMetaObject::invokeMethod(m_engineWorker, [this, &name]() {
                    name = m_engineWorker->engineName();
                }, Qt::BlockingQueuedConnection);
                if (name.isEmpty()) name = QStringLiteral("Computer");
                if (name != m_engineName) {
                    m_engineName = name;
                    emit engineNameChanged();
                }
                // Clear error if we successfully started (possibly after falling back)
                if (QString::fromStdString(chosenPath) != userWhitePath && !userWhitePath.isEmpty()) {
                    // We fell back to default
                } else {
                    m_enginePathError.clear();
                    emit enginePathErrorChanged();
                }
                started = true;
                break;
            } else {
                // Clean up failed thread
                m_engineThread->quit();
                m_engineThread->wait();
                m_engineThread = nullptr;
                m_engineWorker = nullptr;
                if (candidate == userWhitePath) {
                    m_enginePathError = "Failed to start engine – using built-in Stockfish.";
                    emit enginePathErrorChanged();
                }
            }
        }

        if (!started) {
            m_enginePathError = "Could not start any chess engine.";
            emit enginePathErrorChanged();
            qWarning() << m_enginePathError;
        }
    }

    if (m_engineReady) {
        QMetaObject::invokeMethod(m_engineWorker, &EngineWorker::newGame, Qt::QueuedConnection);
        QTimer::singleShot(0, this, &GameBridge::maybeStartEngineMove);
    }

    // CvC with a dedicated second engine for black.
    // Effective path resolution: per-game override → settings path → bundled default.
    const QString defaultPath = QString::fromUtf8(ASSETS_PATH "/stockfish/stockfish-ubuntu-x86-64-avx2");
    const QString effectiveBlackPath = !m_enginePathBlack.isEmpty() ? m_enginePathBlack
                                     : !m_enginePath.isEmpty()      ? m_enginePath
                                     : defaultPath;
    qDebug() << "setComputerSide: black engine block – color=" << color
             << "effectiveBlackPath=" << effectiveBlackPath
             << "readyBlack=" << m_engineReadyBlack;
    if (color == 2 && !m_engineReadyBlack) {
        QFileInfo fi(effectiveBlackPath);
        qDebug() << "Black engine file exists=" << fi.exists() << "executable=" << fi.isExecutable();
        if (fi.exists() && fi.isExecutable()) {
            std::string blackPath = effectiveBlackPath.toStdString();
            m_engineThreadBlack = new QThread(this);
            m_engineWorkerBlack = new EngineWorker();
            m_engineWorkerBlack->m_threads = m_engineThreads;
            m_engineWorkerBlack->m_elo     = m_blackComputerElo;
            m_engineWorkerBlack->moveToThread(m_engineThreadBlack);
            connect(m_engineThreadBlack, &QThread::finished, m_engineWorkerBlack, &QObject::deleteLater);
            connect(m_engineWorkerBlack, &EngineWorker::thinkFinished, this, [this](UCISearchResult result) {
                m_engineThinking = false;
                qDebug() << "Black engine result: valid=" << result.valid() << "move=" << QString::fromStdString(result.bestmove);
                if (result.valid() && m_game.get_game_status() == ONGOING)
                    applyEngineMove(result.bestmove);
                else if (m_game.get_game_status() == ONGOING)
                    QTimer::singleShot(100, this, &GameBridge::maybeStartEngineMove);
            });
            m_engineThreadBlack->start();

            bool ok = false;
            QMetaObject::invokeMethod(m_engineWorkerBlack, [this, blackPath, &ok]() {
                ok = m_engineWorkerBlack->startEngine(blackPath);
            }, Qt::BlockingQueuedConnection);

            if (ok) {
                m_engineReadyBlack = true;
                QString bname;
                QMetaObject::invokeMethod(m_engineWorkerBlack, [this, &bname]() {
                    bname = m_engineWorkerBlack->engineName();
                }, Qt::BlockingQueuedConnection);
                if (bname.isEmpty()) bname = QStringLiteral("Computer");
                if (bname != m_engineNameBlack) {
                    m_engineNameBlack = bname;
                    emit engineNameBlackChanged();
                }
                QMetaObject::invokeMethod(m_engineWorkerBlack, &EngineWorker::newGame, Qt::QueuedConnection);
                qDebug() << "Black engine started:" << m_enginePathBlack << "name:" << m_engineNameBlack;
            } else {
                m_engineThreadBlack->quit();
                m_engineThreadBlack->wait();
                m_engineThreadBlack = nullptr;
                m_engineWorkerBlack = nullptr;
                qWarning() << "Failed to start black engine – using white engine for both sides.";
            }
        } else {
            qWarning() << "Black engine path not found or not executable:" << m_enginePathBlack;
        }
    }
}

void GameBridge::maybeStartEngineMove()
{
    qDebug() << "maybeStart: side=" << m_computerSide << "ready=" << m_engineReady
             << "thinking=" << m_engineThinking << "status=" << (int)m_game.get_game_status()
             << "turn=" << (int)m_game.get_turn();
    if (m_computerSide < 0)                  return;
    if (!m_engineReady)                      return;
    if (m_engineThinking)                    return;
    if (m_game.get_game_status() != ONGOING) return;

    // m_computerSide: 0=engine plays white, 1=engine plays black, 2=both sides
    if (m_computerSide != 2) {
        Color engineColor = (m_computerSide == 0) ? WHITE : BLACK;
        if (m_game.get_turn() != engineColor) return;
    }

    m_engineThinking = true;

    // Restart the clock for the side about to move so that any time spent on
    // engine initialisation or Qt event-loop overhead (which can be hundreds of
    // milliseconds on the very first move) is not charged to this player.
    if (m_game.has_clocks())
        m_game.start_clock();

    // Copy the game for the worker thread
    Game gameCopy = m_game;

    UCIGoOptions opts;
    if (m_game.has_clocks()) {
        opts.wtime = whiteRemainingMs();
        opts.btime = blackRemainingMs();
        opts.winc  = m_wIncMs;
        opts.binc  = m_bIncMs;
    } else {
        // No clock: give the engine a virtual 5-minute pool so it uses its
        // own time management (replies instantly on easy moves, thinks longer
        // on complex ones) rather than always burning a fixed duration.
        opts.wtime = 300000;
        opts.btime = 300000;
        opts.winc  = 0;
        opts.binc  = 0;
    }

    bool useBlackEngine = (m_computerSide == 2 && m_engineReadyBlack && m_game.get_turn() == BLACK);
    qDebug() << "maybeStart: useBlackEngine=" << useBlackEngine << "readyBlack=" << m_engineReadyBlack;
    EngineWorker* activeWorker = useBlackEngine ? m_engineWorkerBlack : m_engineWorker;
    activeWorker->setInputs(gameCopy, opts,
        (m_game.get_turn() == WHITE) ? m_whiteComputerElo : m_blackComputerElo);
    QMetaObject::invokeMethod(activeWorker, &EngineWorker::doThink, Qt::QueuedConnection);
}

void GameBridge::applyEngineMove(const std::string& uciMove)
{
    Board boardCopy = m_game.get_board();
    Color turn      = m_game.get_turn();
    Move  m         = uci_to_move(uciMove, boardCopy, turn);
    if (m.square_from == m.square_to) return; // invalid

    int promotionPiece = 2; // QUEEN default
    if (m.move_type == PROMOTION)
        promotionPiece = static_cast<int>(m.promotion_piece_type);

    makeMove(m.square_from, m.square_to, promotionPiece);
}

void GameBridge::buildViewGame()
{
    m_viewGame = Game();
    const std::string& sf = m_game.start_fen();
    if (!sf.empty()) m_viewGame.set_start_fen(sf);
    const auto& hist = m_game.get_move_history();
    int limit = (m_viewIndex >= 0) ? m_viewIndex : (int)hist.size();
    for (int i = 0; i < limit && i < (int)hist.size(); ++i)
        m_viewGame.make_move(hist[i]);
    m_viewGame.check_game_status();
}

void GameBridge::stepBack()
{
    int histLen = (int)m_game.get_move_history().size();
    if (histLen == 0) return;
    if (m_viewIndex < 0)
        m_viewIndex = histLen - 1;
    else if (m_viewIndex > 0)
        m_viewIndex--;
    else
        return; // already at start
    buildViewGame();
    emit positionChanged();
}

void GameBridge::stepForward()
{
    if (m_viewIndex < 0) return; // already at head
    int histLen = (int)m_game.get_move_history().size();
    m_viewIndex++;
    if (m_viewIndex >= histLen)
        m_viewIndex = -1; // back to live
    else
        buildViewGame();
    emit positionChanged();
}

void GameBridge::takeBack()
{
    if (!m_undoAllowed) return;
    const auto& hist = m_game.get_move_history();
    int histLen = (int)hist.size();
    if (histLen == 0) return;

    // In HvC: undo 2 plies so the human gets their turn back.
    // (After computer plays it's the human's turn; undo engine reply + human move.)
    // In HvH or CvC: undo 1 ply.
    int plies = 1;
    if (m_computerSide >= 0 && m_computerSide <= 1 && histLen >= 2)
        plies = 2;

    int replayTo = histLen - plies;

    // Stop clock and engine
    m_clockTimer->stop();
    m_engineThinking = false;

    // Rebuild game from scratch up to replayTo moves
    Game newGame;
    const std::string& sf = m_game.start_fen();
    if (!sf.empty()) newGame.set_start_fen(sf);
    for (int i = 0; i < replayTo; ++i)
        newGame.make_move(hist[i]);
    newGame.check_game_status();

    // Preserve clocks if game had them
    if (m_game.has_clocks()) {
        // Restore to the same totals — simplest: keep current remaining
        newGame.set_clocks(
            (long long)whiteRemainingMs(),
            (long long)blackRemainingMs(),
            (long long)m_wIncMs, (long long)m_bIncMs);
        newGame.start_clock();
        m_clockTimer->start();
    }

    m_game = newGame;
    m_viewIndex = -1;

    if (replayTo > 0) {
        const auto& newHist = m_game.get_move_history();
        m_lastMoveFrom = newHist.back().square_from;
        m_lastMoveTo   = newHist.back().square_to;
    } else {
        m_lastMoveFrom = -1;
        m_lastMoveTo   = -1;
    }

    buildCapturedLists();
    emit positionChanged();
    emit clockChanged();

    // Let engine move if it's now the engine's turn
    if (m_engineReady)
        QMetaObject::invokeMethod(m_engineWorker, &EngineWorker::newGame, Qt::QueuedConnection);
    QTimer::singleShot(0, this, &GameBridge::maybeStartEngineMove);
}

