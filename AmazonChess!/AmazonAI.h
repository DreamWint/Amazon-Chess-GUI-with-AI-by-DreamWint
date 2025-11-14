//cpp AmazonChess!\AmazonAI.h
#pragma once
#include <array>
#include <vector>
#include <utility>
#include <limits>

namespace AmazonChess
{
    // 返回值说明：
    // pair.first = movePacked, pair.second = arrowIndex
    // movePacked = fromIndex * (BOARD_SIZE*BOARD_SIZE) + toIndex
    // fromIndex = fromY * BOARD_SIZE + fromX
    // toIndex   = toY   * BOARD_SIZE + toX
    // arrowIndex = arrowY * BOARD_SIZE + arrowX  （若无箭位则为 -1）
    inline std::pair<int, int> GetBestMove(const std::array<std::array<PieceType, BOARD_SIZE>, BOARD_SIZE>& board, Player currentPlayer)
    {
        auto IsWithin = [](int x, int y) -> bool {
            return x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE;
        };

        auto IsCellEmpty = [&](int x, int y, const auto& b) -> bool {
            return IsWithin(x, y) && b[y][x] == PieceType::None;
        };

        static const int dirs[8][2] = {
            {1,0},{-1,0},{0,1},{0,-1},
            {1,1},{1,-1},{-1,1},{-1,-1}
        };

        // 计算某格在棋盘 b 上的可达格（不含被占格）
        auto GetReachable = [&](int sx, int sy, const auto& b) {
            std::vector<std::pair<int,int>> res;
            for (int d = 0; d < 8; ++d)
            {
                int dx = dirs[d][0], dy = dirs[d][1];
                int x = sx + dx, y = sy + dy;
                while (IsWithin(x, y) && IsCellEmpty(x, y, b))
                {
                    res.emplace_back(x, y);
                    x += dx; y += dy;
                }
            }
            return res;
        };

        // 计算玩家 p 在棋盘 b 上的开放度（所有 Amazon 的可达格数之和）
        auto Openness = [&](Player p, const auto& b) -> int {
            PieceType at = (p == Player::White) ? PieceType::WhiteAmazon : PieceType::BlackAmazon;
            int sum = 0;
            for (int y = 0; y < BOARD_SIZE; ++y)
            {
                for (int x = 0; x < BOARD_SIZE; ++x)
                {
                    if (b[y][x] == at)
                    {
                        auto r = GetReachable(x, y, b);
                        sum += static_cast<int>(r.size());
                    }
                }
            }
            return sum;
        };

        PieceType myType = (currentPlayer == Player::White) ? PieceType::WhiteAmazon : PieceType::BlackAmazon;
        Player oppPlayer = (currentPlayer == Player::White) ? Player::Black : Player::White;

        int bestScore = std::numeric_limits<int>::min();
        std::pair<int,int> bestMove = {-1, -1};

        const int squareCount = BOARD_SIZE * BOARD_SIZE;

        // 枚举我方每个 Amazon 的每个移动目标，移动后枚举所有可放箭的位置，评估最终开放度差
        for (int y = 0; y < BOARD_SIZE; ++y)
        {
            for (int x = 0; x < BOARD_SIZE; ++x)
            {
                if (board[y][x] != myType) continue;
                auto moveTargets = GetReachable(x, y, board);
                for (auto &mv : moveTargets)
                {
                    int tx = mv.first, ty = mv.second;
                    // 模拟移动：b2
                    auto b2 = board;
                    b2[ty][tx] = b2[y][x];
                    b2[y][x] = PieceType::None;

                    // 枚举箭的位置（从新位置出发）
                    auto arrowTargets = GetReachable(tx, ty, b2);
                    if (arrowTargets.empty())
                    {
                        // 没有箭位，直接评估 b2
                        int myOpen = Openness(currentPlayer, b2);
                        int oppOpen = Openness(oppPlayer, b2);
                        int score = myOpen - oppOpen;
                        if (score > bestScore)
                        {
                            int fromIndex = y * BOARD_SIZE + x;
                            int toIndex = ty * BOARD_SIZE + tx;
                            int movePacked = fromIndex * squareCount + toIndex;
                            bestScore = score;
                            bestMove.first = movePacked;
                            bestMove.second = -1;
                        }
                    }
                    else
                    {
                        for (auto &at : arrowTargets)
                        {
                            int ax = at.first, ay = at.second;
                            // 模拟放箭：b3
                            auto b3 = b2;
                            b3[ay][ax] = PieceType::Arrow;

                            int myOpen = Openness(currentPlayer, b3);
                            int oppOpen = Openness(oppPlayer, b3);
                            int score = myOpen - oppOpen;
                            if (score > bestScore)
                            {
                                int fromIndex = y * BOARD_SIZE + x;
                                int toIndex = ty * BOARD_SIZE + tx;
                                int movePacked = fromIndex * squareCount + toIndex;
                                int arrowIndex = ay * BOARD_SIZE + ax;
                                bestScore = score;
                                bestMove.first = movePacked;
                                bestMove.second = arrowIndex;
                            }
                        }
                    }
                }
            }
        }

        return bestMove;
    }
} // namespace AmazonChess