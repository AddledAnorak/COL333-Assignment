#include "mcts.hpp"
#include <algorithm>
#include <memory>
#include <utility>
#include <map>

// MCTSNode function definitions
MCTSNode::MCTSNode(
    std::shared_ptr<StonesAndRiversGameEnv> game,
    const GameState& state,
    GameMove actionTaken,
    int player,
    MCTSNode* parent,
    float probPrior,
    float explorationWeight
) : game(game), state(state), actionTaken(actionTaken), player(player),
    parent(parent), probPrior(probPrior), explorationWeight(explorationWeight) {
}

MCTSNode::~MCTSNode() {}

bool MCTSNode::isFullyExpanded() const {
    return !children.empty() || (state.isTerminal && visits > 0);
}

float MCTSNode::getUCB(const MCTSNode* child) const {
    float qValue = 0;

    if(child->visits > 0)
        qValue = -child->valueSum / child->visits;

    return qValue + explorationWeight * child->probPrior * 
           (std::sqrt(static_cast<float>(visits)) / (1.0f + child->visits));
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

void MCTSNode::expand() {
    std::vector<GameMove> validMoves = game->getValidMoves(state);
    
    for (GameMove action : validMoves) {
        GameState newState = game->step(state, action);
        newState = game->flipBoard(newState);
        int newPlayer = -player;
        
        auto childNode = std::make_unique<MCTSNode>(
            game, newState, action, newPlayer,
            this, game->getStateValue(newState), explorationWeight
        );
        
        children.push_back(std::move(childNode));
    }
}

void MCTSNode::backpropagate(float value) {
    visits++;
    valueSum += value;
    
    if (parent != nullptr) {
        parent->backpropagate(-value);
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
        MCTSNode* parent = root.get();
        
        // Selection phase
        while (parent->isFullyExpanded()) {
            if (parent->state.isTerminal)
                break;
            parent = parent->bestChild();
        }
        
        float value;

        // TODO: recheck
        if (!parent->state.isTerminal) {
            value = (playerID == parent->player? 1 : -1) * game->getStateValue(parent->state);
            parent->expand();
        } else {
            value = playerID == parent->player? WIN_REWARD : -WIN_REWARD;
        }
        
        // Backpropagation phase
        parent->backpropagate(value);
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
