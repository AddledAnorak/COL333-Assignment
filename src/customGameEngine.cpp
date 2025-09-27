#include "customGameEngine.hpp"
#include <queue>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <cmath>   // For std::tanh
#include <numeric> // For std::accumulate

// --- GameState Implementation ---
GameState::GameState() {
    board.fill(std::nullopt);
    int width = std::min(6, std::max(2, COLS - 6));
    int start_col = (COLS - width) / 2;

    for (int r : {3, 4}) {
        for (int c = start_col; c < start_col + width; ++c) {
            getPieceAt(r, c) = Piece{Player::SQUARE, Side::STONE, Orientation::NONE};
            squarePieces.push_back({r, c});
        }
    }
    for (int r : {ROWS - 5, ROWS - 4}) {
        for (int c = start_col; c < start_col + width; ++c) {
            getPieceAt(r, c) = Piece{Player::CIRCLE, Side::STONE, Orientation::NONE};
            circlePieces.push_back({r, c});
        }
    }
}

// --- StonesAndRiversGameEnv Implementation ---
StonesAndRiversGameEnv::StonesAndRiversGameEnv() {
    int w = 4;
    int start = std::max(0, (GameState::COLS - w) / 2);
    for (int i = start; i < start + w; ++i) {
        m_scoreCols.push_back(i);
        // NEW: Cache the scoring positions
        m_circleScorePositions.push_back({topScoreRow(), i});
        m_squareScorePositions.push_back({bottomScoreRow(), i});
    }

    for (int r = 0; r < GameState::ROWS; ++r) {
        for (int c = 0; c < GameState::COLS; ++c) {
            bool is_in_score_cols = (std::find(m_scoreCols.begin(), m_scoreCols.end(), c) != m_scoreCols.end());
            m_isOpponentScoreCellCache[0][r][c] = (r == bottomScoreRow() && is_in_score_cols);
            m_isOpponentScoreCellCache[1][r][c] = (r == topScoreRow() && is_in_score_cols);
        }
    }
}
StonesAndRiversGameEnv::~StonesAndRiversGameEnv() {}

bool StonesAndRiversGameEnv::isGameOver(const GameState& state) const {
    return state.isTerminal;
}

Player StonesAndRiversGameEnv::opponent(Player p) const {
    return (p == Player::CIRCLE) ? Player::SQUARE : Player::CIRCLE;
}

int StonesAndRiversGameEnv::topScoreRow() const { return 2; }
int StonesAndRiversGameEnv::bottomScoreRow() const { return GameState::ROWS - 3; }
bool StonesAndRiversGameEnv::inBounds(int r, int c) const {
    return r >= 0 && r < GameState::ROWS && c >= 0 && c < GameState::COLS;
}

bool StonesAndRiversGameEnv::isOpponentScoreCell(int r, int c, Player player) const {
    return m_isOpponentScoreCellCache[static_cast<int>(player)][r][c];
}

void StonesAndRiversGameEnv::checkWin(GameState& state) const {
    int circle_count = 0;
    int square_count = 0;

    for (int c : m_scoreCols) {
        const auto& piece_top = state.getPieceAt(topScoreRow(), c);
        if (piece_top && piece_top->owner == Player::CIRCLE && piece_top->side == Side::STONE) {
            circle_count++;
        }
        const auto& piece_bottom = state.getPieceAt(bottomScoreRow(), c);
        if (piece_bottom && piece_bottom->owner == Player::SQUARE && piece_bottom->side == Side::STONE) {
            square_count++;
        }
    }

    if (circle_count >= WIN_COUNT) {
        state.isTerminal = true;
        state.winner = Player::CIRCLE;
        return; // Win takes precedence over draw
    }
    if (square_count >= WIN_COUNT) {
        state.isTerminal = true;
        state.winner = Player::SQUARE;
        return; // Win takes precedence over draw
    }
    
    // NEW: Check for draw by move count
    if (state.numMoves >= GameState::MAX_MOVES) {
        state.isTerminal = true;
        state.winner = std::nullopt; // No winner means a draw
    }
}

