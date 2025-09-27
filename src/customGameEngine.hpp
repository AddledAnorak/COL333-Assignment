#ifndef CUSTOM_GAME_ENGINE_H
#define CUSTOM_GAME_ENGINE_H

#define WIN_REWARD 100.0f

#include <iostream>
#include <vector>
#include <optional>
#include <string>
#include <set>
#include <variant>
#include <tuple>
#include <array> 

// Enum for players for type safety
enum class Player {
    CIRCLE,
    SQUARE
};

// Enum for the two sides of a piece
enum class Side {
    STONE,
    RIVER
};

// Enum for river orientation
enum class Orientation {
    NONE,
    HORIZONTAL,
    VERTICAL
};

// Represents a position on the board (row, col)
struct Position {
    int r, c;

    // Operator for using Position in sets/maps
    bool operator<(const Position& other) const {
        return std::tie(r, c) < std::tie(other.r, other.c);
    }
    bool operator==(const Position& other) const {
        return r == other.r && c == other.c;
    }
};

// Represents a single piece on the board
struct Piece {
    Player owner;
    Side side = Side::STONE;
    Orientation orientation = Orientation::NONE;
};

// Represents a game move
struct GameMove {
    enum class Action { MOVE, PUSH, FLIP, ROTATE };
    Action action;
    Position from;
    std::optional<Position> to;
    std::optional<Position> pushed_to;
    std::optional<Orientation> orientation;

    bool operator<(const GameMove& other) const {
        return std::tie(action, from, to, pushed_to, orientation) <
               std::tie(other.action, other.from, other.to, other.pushed_to, other.orientation);
    }
};

// Represents the entire state of the game
struct GameState {
    static constexpr int ROWS = 13;
    static constexpr int COLS = 12;
    static constexpr int MAX_MOVES = 1000; // Game ends in a draw after this many moves

    std::array<std::optional<Piece>, ROWS * COLS> board;
    std::vector<Position> circlePieces;
    std::vector<Position> squarePieces;

    Player currentPlayer = Player::CIRCLE;
    bool isTerminal = false;
    std::optional<Player> winner = std::nullopt;
    int numMoves = 0; // NEW: Move counter
    mutable std::optional<float> memoizedValue = std::nullopt;

    GameState(); 

    const std::optional<Piece>& getPieceAt(int r, int c) const {
        return board[r * COLS + c];
    }
    std::optional<Piece>& getPieceAt(int r, int c) {
        return board[r * COLS + c];
    }
};

// The main game environment class
class StonesAndRiversGameEnv {
public:
    StonesAndRiversGameEnv();
    ~StonesAndRiversGameEnv();

    GameState step(const GameState& state, const GameMove& move);
    GameState flipBoard(const GameState& state) const;
    bool isValidMove(const GameState& state, const GameMove& move) const;
    std::vector<GameMove> getValidMoves(const GameState& state) const;
    bool isGameOver(const GameState& state) const;

    /**
     * @brief Calculates the value of the state from the perspective of the current player.
     * @param state The current state.
     * @return A float between -WIN_REWARD and +WIN_REWARD.
     * Positive means the current player is winning, negative means losing.
     */
    float getStateValue(const GameState& state) const;

    void displayBoard(const GameState& state) const; // For debugging
    bool checkEq(const GameState& state1, const GameState& state2) const;
    bool checkEq(const GameMove& move1, const GameMove& move2) const;
    Player opponent(Player p) const;

private:
    static constexpr int WIN_COUNT = 4;

    std::vector<int> m_scoreCols;
    bool m_isOpponentScoreCellCache[2][GameState::ROWS][GameState::COLS];
    std::vector<Position> m_circleScorePositions;
    std::vector<Position> m_squareScorePositions;

    int topScoreRow() const;
    int bottomScoreRow() const;
    bool inBounds(int r, int c) const;
    bool isOpponentScoreCell(int r, int c, Player player) const;

    void checkWin(GameState& state) const;
    std::set<Position> getRiverFlowDestinations(
        const GameState& state, Position riverPos, Position sourcePos, Player movingPlayer, bool isPush = false,
        const std::optional<std::pair<Position, Piece>>& overridePiece = std::nullopt
    ) const;
    std::vector<GameMove> getValidMovesForPiece(const GameState& state, int r, int c) const;

    // NEW: Helper function for the distance heuristic in getStateValue
    int calculateMinStepsToScore(const GameState& state, Player player) const;
};

#endif
