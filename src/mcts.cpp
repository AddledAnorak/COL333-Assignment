#include "mcts.hpp"
#include <algorithm>
#include <memory>
#include <utility>
#include <map>
#include <fstream>
#include <chrono>

// MCTSNode function definitions
MCTSNode::MCTSNode(
    std::shared_ptr<StonesAndRiversGameEnv> game,
    const GameState& state,
    GameMove actionTaken,
    int player,
    MCTSNode* parent,
    float probPrior,
    float explorationWeight,
    int height
) : game(game), state(state), actionTaken(actionTaken), player(player), parent(parent),
    probPrior(probPrior), explorationWeight(explorationWeight), height(height) {
}

MCTSNode::~MCTSNode() {}

bool MCTSNode::isFullyExpanded() const {
    return !children.empty() || (state.isTerminal && visits > 0);
}

// float MCTSNode::getUCB(const MCTSNode* child) const {
//     float qValue = 0;

//     if(child->visits > 0)
//         qValue = -child->valueSum / child->visits;

//     return qValue + explorationWeight * child->probPrior * 
//            (std::sqrt(static_cast<float>(visits)) / (1.0f + child->visits));
// }

float MCTSNode::getUCB(const MCTSNode* child) const {
    // EDIT 1 & 2: The UCB calculation now uses the state's heuristic value for unvisited
    // nodes, providing a more informed initial estimate than infinity.
    float exploitation_value;
    if (child->visits == 0) {
        // For an unvisited node, estimate its value using the game's heuristic function.
        // This provides an informed starting point instead of assuming the best.
        // We negate the value because getStateValue is from the child's perspective (the opponent).
        exploitation_value = -this->game->getStateValue(child->state);
    } else {
        // For a visited node, use the average value from its completed rollouts.
        exploitation_value = -child->valueSum / static_cast<float>(child->visits);
    }

    // --- Exploration Component (U-value) ---
    // This term remains the same, encouraging visits to less-explored nodes.
    const float exploration_value = this->explorationWeight * child->probPrior *
                                    (std::sqrt(static_cast<float>(this->visits)) / (1.0f + static_cast<float>(child->visits)));

    return exploitation_value + exploration_value;
}

MCTSNode* MCTSNode::bestChild() const {
    if (children.empty()) {
        return nullptr;
    }
    
    auto bestIt = std::max_element(children.begin(), children.end(),
        [this](const std::unique_ptr<MCTSNode>& a, const std::unique_ptr<MCTSNode>& b) {
            return getUCB(a.get()) < getUCB(b.get());
        });
    
    return bestIt->get();
}

// void MCTSNode::expand() {
//     std::vector<GameMove> validMoves = game->getValidMoves(state);
    
//     for (GameMove action : validMoves) {
//         GameState newState = game->step(state, action);
//         newState = game->flipBoard(newState);
//         int newPlayer = -player;
        
//         auto childNode = std::make_unique<MCTSNode>(
//             game, newState, action, newPlayer,
//             this, /* priorProbability */ game->getStateValue(newState), explorationWeight
//         );
        
//         children.push_back(std::move(childNode));
//     }
// }

void MCTSNode::expand() {
    // If the game state at this node is terminal, there are no further moves to expand.
    if (this->state.isTerminal) {
        return;
    }

    // Get all possible moves from the current game state.
    auto valid_moves = this->game->getValidMoves(this->state);
    if (valid_moves.empty()) {
        return;
    }
    
    // EDIT 3: Calculate priors based on the heuristic value of resulting states,
    // instead of using a uniform distribution. This focuses the search on promising moves.
    std::vector<float> move_values;
    move_values.reserve(valid_moves.size());
    float max_value = -std::numeric_limits<float>::infinity();

    std::vector<GameState> next_states; // Store states to avoid redundant `step` calls
    next_states.reserve(valid_moves.size());

    for (const auto& move : valid_moves) {
        GameState next_state = this->game->step(this->state, move);
        // The value is from the next player's perspective, so a high value is bad for us. We negate it.
        float value = -this->game->getStateValue(next_state);
        move_values.push_back(value);
        next_states.push_back(next_state);
        if (value > max_value) {
            max_value = value;
        }
    }

    // Use softmax to convert raw heuristic values into a probability distribution (priors).
    // This ensures that better moves get a higher prior probability.
    const float temperature = 1.0f; // Controls sharpness of the distribution
    float sum_exp = 0.0f;
    std::vector<float> priors;
    priors.reserve(move_values.size());

    for (float value : move_values) {
        // Subtract max_value for numerical stability to avoid overflow with exp()
        float exp_val = std::exp((value - max_value) / temperature);
        priors.push_back(exp_val);
        sum_exp += exp_val;
    }

    // --- Create Child Nodes with the new Heuristic-based Priors ---
    for (size_t i = 0; i < valid_moves.size(); ++i) {
        // Normalize to get final probabilities. Fallback to uniform if sum is zero.
        float prior = (sum_exp > 1e-6) ? priors[i] / sum_exp : 1.0f / static_cast<float>(valid_moves.size());
        
        children.push_back(std::make_unique<MCTSNode>(
            this->game,
            next_states[i], // Use the pre-calculated state
            valid_moves[i],
            (this->player == 1) ? -1 : 1, // Toggle player
            this,                          // Parent
            prior,                         // Prior probability
            this->explorationWeight,       // Inherit exploration weight
            this->height + 1               // Increment height
        ));
    }
}

