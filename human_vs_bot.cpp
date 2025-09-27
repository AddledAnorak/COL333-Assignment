#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <chrono>

#include "src/customGameEngine.hpp"
#include "src/mcts.hpp"

// Forward declarations
void printBoard(const GameState& state, const StonesAndRiversGameEnv& env);
std::string moveToString(const GameMove& move);
void clearScreen();

int main() {
    StonesAndRiversGameEnv env;
    GameState state;

    // --- Setup MCTS Bot ---
    // RandomModel model(100, 50); // dummy state/action sizes, adjust if needed
    std::shared_ptr<StonesAndRiversGameEnv> botEnv = std::make_shared<StonesAndRiversGameEnv>();
    MCTS bot(botEnv, nullptr, /*numSimulations*/ 2e3, /*explorationWeight*/ 1.0f, /*playerID*/ 2);

    while (!env.isGameOver(state)) {
        clearScreen();
        printBoard(state, env);

        std::string currentPlayerStr = (state.currentPlayer == Player::CIRCLE) ? "Circle (🔴)" : "Square (🔵)";
        std::cout << "\n--- Player " << currentPlayerStr << "'s Turn ---" << std::endl;

        if (state.currentPlayer == Player::CIRCLE) {
            // --- Human Move ---
            auto validMoves = env.getValidMoves(state);
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
            state = env.step(state, chosenMove);
        } else {
            // --- MCTS Bot Move ---
            std::cout << "🤖 Bot is thinking..." << std::endl;
            auto start = std::chrono::high_resolution_clock::now();

            // Flip state for bot perspective
            GameState flipped = env.flipBoard(state);

            // Run MCTS
            auto moveProbs = bot.search(flipped);

            if (moveProbs.empty()) {
                std::cout << "Bot has no valid moves! Game ends." << std::endl;
                break;
            }

            // Pick best move (highest probability)
            auto bestMove = moveProbs[0].first;

            // Flip move back to human perspective
            // (MCTS works on flipped board, so we need to unflip)
            GameState testState = env.step(flipped, bestMove);
            GameState unflippedState = env.flipBoard(testState);

            // Find equivalent move in current state's valid moves
            auto validMoves = env.getValidMoves(state);
            GameMove chosenMove;
            bool found = false;
            for (auto& m : validMoves) {
                GameState next = env.step(state, m);
                if (env.checkEq(next, unflippedState)) {
                    chosenMove = m;
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "Bot move translation failed!" << std::endl;
                break;
            }

            std::cout << "🤖 Bot plays: " << moveToString(chosenMove) << std::endl;
            state = env.step(state, chosenMove);

            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = end - start;

            // Register move with MCTS root
            bot.registerMove(chosenMove);

            // Pause so human can see bot move
            std::cout << "Bot took " << duration.count() << " seconds to make a move.\n";
            std::cout << "Press Enter to continue...";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cin.get();
        }
    }

    // --- Game Over ---
    clearScreen();
    printBoard(state, env);
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
                 "), push to (" + std::to_string(move.pushed_to->r) + "," + std::to_string(move.pushed_to->c) + ")";
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

void printBoard(const GameState& state, const StonesAndRiversGameEnv& env) {
    std::cout << "    ";
    for (int c = 0; c < GameState::COLS; ++c) {
        printf("%-3d", c);
    }
    std::cout << "\n  +------------------------------------+\n";

    for (int r = 0; r < GameState::ROWS; ++r) {
        printf("%2d| ", r);
        for (int c = 0; c < GameState::COLS; ++c) {
            const auto& piece = state.getPieceAt(r, c);
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
    std::cout << "  +------------------------------------+\n";
}

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}
