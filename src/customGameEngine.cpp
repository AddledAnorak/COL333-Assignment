#include "customGameEngine.hpp"
#include <queue>
#include <algorithm>

// --- GameState Implementation ---
GameState::GameState() {
    board.resize(ROWS, std::vector<std::optional<Piece>>(COLS, std::nullopt));
    int width = std::min(6, std::max(2, COLS - 6));
    int start_col = (COLS - width) / 2;

    // Player::SQUARE pieces (top)
    for (int r : {3, 4}) {
        for (int c = start_col; c < start_col + width; ++c) {
            board[r][c] = Piece{Player::SQUARE, Side::STONE, Orientation::NONE};
        }
    }
    // Player::CIRCLE pieces (bottom)
    for (int r : {ROWS - 5, ROWS - 4}) {
        for (int c = start_col; c < start_col + width; ++c) {
            board[r][c] = Piece{Player::CIRCLE, Side::STONE, Orientation::NONE};
        }
    }
}

// --- StonesAndRiversGameEnv Implementation ---
StonesAndRiversGameEnv::StonesAndRiversGameEnv() {}
StonesAndRiversGameEnv::~StonesAndRiversGameEnv() {}

bool StonesAndRiversGameEnv::isGameOver(const GameState& state) const {
    return state.isTerminal;
}

Player StonesAndRiversGameEnv::opponent(Player p) const {
    return (p == Player::CIRCLE) ? Player::SQUARE : Player::CIRCLE;
}

int StonesAndRiversGameEnv::topScoreRow() const { return 2; }
int StonesAndRiversGameEnv::bottomScoreRow(int rows) const { return rows - 3; }
bool StonesAndRiversGameEnv::inBounds(int r, int c) const {
    return r >= 0 && r < GameState::ROWS && c >= 0 && c < GameState::COLS;
}

std::vector<int> StonesAndRiversGameEnv::scoreCols(int cols) const {
    int w = 4;
    int start = std::max(0, (cols - w) / 2);
    std::vector<int> cols_vec;
    for (int i = start; i < start + w; ++i) {
        cols_vec.push_back(i);
    }
    return cols_vec;
}

bool StonesAndRiversGameEnv::isOpponentScoreCell(int r, int c, Player player) const {
    auto sc = scoreCols(GameState::COLS);
    bool is_in_score_cols = (std::find(sc.begin(), sc.end(), c) != sc.end());

    if (player == Player::CIRCLE) { // Opponent is SQUARE, their score row is bottom
        return r == bottomScoreRow(GameState::ROWS) && is_in_score_cols;
    } else { // Opponent is CIRCLE, their score row is top
        return r == topScoreRow() && is_in_score_cols;
    }
}

void StonesAndRiversGameEnv::checkWin(GameState& state) const {
    int circle_count = 0;
    int square_count = 0;
    auto sc = scoreCols(GameState::COLS);

    // Circle scores at the top
    for (int c : sc) {
        const auto& piece = state.board[topScoreRow()][c];
        if (piece && piece->owner == Player::CIRCLE && piece->side == Side::STONE) {
            circle_count++;
        }
    }

    // Square scores at the bottom
    for (int c : sc) {
        const auto& piece = state.board[bottomScoreRow(GameState::ROWS)][c];
        if (piece && piece->owner == Player::SQUARE && piece->side == Side::STONE) {
            square_count++;
        }
    }
    
    if (circle_count >= WIN_COUNT) {
        state.isTerminal = true;
        state.winner = Player::CIRCLE;
    } else if (square_count >= WIN_COUNT) {
        state.isTerminal = true;
        state.winner = Player::SQUARE;
    }
}

std::set<Position> StonesAndRiversGameEnv::getRiverFlowDestinations(const GameState& state, Position riverPos, Position sourcePos, Player movingPlayer, bool isPush) const {
    std::set<Position> destinations;
    std::set<Position> visited;
    std::queue<Position> q;

    q.push(riverPos);

    while (!q.empty()) {
        Position curr = q.front();
        q.pop();

        if (visited.count(curr) || !inBounds(curr.r, curr.c)) continue;
        visited.insert(curr);

        // Get the piece that determines the flow's properties
        std::optional<Piece> flow_piece = state.board[curr.r][curr.c];

        // If this is a river push, the first location's flow is determined
        // by the pushing river at sourcePos, not the piece being pushed.
        if (isPush && curr == riverPos) {
            flow_piece = state.board[sourcePos.r][sourcePos.c];
        }

        if (!flow_piece) {
             if (!isOpponentScoreCell(curr.r, curr.c, movingPlayer)) {
                destinations.insert(curr);
            }
            continue;
        }

        if (flow_piece->side != Side::RIVER) continue;

        std::vector<std::pair<int, int>> dirs;
        if (flow_piece->orientation == Orientation::HORIZONTAL) {
            dirs = {{0, 1}, {0, -1}};
        } else {
            dirs = {{1, 0}, {-1, 0}};
        }

        for (auto [dr, dc] : dirs) {
            int nr = curr.r + dr;
            int nc = curr.c + dc;
            while (inBounds(nr, nc)) {
                if (isOpponentScoreCell(nr, nc, movingPlayer)) break;

                const auto& next_cell = state.board[nr][nc];
                if (!next_cell) {
                    destinations.insert({nr, nc});
                    nr += dr; nc += dc;
                    continue;
                }
                
                if (nr == sourcePos.r && nc == sourcePos.c) {
                     nr += dr; nc += dc;
                     continue;
                }

                if (next_cell->side == Side::RIVER) {
                    q.push({nr, nc});
                    break;
                }
                break;
            }
        }
    }
    return destinations;
}

