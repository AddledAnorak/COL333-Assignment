#include <iostream>
#include <random>
#include <chrono>
#include <string>

#include "src/customGameEngine.hpp"

// Forward declarations
std::string moveToString(const GameMove& move);

int main() {
    StonesAndRiversGameEnv env;
    GameState state;

    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    int moveCounter = 0;
    const int MAX_RANDOM_MOVES = 200; // safety cap

    while (!env.isGameOver(state) && moveCounter < MAX_RANDOM_MOVES) {
        auto validMoves = env.getValidMoves(state);
        if (validMoves.empty()) {
            std::cout << "No valid moves left! Game ends.\n";
            break;
        }

        // Pick a random move
        std::uniform_int_distribution<int> dist(0, (int)validMoves.size() - 1);
        const GameMove& chosenMove = validMoves[dist(rng)];

        // Apply move
        state = env.step(state, chosenMove);

        if(state.memoizedValue.has_value()) {
            std::cout << "state value is memed" << std::endl;
        }

        moveCounter++;

        // Show board + value
        std::cout << "\n================ Move " << moveCounter << " ================\n";
        std::cout << "Player " 
                  << ((state.currentPlayer == Player::CIRCLE) ? "Square (🔵)" : "Circle (🔴)") 
                  << " just moved.\n";
        std::cout << "Move: " << moveToString(chosenMove) << "\n\n";

        // Use the display style from your Human vs Bot code
        env.displayBoard(state);

        float value = env.getStateValue(state);
        std::cout << "\nBoard value (for current player): " << value << "\n";

        // Pause until user presses Enter
        std::cout << "\nPress ENTER to continue...\n";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    std::cout << "\n=============== GAME OVER ===============\n";
    if (state.winner) {
        std::string winnerStr = (*state.winner == Player::CIRCLE) ? "Circle (🔴)" : "Square (🔵)";
        std::cout << "Winner: " << winnerStr << "\n";
    } else {
        std::cout << "Game ended in a draw.\n";
    }
    return 0;
}

// --- Helper ---
std::string moveToString(const GameMove& move) {
    std::string s;
    switch (move.action) {
        case GameMove::Action::MOVE:
            s = "MOVE (" + std::to_string(move.from.r) + "," + std::to_string(move.from.c) +
                ") -> (" + std::to_string(move.to->r) + "," + std::to_string(move.to->c) + ")";
            break;
        case GameMove::Action::PUSH:
            s = "PUSH (" + std::to_string(move.from.r) + "," + std::to_string(move.from.c) +
                ") -> (" + std::to_string(move.to->r) + "," + std::to_string(move.to->c) +
                "), push to (" + std::to_string(move.pushed_to->r) + "," + std::to_string(move.pushed_to->c) + ")";
            break;
        case GameMove::Action::FLIP:
            s = "FLIP at (" + std::to_string(move.from.r) + "," + std::to_string(move.from.c) + ")";
            if (move.orientation) {
                s += (*move.orientation == Orientation::HORIZONTAL) ? " to HORIZONTAL" : " to VERTICAL";
            } else {
                s += " to STONE";
            }
            break;
        case GameMove::Action::ROTATE:
            s = "ROTATE at (" + std::to_string(move.from.r) + "," + std::to_string(move.from.c) + ")";
            break;
    }
    return s;
}
