#ifndef CUSTOM_GAME_ENGINE_H
#define CUSTOM_GAME_ENGINE_H

#define WIN_REWARD 100

#include <iostream>
#include <vector>
#include <optional>
#include <string>
#include <set>
#include <variant>
#include <tuple>
#include <array> // OPTIMIZATION: Use std::array for the board

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

    // The operator< function allows GameMove to be used as a map key 🔑
    bool operator<(const GameMove& other) const {
        return std::tie(action, from, to, pushed_to, orientation) <
               std::tie(other.action, other.from, other.to, other.pushed_to, other.orientation);
    }
};

// Represents the entire state of the game
struct GameState {
    static constexpr int ROWS = 13; // OPTIMIZATION: Use constexpr
    static constexpr int COLS = 12;

    // OPTIMIZATION: A flat array improves cache locality and performance over a vector of vectors.
    std::array<std::optional<Piece>, ROWS * COLS> board;

    // OPTIMIZATION: Tracking piece positions avoids searching the whole board for moves.
    std::vector<Position> circlePieces;
    std::vector<Position> squarePieces;

    Player currentPlayer = Player::CIRCLE;
    bool isTerminal = false;
    std::optional<Player> winner = std::nullopt;

    GameState(); // Constructor to create the default starting board

    // OPTIMIZATION: Helper to access the flat board using 2D coordinates.
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

    /**
     * @brief Applies a move to a given state and returns the new state.
     * @param state The current state.
     * @param move The move to apply.
     * @return The new GameState after the move.
     */
    GameState step(const GameState& state, const GameMove& move);

    /**
     * @brief Flips the board for the other player's perspective.
     * @param state The current state.
     * @return A new GameState with the board and players flipped.
     */
    GameState flipBoard(const GameState& state) const;

    /**
     * @brief Checks if a move is valid for the given state.
     * @param state The current state.
     * @param move The move to check.
     * @return True if the move is valid, false otherwise.
     */
    bool isValidMove(const GameState& state, const GameMove& move) const;

    /**
     * @brief Gets all valid moves for the current player in a given state.
     * @param state The current state.
     * @return A vector of all possible GameMoves.
     */
    std::vector<GameMove> getValidMoves(const GameState& state) const;

    /**
     * @brief Checks if the game is over.
     * @param state The current state.
     * @return True if the game has ended, false otherwise.
     */
    bool isGameOver(const GameState& state) const;

    void displayBoard(const GameState& state) const;

    bool checkEq(const GameState& state1, const GameState& state2) const;
    bool checkEq(const GameMove& move1, const GameMove& move2) const;
    float getStateValue(const GameState& state) const;

    Player opponent(Player p) const;

private:
    // Game constants
    static constexpr int WIN_COUNT = 4;

    // OPTIMIZATION: Pre-calculated values to avoid re-computation in hot loops.
    std::vector<int> m_scoreCols;
    bool m_isOpponentScoreCellCache[2][GameState::ROWS][GameState::COLS];

    // Helper functions
    int topScoreRow() const;
    int bottomScoreRow() const;
    bool inBounds(int r, int c) const;
    bool isOpponentScoreCell(int r, int c, Player player) const;

    // Core game logic helpers
    void checkWin(GameState& state) const;
    
    // OPTIMIZATION: Added an override parameter to avoid copying the entire GameState when checking hypothetical moves (like flips/rotates).
    std::set<Position> getRiverFlowDestinations(
        const GameState& state, Position riverPos, Position sourcePos, Player movingPlayer, bool isPush = false,
        const std::optional<std::pair<Position, Piece>>& overridePiece = std::nullopt
    ) const;

    std::vector<GameMove> getValidMovesForPiece(const GameState& state, int r, int c) const;
};

#endif