std::vector<GameMove> StonesAndRiversGameEnv::getValidMovesForPiece(const GameState& state, int r, int c) const {
    std::vector<GameMove> moves;
    const auto& piece = state.board[r][c];
    if (!piece || piece->owner != state.currentPlayer) return moves;

    Player player = state.currentPlayer;

    // --- Generate Moves and Pushes (for BOTH Stones and Rivers) ---
    int dr[] = {-1, 1, 0, 0};
    int dc[] = {0, 0, -1, 1};

    for (int i = 0; i < 4; ++i) {
        int nr = r + dr[i];
        int nc = c + dc[i];

        if (!inBounds(nr, nc) || isOpponentScoreCell(nr, nc, player)) continue;

        const auto& target_cell = state.board[nr][nc];

        // Case 1: Move to an empty cell
        if (!target_cell) {
            moves.push_back({GameMove::Action::MOVE, {r, c}, {{nr, nc}}, std::nullopt, std::nullopt});
        }
        // Case 2: Move onto a river or push another piece
        else { 
            // Subcase 2a: Moving piece flows through a target river (self-movement)
            if (target_cell->side == Side::RIVER) {
                auto destinations = getRiverFlowDestinations(state, {nr, nc}, {r, c}, player, false);
                for (const auto& dest : destinations) {
                    moves.push_back({GameMove::Action::MOVE, {r, c}, {dest}, std::nullopt, std::nullopt});
                }
            } 
            // Subcase 2b: Pushing a target stone (Rivers cannot push other Rivers)
            else if (target_cell->side == Side::STONE) {
                if (piece->side == Side::STONE) { // Stone pushing a stone (one step)
                    int pr = nr + dr[i];
                    int pc = nc + dc[i];
                    Player pushed_owner = target_cell->owner;
                    if (inBounds(pr, pc) && !state.board[pr][pc] && !isOpponentScoreCell(pr, pc, pushed_owner)) {
                        moves.push_back({GameMove::Action::PUSH, {r, c}, {{nr, nc}}, {{pr, pc}}, std::nullopt});
                    }
                } else { // River pushing a stone (can be long distance)
                    Player pushed_owner = target_cell->owner;
                    // Call with isPush=true. Flow starts from the pushed piece's location {nr, nc}.
                    auto destinations = getRiverFlowDestinations(state, {nr, nc}, {r, c}, pushed_owner, true);
                    for (const auto& dest : destinations) {
                        moves.push_back({GameMove::Action::PUSH, {r, c}, {{nr, nc}}, {dest}, std::nullopt});
                    }
                }
            }
        }
    }

    // --- Generate Side-Specific Actions (Flips and Rotates) ---
    if (piece->side == Side::STONE) {
        // A stone can flip to a river
        for (auto ori : {Orientation::HORIZONTAL, Orientation::VERTICAL}) {
             GameState temp_state = state;
             temp_state.board[r][c]->side = Side::RIVER;
             temp_state.board[r][c]->orientation = ori;
             auto flow = getRiverFlowDestinations(temp_state, {r, c}, {r, c}, player, false);
             bool safe = true;
             for(const auto& dest : flow){
                 if(isOpponentScoreCell(dest.r, dest.c, player)){
                     safe = false;
                     break;
                 }
             }
             if(safe){
                moves.push_back({GameMove::Action::FLIP, {r, c}, std::nullopt, std::nullopt, {ori}});
             }
        }
    } else { // It's a river
        // A river can flip back to a stone
        moves.push_back({GameMove::Action::FLIP, {r, c}, std::nullopt, std::nullopt, std::nullopt});

        // A river can rotate
        Orientation new_ori = (piece->orientation == Orientation::HORIZONTAL) ? Orientation::VERTICAL : Orientation::HORIZONTAL;
        GameState temp_state = state;
        temp_state.board[r][c]->orientation = new_ori;
        auto flow = getRiverFlowDestinations(temp_state, {r, c}, {r, c}, player, false);
        bool safe = true;
        for(const auto& dest : flow){
            if(isOpponentScoreCell(dest.r, dest.c, player)){
                safe = false;
                break;
            }
        }
        if (safe) {
            moves.push_back({GameMove::Action::ROTATE, {r, c}, std::nullopt, std::nullopt, std::nullopt});
        }
    }
    return moves;
}

