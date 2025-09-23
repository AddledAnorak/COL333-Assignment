#ifndef MCTS_H
#define MCTS_H

#include "customGameEngine.hpp"
#include <vector>
#include <memory>
#include <cmath>

class Model {
public:
    virtual ~Model() = default;
    virtual std::pair<std::vector<float>, float> predict(const std::vector<float>& encodedState) = 0;
};

class MCTSNode {
public:
    MCTSNode(
        std::shared_ptr<StonesAndRiversGameEnv> game,
        const GameState& state,
        GameMove actionTaken,
        int player = 1,
        MCTSNode* parent = nullptr,
        float probPrior = 1.0f,
        float explorationWeight = 1.0f
    );

    ~MCTSNode();

    bool isFullyExpanded() const;
    float getUCB(const MCTSNode* child) const;
    MCTSNode* bestChild() const;
    void expand();
    void backpropagate(float value);

    std::shared_ptr<StonesAndRiversGameEnv> game;
    GameState state;
    GameMove actionTaken;
    int player;
    MCTSNode* parent;
    float probPrior;
    float explorationWeight;
    float valueSum;
    int visits;
    
    std::vector<std::unique_ptr<MCTSNode>> children;
};

class MCTS {
public:
    MCTS(std::shared_ptr<StonesAndRiversGameEnv> game, Model* model, int numSimulations = 1000, float explorationWeight = 1.0f, int playerID = 1);
    ~MCTS() = default;

    std::vector<std::pair<GameMove, float>> search(const GameState& state);
    void registerMove(const GameMove& move);

private:
    std::shared_ptr<StonesAndRiversGameEnv> game;
    Model* model;
    std::unique_ptr<MCTSNode> root;
    int numSimulations;
    float explorationWeight;
    int playerID;
};

class RandomModel : public Model {
public:
    RandomModel(int stateSize, int actionSize);
    std::pair<std::vector<float>, float> predict(const std::vector<float>& encodedState) override;

private:
    int stateSize;
    int actionSize;
};

#endif
