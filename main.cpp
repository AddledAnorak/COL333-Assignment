#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <chrono>
#include <memory> // Required for std::shared_ptr

#include "src/customGameEngine.hpp"
#include "src/mcts_optimized.hpp"

// Forward declarations
void printBoard(const GameState& state);
std::string moveToString(const GameMove& move);
void clearScreen();

int main() {
    auto env = std::make_shared<StonesAndRiversGameEnv>();
    GameState state;

    // --- Setup MCTS Bot ---
    // The bot is initialized once with the game environment.
    // Let's give it more simulations for a stronger AI.
    MCTS bot(env, /*numSimulations*/ 100, /*explorationWeight*/ 1.41f);

    while (!state.isTerminal) {
        clearScreen();
        printBoard(state);

        std::string currentPlayerStr = (state.currentPlayer == Player::CIRCLE) ? "Circle (🔴)" : "Square (🔵)";
        std::cout << "\n--- Player " << currentPlayerStr << "'s Turn ---" << std::endl;

        if (state.currentPlayer == Player::CIRCLE) {
            // --- Human Move ---
            auto validMoves = env->getValidMoves(state);
            if (validMoves.empty()) {
                std::cout << "No valid moves available! Game ends." << std::endl;
                break;
            }

            std::cout << "Available Moves:" << std::endl;
            for (size_t i = 0; i < validMoves.size(); ++i) {
                std::cout << "  " << i + 1 << ". " << moveToString(validMoves[i]) << std::endl;
            }

            int moveChoice = 0;
            while (true) {
                std::cout << "\nEnter the number of your move (1-" << validMoves.size() << "): ";
                std::cin >> moveChoice;

                if (std::cin.good() && moveChoice >= 1 && moveChoice <= static_cast<int>(validMoves.size())) {
                    break;
                } else {
                    std::cout << "Invalid input. Try again." << std::endl;
                    std::cin.clear();
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                }
            }

            const GameMove& chosenMove = validMoves[moveChoice - 1];
            state = env->step(state, chosenMove);
            
            // **CORRECTION**: After the human moves, tell the bot which move was made
            // so it can reuse its search tree.
            bot.update_root(chosenMove);

        } else {
            // --- MCTS Bot Move ---
            std::cout << "🤖 Bot is thinking..." << std::endl;

            auto start = std::chrono::high_resolution_clock::now();

            // **CORRECTION**: The search function now directly takes the current state
            // and returns the best move. No board flipping needed.
            GameMove bestMove = bot.search(state);

            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = end - start;
            
            std::cout << "🤖 Bot plays: " << moveToString(bestMove) << std::endl;
            std::cout << "   (Thinking time: " << duration.count() << " seconds)\n";

            state = env->step(state, bestMove);
            // **CORRECTION**: The bot's search automatically updates its root,
            // so we don't need to call update_root after the bot's turn.

            // Pause so human can see bot move
            std::cout << "Press Enter to continue...";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cin.get();
        }
    }

    // --- Game Over ---
    clearScreen();
    printBoard(state);
    std::cout << "\n================ GAME OVER ================\n";
    if (state.winner) {
        std::string winnerStr = (*state.winner == Player::CIRCLE) ? "Circle (🔴)" : "Square (🔵)";
        std::cout << "🎉 The winner is Player " << winnerStr << "! 🎉\n";
    } else {
        std::cout << "The game is a draw!\n";
    }
    std::cout << "===========================================\n";
    return 0;
}

// --- Helper Functions ---

std::string moveToString(const GameMove& move) {
    std::string s = "";
    switch (move.action) {
        case GameMove::Action::MOVE:
            s += "MOVE (" + std::to_string(move.from.r) + "," + std::to_string(move.from.c) +
                 ") -> (" + std::to_string(move.to->r) + "," + std::to_string(move.to->c) + ")";
            break;
        case GameMove::Action::PUSH:
            s += "PUSH (" + std::to_string(move.from.r) + "," + std::to_string(move.from.c) +
                 ") -> (" + std::to_string(move.to->r) + "," + std::to_string(move.to->c) +
                 "), pushes to (" + std::to_string(move.pushed_to->r) + "," + std::to_string(move.pushed_to->c) + ")";
            break;
        case GameMove::Action::FLIP:
            s += "FLIP at (" + std::to_string(move.from.r) + "," + std::to_string(move.from.c) + ")";
            if (move.orientation) {
                s += (*move.orientation == Orientation::HORIZONTAL) ? " to HORIZONTAL" : " to VERTICAL";
            } else {
                s += " to STONE";
            }
            break;
        case GameMove::Action::ROTATE:
            s += "ROTATE at (" + std::to_string(move.from.r) + "," + std::to_string(move.from.c) + ")";
            break;
    }
    return s;
}

void printBoard(const GameState& state) {
    std::cout << "      ";
    for (int c = 0; c < GameState::COLS; ++c) {
        printf("%-3d", c);
    }
    std::cout << "\n   +------------------------------------+\n";

    for (int r = 0; r < GameState::ROWS; ++r) {
        printf("%2d | ", r);
        for (int c = 0; c < GameState::COLS; ++c) {
            const auto& piece = state.board[r][c];
            if (!piece) {
                std::cout << " . ";
            } else {
                if (piece->owner == Player::CIRCLE) {
                    if (piece->side == Side::STONE) std::cout << "🔴 ";
                    else if (piece->orientation == Orientation::HORIZONTAL) std::cout << " C↔";
                    else std::cout << " C↕";
                } else {
                    if (piece->side == Side::STONE) std::cout << "🔵 ";
                    else if (piece->orientation == Orientation::HORIZONTAL) std::cout << " S↔";
                    else std::cout << " S↕";
                }
            }
        }
        std::cout << " |";
        if (r == 2) std::cout << " <-- Circle (🔴) Scoring Row";
        if (r == 10) std::cout << " <-- Square (🔵) Scoring Row";
        std::cout << "\n";
    }
    std::cout << "   +------------------------------------+\n";
}

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}
