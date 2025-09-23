#include "mcts_optimized.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

// MCTSNode Method Definitions
// ==========================

void MCTSNode::init(
    std::shared_ptr<StonesAndRiversGameEnv> game_env,
    const GameState& initial_state,
    Player current_player,
    MCTSNode* parent_node,
    std::optional<GameMove> move,
    float prior,
    float exploration
) {
    this->game = game_env;
    this->state = initial_state;
    this->player = current_player;
    this->parent = parent_node;
    this->action_taken = move;
    this->prior_prob = prior;
    this->exploration_weight = exploration;
    this->value_sum = 0.0;
    this->visit_count = 0;
    this->children.clear();
}

bool MCTSNode::is_fully_expanded() const {
    // A node is fully expanded if it has children or if the game state is terminal.
    return !children.empty() || state.isTerminal;
}

float MCTSNode::get_ucb_score() const {
    // UCB (Upper Confidence Bound) formula balances exploitation and exploration.
    if (!parent) {
        return std::numeric_limits<float>::infinity();
    }
    
    // Q-value (exploitation term): average value from this child's perspective.
    // The parent wants to maximize its value, which is the negative of the child's value.
    double q_value = 0.0;
    if (visit_count > 0) {
        q_value = -value_sum / visit_count;
    }

    // U-value (exploration term): favors less-visited nodes.
    float u_value = exploration_weight * prior_prob * (std::sqrt(static_cast<float>(parent->visit_count)) / (1.0f + visit_count));

    return static_cast<float>(q_value) + u_value;
}


MCTSNode* MCTSNode::select_best_child() const {
    MCTSNode* best_child = nullptr;
    float max_ucb = -std::numeric_limits<float>::infinity();

    for (const auto& pair : children) {
        MCTSNode* child = pair.second;
        float ucb = child->get_ucb_score();
        if (ucb > max_ucb) {
            max_ucb = ucb;
            best_child = child;
        }
    }
    return best_child;
}

void MCTSNode::backpropagate(float value) {
    // Update visit counts and value sums from this node up to the root.
    MCTSNode* current = this;
    while (current != nullptr) {
        current->visit_count++;
        current->value_sum += value;
        value = -value; // The value is negated for the parent's perspective.
        current = current->parent;
    }
}

// NodePool Method Definitions
// ===========================

NodePool::NodePool(size_t size) : next_available_idx(0) {
    pool.resize(size);
}

MCTSNode* NodePool::get_new_node() {
    if (next_available_idx >= pool.size()) {
        throw std::runtime_error("MCTS Node Pool exhausted. Increase pool size.");
    }
    return &pool[next_available_idx++];
}

void NodePool::reset() {
    next_available_idx = 0;
}


// MCTS Method Definitions
// =======================

MCTS::MCTS(
    std::shared_ptr<StonesAndRiversGameEnv> game_env,
    int simulations,
    float exploration
) : game(game_env), 
    num_simulations(simulations),
    exploration_weight(exploration) {
}

void MCTS::expand(MCTSNode* node) {
    std::vector<GameMove> valid_moves = game->getValidMoves(node->state);
    Player next_player = game->opponent(node->player);

    for (const auto& move : valid_moves) {
        GameState next_state = game->step(node->state, move);
        
        MCTSNode* child_node = node_pool.get_new_node();
        child_node->init(
            game,
            next_state,
            next_player,
            node, // parent
            move,
            1.0f / valid_moves.size(), // Uniform prior probability
            exploration_weight
        );
        node->children[move] = child_node;
    }
}

GameMove MCTS::search(const GameState& initial_state) {
    // If the root is null or doesn't match the new state, create a new tree.
    // Note: For better performance, add a 'last_move' field to GameState
    // to find the child node without a full state comparison.
    bool reuse_tree = false;
    if (root) {
        for(const auto& pair : root->children) {
            if(game->checkEq(pair.second->state, initial_state)) {
                root = pair.second;
                root->parent = nullptr; // Detach from old parent
                reuse_tree = true;
                break;
            }
        }
    }

    if (!reuse_tree) {
        node_pool.reset();
        root = node_pool.get_new_node();
        root->init(game, initial_state, initial_state.currentPlayer, nullptr, std::nullopt, 1.0f, exploration_weight);
    }
    
    for (int i = 0; i < num_simulations; ++i) {
        MCTSNode* node = root;
        
        // 1. Selection: Traverse the tree using UCB to find a promising leaf node.
        while (node->is_fully_expanded()) {
            if (node->state.isTerminal) break;
            node = node->select_best_child();
        }
        
        // 2. Expansion: If the node is not terminal, create its children.
        if (!node->state.isTerminal) {
            expand(node);
        }

        // 3. Simulation & 4. Backpropagation
        float value;
        if (node->state.isTerminal) {
            // If terminal, the value is determined by the winner.
            if (!node->state.winner.has_value()) {
                value = 0.0f; // Draw
            } else if (node->state.winner.value() == node->player) {
                value = WIN_REWARD; // Win for the player whose turn it is
            } else {
                value = -WIN_REWARD; // Loss
            }
        } else {
            // For non-terminal nodes, use the game's heuristic function.
            value = game->getStateValue(node->state);
        }
        
        node->backpropagate(value);
    }
    
    // After all simulations, pick the move leading to the most visited child node.
    MCTSNode* best_node = nullptr;
    int max_visits = -1;
    for(const auto& pair : root->children) {
        if(pair.second->visit_count > max_visits) {
            max_visits = pair.second->visit_count;
            best_node = pair.second;
        }
    }

    if (!best_node || !best_node->action_taken.has_value()) {
        // Fallback: if no simulations were run or something went wrong, return the first valid move.
        return game->getValidMoves(initial_state)[0];
    }
    
    return best_node->action_taken.value();
}


void MCTS::update_root(const GameMove& move) {
    if (!root) return;

    // OPTIMIZATION: Use O(log N) map lookup to find the new root.
    auto it = root->children.find(move);
    if (it != root->children.end()) {
        root = it->second;
        root->parent = nullptr; // The new root has no parent.
    } else {
        // The opponent made a move we didn't expect. The tree is now invalid.
        root = nullptr;
    }
}