std::vector<GameMove> StonesAndRiversGameEnv::getValidMoves(const GameState& state) const {
    if (state.isTerminal) return {};
    std::vector<GameMove> all_moves;
    for (int r = 0; r < GameState::ROWS; ++r) {
        for (int c = 0; c < GameState::COLS; ++c) {
            auto piece_moves = getValidMovesForPiece(state, r, c);
            all_moves.insert(all_moves.end(), piece_moves.begin(), piece_moves.end());
        }
    }
    return all_moves;
}

bool StonesAndRiversGameEnv::isValidMove(const GameState& state, const GameMove& move) const {
    auto valid_moves = getValidMoves(state);
    for(const auto& valid_move : valid_moves) {
        if (valid_move.action == move.action && valid_move.from == move.from) {
             if (move.to == valid_move.to && move.pushed_to == valid_move.pushed_to && move.orientation == valid_move.orientation) {
                 return true;
             }
        }
    }
    return false;
}

GameState StonesAndRiversGameEnv::step(const GameState& state, const GameMove& move) {
    if (state.isTerminal || !isValidMove(state, move)) {
        return state; // Return original state if game is over or move is invalid
    }

    GameState next_state = state;
    auto& board = next_state.board;
    auto piece = board[move.from.r][move.from.c];

    switch (move.action) {
        case GameMove::Action::MOVE:
            board[move.to->r][move.to->c] = piece;
            board[move.from.r][move.from.c] = std::nullopt;
            break;
        case GameMove::Action::PUSH:
            board[move.pushed_to->r][move.pushed_to->c] = board[move.to->r][move.to->c];
            board[move.to->r][move.to->c] = piece;
            board[move.from.r][move.from.c] = std::nullopt;
            // If a river pushes, it becomes a stone
            if (board[move.to->r][move.to->c]->side == Side::RIVER) {
                 board[move.to->r][move.to->c]->side = Side::STONE;
                 board[move.to->r][move.to->c]->orientation = Orientation::NONE;
            }
            break;
        case GameMove::Action::FLIP:
            if (piece->side == Side::STONE) {
                board[move.from.r][move.from.c]->side = Side::RIVER;
                board[move.from.r][move.from.c]->orientation = *move.orientation;
            } else {
                board[move.from.r][move.from.c]->side = Side::STONE;
                board[move.from.r][move.from.c]->orientation = Orientation::NONE;
            }
            break;
        case GameMove::Action::ROTATE:
            if (piece->orientation == Orientation::HORIZONTAL) {
                board[move.from.r][move.from.c]->orientation = Orientation::VERTICAL;
            } else {
                board[move.from.r][move.from.c]->orientation = Orientation::HORIZONTAL;
            }
            break;
    }

    // Check for a win condition
    checkWin(next_state);

    // Switch player
    if (!next_state.isTerminal) {
        next_state.currentPlayer = opponent(state.currentPlayer);
    }
    
    return next_state;
}

GameState StonesAndRiversGameEnv::flipBoard(const GameState& state) const {
    GameState flipped_state;
    flipped_state.board.assign(GameState::ROWS, std::vector<std::optional<Piece>>(GameState::COLS, std::nullopt));

    for (int r = 0; r < GameState::ROWS; ++r) {
        for (int c = 0; c < GameState::COLS; ++c) {
            if (state.board[r][c].has_value()) {
                Piece new_piece = state.board[r][c].value();
                new_piece.owner = opponent(new_piece.owner);
                int new_r = GameState::ROWS - 1 - r;
                flipped_state.board[new_r][c] = new_piece;
            }
        }
    }

    flipped_state.currentPlayer = opponent(state.currentPlayer);
    flipped_state.isTerminal = state.isTerminal;
    if(state.winner.has_value()){
        flipped_state.winner = opponent(state.winner.value());
    }

    return flipped_state;
}


float StonesAndRiversGameEnv::getStateValue(const GameState& state) const {
    return 0;
}

bool StonesAndRiversGameEnv::checkEq(const GameState& state1, const GameState& state2) const {
    // 1. Compare simple properties first for a quick exit
    if (state1.currentPlayer != state2.currentPlayer ||
        state1.isTerminal != state2.isTerminal ||
        state1.winner != state2.winner) {
        return false;
    }

    // 2. Compare the boards cell by cell
    for (int r = 0; r < GameState::ROWS; ++r) {
        for (int c = 0; c < GameState::COLS; ++c) {
            const auto& p1 = state1.board[r][c];
            const auto& p2 = state2.board[r][c];

            // Check if one cell is empty and the other is not
            if (p1.has_value() != p2.has_value()) {
                return false;
            }

            // If both cells have a piece, compare the piece's properties
            if (p1.has_value()) {
                if (p1->owner != p2->owner ||
                    p1->side != p2->side ||
                    p1->orientation != p2->orientation) {
                    return false;
                }
            }
        }
    }

    // If all checks pass, the states are equal
    return true;
}

bool StonesAndRiversGameEnv::checkEq(const GameMove& move1, const GameMove& move2) const {
    // The '==' operator is already defined for Position, std::optional, and enums,
    // so we can compare all members directly.
    return (move1.action == move2.action &&
            move1.from == move2.from &&
            move1.to == move2.to &&
            move1.pushed_to == move2.pushed_to &&
            move1.orientation == move2.orientation);
}