void MCTSNode::backpropagate(float value, int height) {
    this->height = std::max(this->height, height);
    visits++;
    valueSum += value;
    
    if (parent != nullptr) {
        parent->backpropagate(-value, height+1);
    }
}


// MCTS main functions
MCTS::MCTS(std::shared_ptr<StonesAndRiversGameEnv> game, Model* model, int numSimulations, 
    float explorationWeight, int playerID)
    : game(game), model(model), numSimulations(numSimulations), 
      explorationWeight(explorationWeight), playerID(playerID) {
    root = nullptr;
}

std::vector<std::pair<GameMove, float>> MCTS::search(const GameState& state) {
    // std::vector<std::vector<double>> dataPoints(numSimulations, std::vector<double>(4, 0));
    // int numDatapoints = 0;

    // check if any of the child states are equal to state 2 levels down
    bool createNew = true;
    if (root) {
        for (auto& child : root->children) {
            if (game->checkEq(child->state, state)) {
                // If we found a matching child, we can use it
                root = std::move(child);
                root->parent = nullptr;
                createNew = false;
                break;
            }
        }
    }

    if(createNew)
        root = std::make_unique<MCTSNode>(
            game, state, GameMove{GameMove::Action::FLIP, {-1, -1}}, 
            playerID, nullptr, 1.0f, explorationWeight);
    
    
    for (int i = 0; i < numSimulations; ++i) {
        if((i+1) % (numSimulations/10) == 0) {
            std::cout << i << "/" << numSimulations << " done" << std::endl;
        }

        MCTSNode* parent = root.get();
        
        // Selection phase
        auto startExpansion = std::chrono::high_resolution_clock::now();
        int depth = 0;
        while (parent->isFullyExpanded()) {
            if (parent->state.isTerminal)
                break;
            parent = parent->bestChild();
            depth++;
        }
        auto endExpansion = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> expansionDuration = endExpansion - startExpansion;
        
        float value;

        // TODO: recheck
        if (!parent->state.isTerminal) {
            value = (playerID == parent->player? 1 : -1) * game->getStateValue(parent->state);
            parent->expand();
        } else {
            value = playerID == parent->player? WIN_REWARD : -WIN_REWARD;
        }
        
        // Backpropagation phase
        auto startBackprop = std::chrono::high_resolution_clock::now();
        parent->backpropagate(value);
        auto endBackprop = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> backpropDuration = endBackprop - startBackprop;

        // // add to the datapoints
        // dataPoints[numDatapoints][0] = i;
        // dataPoints[numDatapoints][1] = depth;
        // dataPoints[numDatapoints][2] = expansionDuration.count();
        // dataPoints[numDatapoints++][3] = backpropDuration.count();
    }
    
    // Calculate action probabilities based on visit counts
    std::vector<std::pair<GameMove, float>> probs(root->children.size());
    float totalVisits = 0.0f;
    int i = 0;
    for (const auto& child : root->children) {
        probs[i] = std::make_pair(child->actionTaken, child->visits);
        totalVisits += child->visits;
        i++;
    }

    // Normalize probabilities
    if (totalVisits > 0.0f) {
        for (auto& prob : probs) {
            prob.second /= totalVisits;
        }
    }


    // // write data to file
    // std::ofstream outputFile("output_withvalue_30k.txt");
    // if (outputFile.is_open()) {
    //     // 3. Iterate through the 2D vector
    //     for (const auto& row : dataPoints) {
    //         for (double element : row) {
    //             // Write each element to the file, followed by a space
    //             outputFile << element << " ";
    //         }
    //         // Add a newline character after each row
    //         outputFile << "\n";
    //     }
    
    //     // 4. Close the file
    //     outputFile.close();
    //     std::cout << "Vector successfully saved to output.txt" << std::endl;
    // } else {
    //     std::cerr << "Error: Unable to open the file for writing." << std::endl;
    // }


    return probs;
}

void MCTS::registerMove(const GameMove& move) {
    if(root) {
        for(auto &child : root->children) {
            if (game->checkEq(child->actionTaken, move)) {
                root = std::move(child);
                root->parent = nullptr;
                break;
            }
        }
    }
}