std::set<Position> StonesAndRiversGameEnv::getRiverFlowDestinations(const GameState& state, Position riverPos, Position sourcePos, Player movingPlayer, bool isPush, const std::optional<std::pair<Position, Piece>>& overridePiece) const {
    std::set<Position> destinations;
    // OPTIMIZATION: A flat boolean array is much faster for "visited" checks than a std::set.
    std::array<bool, GameState::ROWS * GameState::COLS> visited{};
    std::queue<Position> q;

    q.push(riverPos);

    while (!q.empty()) {
        Position curr = q.front();
        q.pop();

        if (visited[curr.r * GameState::COLS + curr.c] || !inBounds(curr.r, curr.c)) continue;
        visited[curr.r * GameState::COLS + curr.c] = true;

        std::optional<Piece> flow_piece;
        // OPTIMIZATION: Use the override piece if provided for checking hypothetical moves without copying state.
        if (overridePiece && overridePiece->first == curr) {
            flow_piece = overridePiece->second;
        } else {
            flow_piece = state.getPieceAt(curr.r, curr.c);
        }

        if (isPush && curr == riverPos) {
            flow_piece = state.getPieceAt(sourcePos.r, sourcePos.c);
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

                const auto& next_cell = state.getPieceAt(nr, nc);
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
    const auto& piece_opt = state.getPieceAt(r, c);
    // This check should not be needed if called from getValidMoves, but it is safe to keep.
    if (!piece_opt || piece_opt->owner != state.currentPlayer) return moves;
    const auto& piece = *piece_opt;
    Player player = state.currentPlayer;

    // --- Generate Moves and Pushes ---
    int dr[] = {-1, 1, 0, 0};
    int dc[] = {0, 0, -1, 1};

    for (int i = 0; i < 4; ++i) {
        int nr = r + dr[i];
        int nc = c + dc[i];

        if (!inBounds(nr, nc) || isOpponentScoreCell(nr, nc, player)) continue;

        const auto& target_cell = state.getPieceAt(nr, nc);

        if (!target_cell) {
            moves.emplace_back(GameMove{GameMove::Action::MOVE, {r, c}, {{nr, nc}}, std::nullopt, std::nullopt});
        } else {
            if (target_cell->side == Side::RIVER) {
                auto destinations = getRiverFlowDestinations(state, {nr, nc}, {r, c}, player, false);
                for (const auto& dest : destinations) {
                    moves.emplace_back(GameMove{GameMove::Action::MOVE, {r, c}, {dest}, std::nullopt, std::nullopt});
                }
            } else if (target_cell->side == Side::STONE) {
                if (piece.side == Side::STONE) {
                    int pr = nr + dr[i];
                    int pc = nc + dc[i];
                    Player pushed_owner = target_cell->owner;
                    if (inBounds(pr, pc) && !state.getPieceAt(pr, pc) && !isOpponentScoreCell(pr, pc, pushed_owner)) {
                        moves.emplace_back(GameMove{GameMove::Action::PUSH, {r, c}, {{nr, nc}}, {{pr, pc}}, std::nullopt});
                    }
                } else {
                    Player pushed_owner = target_cell->owner;
                    auto destinations = getRiverFlowDestinations(state, {nr, nc}, {r, c}, pushed_owner, true);
                    for (const auto& dest : destinations) {
                        moves.emplace_back(GameMove{GameMove::Action::PUSH, {r, c}, {{nr, nc}}, {dest}, std::nullopt});
                    }
                }
            }
        }
    }

    // --- Generate Side-Specific Actions (Flips and Rotates) ---
    if (piece.side == Side::STONE) {
        for (auto ori : {Orientation::HORIZONTAL, Orientation::VERTICAL}) {
            // OPTIMIZATION: Check safety without creating a new GameState. Pass a hypothetical piece instead.
            Piece temp_piece = piece;
            temp_piece.side = Side::RIVER;
            temp_piece.orientation = ori;
            auto flow = getRiverFlowDestinations(state, {r, c}, {r, c}, player, false, {{{r, c}, temp_piece}});
            bool safe = true;
            for (const auto& dest : flow) {
                if (isOpponentScoreCell(dest.r, dest.c, player)) {
                    safe = false;
                    break;
                }
            }
            if (safe) {
                moves.emplace_back(GameMove{GameMove::Action::FLIP, {r, c}, std::nullopt, std::nullopt, {ori}});
            }
        }
    } else { // It's a river
        moves.emplace_back(GameMove{GameMove::Action::FLIP, {r, c}, std::nullopt, std::nullopt, std::nullopt});
        Orientation new_ori = (piece.orientation == Orientation::HORIZONTAL) ? Orientation::VERTICAL : Orientation::HORIZONTAL;
        // OPTIMIZATION: Check safety without creating a new GameState.
        Piece temp_piece = piece;
        temp_piece.orientation = new_ori;
        auto flow = getRiverFlowDestinations(state, {r, c}, {r, c}, player, false, {{{r,c}, temp_piece}});
        bool safe = true;
        for (const auto& dest : flow) {
            if (isOpponentScoreCell(dest.r, dest.c, player)) {
                safe = false;
                break;
            }
        }
        if (safe) {
            moves.emplace_back(GameMove{GameMove::Action::ROTATE, {r, c}, std::nullopt, std::nullopt, std::nullopt});
        }
    }
    return moves;
}

std::vector<GameMove> StonesAndRiversGameEnv::getValidMoves(const GameState& state) const {
    if (state.isTerminal) return {};
    std::vector<GameMove> all_moves;
    
    // OPTIMIZATION: Iterate only over the current player's pieces instead of the whole board.
    const auto& pieces = (state.currentPlayer == Player::CIRCLE) ? state.circlePieces : state.squarePieces;
    all_moves.reserve(pieces.size() * 10); // OPTIMIZATION: Pre-allocate memory to reduce reallocations.

    for (const auto& pos : pieces) {
        auto piece_moves = getValidMovesForPiece(state, pos.r, pos.c);
        all_moves.insert(all_moves.end(), piece_moves.begin(), piece_moves.end());
    }
    return all_moves;
}

bool StonesAndRiversGameEnv::isValidMove(const GameState& state, const GameMove& move) const {
    // OPTIMIZATION: Much faster validation. Generate moves for only the specific piece involved
    // and check if the move is in that small set. Avoids generating all moves for all pieces.
    if (!inBounds(move.from.r, move.from.c)) return false;
    const auto& piece = state.getPieceAt(move.from.r, move.from.c);
    if (!piece || piece->owner != state.currentPlayer) return false;

    auto valid_moves_for_piece = getValidMovesForPiece(state, move.from.r, move.from.c);
    for (const auto& valid_move : valid_moves_for_piece) {
        if (checkEq(move, valid_move)) {
            return true;
        }
    }
    return false;
}

GameState StonesAndRiversGameEnv::step(const GameState& state, const GameMove& move) {
    // OPTIMIZATION: For high-speed rollouts, the check for move validity is often skipped,
    // assuming the move comes from the `getValidMoves` list. I'm removing it here.
    // If you need validation, call isValidMove explicitly before calling step.
    // if (state.isTerminal || !isValidMove(state, move)) {
    //     return state;
    // }
    if (state.isTerminal) return state;

    GameState next_state = state;
    next_state.memoizedValue = std::nullopt;
    auto& board = next_state.board;
    auto piece = board[move.from.r * GameState::COLS + move.from.c];

    // Helper lambda to update piece positions in the tracking vectors
    auto update_piece_pos = [&](Player owner, Position from, Position to) {
        auto& piece_list = (owner == Player::CIRCLE) ? next_state.circlePieces : next_state.squarePieces;
        auto it = std::find_if(piece_list.begin(), piece_list.end(), [&](const Position& p){
            return p.r == from.r && p.c == from.c;
        });
        if (it != piece_list.end()) {
            it->r = to.r;
            it->c = to.c;
        }
    };

    switch (move.action) {
        case GameMove::Action::MOVE:
            next_state.getPieceAt(move.to->r, move.to->c) = piece;
            next_state.getPieceAt(move.from.r, move.from.c) = std::nullopt;
            update_piece_pos(piece->owner, move.from, *move.to);
            break;
        case GameMove::Action::PUSH: {
            auto pushed_piece = next_state.getPieceAt(move.to->r, move.to->c);
            next_state.getPieceAt(move.pushed_to->r, move.pushed_to->c) = pushed_piece;
            next_state.getPieceAt(move.to->r, move.to->c) = piece;
            next_state.getPieceAt(move.from.r, move.from.c) = std::nullopt;

            update_piece_pos(pushed_piece->owner, *move.to, *move.pushed_to);
            update_piece_pos(piece->owner, move.from, *move.to);

            if (next_state.getPieceAt(move.to->r, move.to->c)->side == Side::RIVER) {
                next_state.getPieceAt(move.to->r, move.to->c)->side = Side::STONE;
                next_state.getPieceAt(move.to->r, move.to->c)->orientation = Orientation::NONE;
            }
            break;
        }
        case GameMove::Action::FLIP:
            if (piece->side == Side::STONE) {
                next_state.getPieceAt(move.from.r, move.from.c)->side = Side::RIVER;
                next_state.getPieceAt(move.from.r, move.from.c)->orientation = *move.orientation;
            } else {
                next_state.getPieceAt(move.from.r, move.from.c)->side = Side::STONE;
                next_state.getPieceAt(move.from.r, move.from.c)->orientation = Orientation::NONE;
            }
            break;
        case GameMove::Action::ROTATE:
            if (piece->orientation == Orientation::HORIZONTAL) {
                next_state.getPieceAt(move.from.r, move.from.c)->orientation = Orientation::VERTICAL;
            } else {
                next_state.getPieceAt(move.from.r, move.from.c)->orientation = Orientation::HORIZONTAL;
            }
            break;
    }

    // INCREMENT MOVE COUNT
    next_state.numMoves++;

    checkWin(next_state);

    if (!next_state.isTerminal) {
        next_state.currentPlayer = opponent(state.currentPlayer);
    }

    return next_state;
}

GameState StonesAndRiversGameEnv::flipBoard(const GameState& state) const {
    GameState flipped_state;
    flipped_state.board.fill(std::nullopt);
    flipped_state.circlePieces.clear();
    flipped_state.squarePieces.clear();

    for (int r = 0; r < GameState::ROWS; ++r) {
        for (int c = 0; c < GameState::COLS; ++c) {
            if (state.getPieceAt(r, c).has_value()) {
                Piece new_piece = state.getPieceAt(r, c).value();
                new_piece.owner = opponent(new_piece.owner);
                int new_r = GameState::ROWS - 1 - r;
                flipped_state.getPieceAt(new_r, c) = new_piece;

                // OPTIMIZATION: Populate new piece lists while flipping
                if (new_piece.owner == Player::CIRCLE) {
                    flipped_state.circlePieces.push_back({new_r, c});
                } else {
                    flipped_state.squarePieces.push_back({new_r, c});
                }
            }
        }
    }

    // Copy the move count
    flipped_state.numMoves = state.numMoves;

    flipped_state.currentPlayer = opponent(state.currentPlayer);
    flipped_state.isTerminal = state.isTerminal;
    if (state.winner.has_value()) {
        flipped_state.winner = opponent(state.winner.value());
    }

    return flipped_state;
}

/**
 * @brief Heuristic 2: Calculates the sum of shortest paths for the 4 closest stones to reach any 4 scoring positions.
 * This is a fast approximation using BFS. A lower number is better.
 */
int StonesAndRiversGameEnv::calculateMinStepsToScore(const GameState& state, Player player) const {
    const auto& pieces = (player == Player::CIRCLE) ? state.circlePieces : state.squarePieces;
    const auto& score_positions = (player == Player::CIRCLE) ? m_circleScorePositions : m_squareScorePositions;

    std::vector<int> all_min_distances;
    all_min_distances.reserve(pieces.size());

    for (const auto& piece_pos : pieces) {
        if(state.getPieceAt(piece_pos.r, piece_pos.c)->side != Side::STONE) continue;

        // BFS to find shortest path from this piece to any score cell
        std::queue<std::pair<Position, int>> q;
        q.push({piece_pos, 0});
        std::array<bool, GameState::ROWS * GameState::COLS> visited{};
        visited[piece_pos.r * GameState::COLS + piece_pos.c] = true;
        
        int min_dist = 999; // Sentinel value

        while(!q.empty()){
            auto [curr_pos, dist] = q.front();
            q.pop();

            // Check if this is a score position
            for(const auto& score_pos : score_positions){
                if(curr_pos.r == score_pos.r && curr_pos.c == score_pos.c){
                    min_dist = dist;
                    goto found_path; // Exit both loops
                }
            }

            if (dist > 20) continue; // Optimization: don't search too far

            // Explore neighbors
            int dr[] = {-1, 1, 0, 0};
            int dc[] = {0, 0, -1, 1};
            for(int i = 0; i < 4; ++i){
                int nr = curr_pos.r + dr[i];
                int nc = curr_pos.c + dc[i];

                if(inBounds(nr, nc) && !visited[nr * GameState::COLS + nc]){
                    // Can only move through empty cells for this heuristic
                    if(!state.getPieceAt(nr, nc) || (nr == score_positions[0].r && std::find(m_scoreCols.begin(), m_scoreCols.end(), nc) != m_scoreCols.end())){
                        visited[nr * GameState::COLS + nc] = true;
                        q.push({{nr, nc}, dist + 1});
                    }
                }
            }
        }
        found_path:;
        if(min_dist != 999) {
            all_min_distances.push_back(min_dist);
        }
    }

    if (all_min_distances.empty()) return 999 * 4;

    std::sort(all_min_distances.begin(), all_min_distances.end());
    int total_dist = 0;
    int count = 0;
    for(int dist : all_min_distances) {
        if(count >= 4) break;
        total_dist += dist;
        count++;
    }
    return total_dist;
}

float StonesAndRiversGameEnv::getStateValue(const GameState& state) const {
    if (state.memoizedValue.has_value()) {
        return *state.memoizedValue;
    }
    
    // --- 1. Handle Terminal States ---
    if (state.isTerminal) {
        if (state.winner.has_value()) {
            return (*state.winner == state.currentPlayer) ? WIN_REWARD : -WIN_REWARD;
        }
        return 0.0f; // Draw
    }

    // --- 2. Calculate Heuristics for a Non-Terminal State ---
    auto calculate_score_for_player = [&](Player player) {
        float score = 0.0f;
        
        const float W_MOBILITY = 0.2f;
        const float W_DISTANCE = 0.5f;
        const float W_SCORE_POS = 15.0f;

        // Heuristic 1: Mobility (number of travellable locations)
        std::set<Position> unique_dests;
        const auto& pieces = (player == Player::CIRCLE) ? state.circlePieces : state.squarePieces;
        for (const auto& pos : pieces) {
            auto moves = getValidMovesForPiece(state, pos.r, pos.c);
            for(const auto& move : moves){
                if(move.to.has_value()) unique_dests.insert(*move.to);
            }
        }
        score += unique_dests.size() * W_MOBILITY;

        // Heuristic 2: Distance to scoring zone (lower is better)
        int distance = calculateMinStepsToScore(state, player);
        score -= distance * W_DISTANCE;

        // Heuristic 3: Number of stones already in scoring locations
        int stones_in_score = 0;
        const auto& score_positions = (player == Player::CIRCLE) ? m_circleScorePositions : m_squareScorePositions;
        for(const auto& pos : score_positions){
            const auto& piece = state.getPieceAt(pos.r, pos.c);
            if(piece && piece->owner == player && piece->side == Side::STONE){
                stones_in_score++;
            }
        }
        score += stones_in_score * W_SCORE_POS;
        
        return score;
    };

    float current_player_score = calculate_score_for_player(state.currentPlayer);
    float opponent_player_score = calculate_score_for_player(opponent(state.currentPlayer));

    // std::cout << "Score for player: " << current_player_score << std::endl;
    // std::cout << "Score for opp: " << opponent_player_score << std::endl;

    float raw_value = current_player_score - opponent_player_score;
    
    // --- 3. Normalize and Return ---
    // Use tanh to smoothly scale the value into the [-WIN_REWARD, WIN_REWARD] range.
    // This prevents extreme values from single heuristics and provides a stable gradient.
    const float scaling_factor = 50.0f;
    float final_value = std::tanh(raw_value / scaling_factor) * WIN_REWARD;
    state.memoizedValue = final_value;
    return final_value;
}


bool StonesAndRiversGameEnv::checkEq(const GameState& state1, const GameState& state2) const {
    if (state1.currentPlayer != state2.currentPlayer ||
        state1.isTerminal != state2.isTerminal ||
        state1.winner != state2.winner) {
        return false;
    }

    // This board comparison is simple and correct with the flat array.
    for (size_t i = 0; i < GameState::ROWS * GameState::COLS; ++i) {
        const auto& p1 = state1.board[i];
        const auto& p2 = state2.board[i];
        if (p1.has_value() != p2.has_value()) return false;
        if (p1.has_value()) {
            if (p1->owner != p2->owner || p1->side != p2->side || p1->orientation != p2->orientation) {
                return false;
            }
        }
    }
    return true;
}

bool StonesAndRiversGameEnv::checkEq(const GameMove& move1, const GameMove& move2) const {
    return (move1.action == move2.action &&
            move1.from == move2.from &&
            move1.to == move2.to &&
            move1.pushed_to == move2.pushed_to &&
            move1.orientation == move2.orientation);
}

void StonesAndRiversGameEnv::displayBoard(const GameState& state) const {
    std::cout << "\n  ";
    for (int c = 0; c < GameState::COLS; ++c) {
        std::cout << " " << std::setw(2) << c; // Column headers
    }
    std::cout << "\n  -------------------------------------\n";

    for (int r = 0; r < GameState::ROWS; ++r) {
        std::cout << std::setw(2) << r << "|"; // Row header
        for (int c = 0; c < GameState::COLS; ++c) {
            // OPTIMIZATION FIX: Use the getPieceAt helper for the flat board array.
            const auto& piece = state.getPieceAt(r, c);
            
            if (!piece) {
                std::cout << " . ";
            } else {
                // Determine character for the player ('O' or 'S')
                char player_char = (piece->owner == Player::CIRCLE) ? 'O' : 'S';
                char side_char;

                // Determine character for the side (Stone '#' or River '-'/ '|')
                if (piece->side == Side::RIVER) {
                    side_char = (piece->orientation == Orientation::HORIZONTAL) ? '-' : '|';
                } else {
                    side_char = '#';
                }
                std::cout << player_char << side_char << " ";
            }
        }
        std::cout << "|\n";
    }
    std::cout << "  -------------------------------------\n";
    std::cout << "Current Player: " << ((state.currentPlayer == Player::CIRCLE) ? "CIRCLE (O)" : "SQUARE (S)") << "\n";
    if (state.isTerminal) {
        if(state.winner){
            std::cout << "Winner: " << ((state.winner == Player::CIRCLE) ? "CIRCLE (O)" : "SQUARE (S)") << "!\n";
        } else {
            std::cout << "Game is over! It's a draw.\n";
        }
    }
}
