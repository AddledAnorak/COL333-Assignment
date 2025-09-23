#ifndef MCTS_OPTIMIZED_H
#define MCTS_OPTIMIZED_H

#include "customGameEngine.hpp" // Use the provided game engine header
#include <vector>
#include <memory>
#include <cmath>
#include <map>

class MCTSNode; // Forward declaration

// OPTIMIZATION: A memory pool to avoid frequent, slow heap allocations for nodes.
class NodePool {
private:
    std::vector<MCTSNode> pool;
    size_t next_available_idx;
public:
    NodePool(size_t size = 200000); // Pre-allocate memory for many nodes
    MCTSNode* get_new_node();
    void reset();
};

class MCTSNode {
public:
    MCTSNode() = default; // Required for placement in the pool's vector

    void init(
        std::shared_ptr<StonesAndRiversGameEnv> game_env,
        const GameState& state,
        Player player,
        MCTSNode* parent = nullptr,
        std::optional<GameMove> action_taken = std::nullopt,
        float prior_prob = 1.0f,
        float exploration_weight = 1.41f
    );

    bool is_fully_expanded() const;
    float get_ucb_score() const;
    MCTSNode* select_best_child() const;
    void backpropagate(float value);

    std::shared_ptr<StonesAndRiversGameEnv> game;
    GameState state;
    Player player; // The player who will move from this node
    MCTSNode* parent;
    std::optional<GameMove> action_taken; // The move that led to this node
    float prior_prob;
    float exploration_weight;
    
    double value_sum = 0.0;
    int visit_count = 0;
    
    // OPTIMIZATION: Map for O(log N) lookup of child nodes by the move taken.
    // Raw pointers are used because the NodePool manages the memory.
    std::map<GameMove, MCTSNode*> children;
};

class MCTS {
public:
    MCTS(
        std::shared_ptr<StonesAndRiversGameEnv> game_env,
        int num_simulations = 1000,
        float exploration_weight = 1.41f
    );

    // Main search function to find the best move from a given state.
    GameMove search(const GameState& initial_state);

    // Updates the root of the tree to the node corresponding to the given move.
    void update_root(const GameMove& move);

private:
    std::shared_ptr<StonesAndRiversGameEnv> game;
    MCTSNode* root = nullptr;
    int num_simulations;
    float exploration_weight;

    // The MCTS instance owns the memory pool for all its nodes.
    NodePool node_pool;
    
    // Helper function to expand a node, creating all its children.
    void expand(MCTSNode* node);
};

#endif // MCTS_OPTIMIZED_H