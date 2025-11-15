#pragma once

#include <windows.h>
#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <string>
#include "resource.h"

// 亚马逊棋核心类型与界面接口（C++14）
// 仅声明与轻量实现占位符：为以后在 .cpp 中实现游戏逻辑、渲染与鼠标处理保留接口。
// 设计目标：
// - 8x8 棋盘（坐标以左下角为 (0,0)，x 向右，y 向上）
// - 支持添加棋子（白 Amazon、黑 Amazon、棕色 Arrow）和加载棋盘/棋子图片资源
// - 支持鼠标交互：选中己方 Amazon 时高亮可达格子（移动或发箭阶段）
// - 提供检查一方是否被封死的接口
// - 新增：记录走子记谱、保存/载入棋谱

namespace AmazonChess
{
    // 棋盘尺寸
    static constexpr int BOARD_SIZE = 8;

    // 棋子类型（Arrow 无所属方）
    enum class PieceType : uint8_t
    {
        None = 0,
        WhiteAmazon,
        BlackAmazon,
        Arrow
    };

    // 玩家侧
    enum class Player : int8_t
    {
        None = -1,
        White = 0,
        Black = 1
    };

    // 坐标：以 (x,y) 表示，0<=x,y<8。注意：左下角为 (0,0)
    struct Pos
    {
        int x;
        int y;
        Pos() : x(-1), y(-1) {}
        Pos(int _x, int _y) : x(_x), y(_y) {}
        bool operator==(const Pos& o) const { return x == o.x && y == o.y; }
        bool IsValid() const { return x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE; }
    };

    // 单格信息（仅存储类型，必要时可扩展）
    struct Cell
    {
        PieceType type;
        Cell() : type(PieceType::None) {}
    };

    // 当前玩家回合哪个阶段：先移动 Amazon 再发箭
    enum class TurnPhase : uint8_t
    {
        SelectAmazon = 0, // 等待玩家选择己方 Amazon
        MoveAmazon,       // 已选择 Amazon，等待选择目标移动格
        ShootArrow        // 已完成移动，等待选择箭的目标格
    };

    // 资源句柄容器（位图/图像占位），实现时可替换为 GDI+ 或 Direct2D 资源
    struct UIResources
    {
        // HBITMAP / HICON / ID 等由实现决定
        HBITMAP hBoardBitmap = nullptr;
        HBITMAP hWhiteAmazon = nullptr;
        HBITMAP hBlackAmazon = nullptr;
        HBITMAP hArrow = nullptr;

        // 在实现中负责释放句柄
        void Release();
    };

    // 游戏主类（声明）
    class Game
    {
    public:
        Game();
        ~Game();

        // 初始化/重置到初始布局（使用题述的默认位置）
        void Reset();

        // 载入 UI 资源（占位接口），由外部在 WinMain/Init 中调用
        // hInst: 应用实例句柄，GetModuleHandle 或传入 hInst
        // resourceIDs: 可选，用于传入资源 id（占位）
        void LoadResources(HINSTANCE hInst);

        // 将像素坐标转换为棋盘格坐标（根据 SetBoardRect 设置的绘制区域）
        Pos PixelToCell(POINT pt) const;

        // 将格子坐标转换为对应的绘制矩形（用于绘制棋子 / 高亮）
        RECT CellToRect(const Pos& p) const;

        // 设置棋盘在窗口客户区中的绘制矩形（整张棋盘的像素区域）
        void SetBoardRect(const RECT& rcBoard);

        // 鼠标消息处理接口（在 WndProc 中调用）
        // 返回 true 表示已处理并需要重绘
        bool OnLButtonDown(HWND hWnd, int x, int y);
        bool OnMouseMove(HWND hWnd, int x, int y);
        bool OnLButtonUp(HWND hWnd, int x, int y);

        // 绘制接口：在 WM_PAINT 中调用，传入 HDC 和 客户区矩形
        void OnPaint(HDC hdc, const RECT& clientRect);

        // 查询 / 编辑棋盘状态（AI 或其它模块可用）
        PieceType GetPieceAt(const Pos& p) const;
        bool AddPiece(PieceType type, const Pos& p); // 将棋子放到空格（不做规则检查）
        bool RemovePiece(const Pos& p);

        // 根据规则生成可达格（类似于象棋中的女王走法，遇棋阻挡）
        // 不包括发箭后的阻挡（即以当前棋盘状态为准）
        std::vector<Pos> GetReachableFrom(const Pos& from) const;

        // 移动 Amazon（含合法性检查），并进入发箭阶段
        // 返回 true 表示移动成功
        bool MoveAmazon(const Pos& from, const Pos& to);

        // 发射箭（部署 Arrow），箭为永久存在、无阵营
        // 返回 true 表示部署成功并结束回合（切换玩家）
        bool ShootArrow(const Pos& target);

        // 当前回合玩家与阶段访问
        Player CurrentPlayer() const { return currentPlayer; }
        TurnPhase CurrentPhase() const { return phase; }

        // 新增：设置当前玩家（用于逐步重放中设置玩家）
        void SetCurrentPlayer(Player p) { currentPlayer = p; }

        // 检查指定玩家是否被封死（即所有 Amazon 无任何可移动位置）
        bool IsPlayerTrapped(Player player) const;

        // 检查游戏结束并返回胜者（None 表示未结束）
        Player GetWinner() const;

        // 游戏板数据直接访问（只读）
        const std::array<std::array<Cell, BOARD_SIZE>, BOARD_SIZE>& BoardGrid() const { return board; }

        // 高亮信息（用于 UI）
        const std::vector<Pos>& Highlighted() const { return highlighted; }
        Pos SelectedAmazon() const { return selected; }

        // ----- 记谱与存读档 -----
        // 将当前记谱保存到指定文件（utf-8），返回是否成功
        bool SaveToFile(const std::wstring& path) const;
        // 从指定文件读取记谱并重放（从初始局面开始），返回是否成功
        bool LoadFromFile(const std::wstring& path);

        // 访问 / 清理记谱
        const std::vector<std::wstring>& GetMoveList() const;
        void ClearMoveList();

    private:
        // 内部辅助
        bool IsCellEmpty(const Pos& p) const;
        bool IsWithinBoard(const Pos& p) const;
        void ClearHighlights();
        void ToggleNextPlayer();

        // 记录一手（在发箭完成时由 ShootArrow 调用）
        void RecordMove(Player player, const Pos& from, const Pos& to, const Pos& arrow, bool gameEnd);

        std::array<std::array<Cell, BOARD_SIZE>, BOARD_SIZE> board;
        UIResources resources;

        // 回合控制
        Player currentPlayer;
        TurnPhase phase;

        // 用户交互状态
        Pos selected; // 选中的 Amazon（如果有）
        std::vector<Pos> highlighted;

        // 绘制转换（棋盘像素区域）
        RECT boardRect; // pixel rect of the whole board (left, top, right, bottom)

        // 记谱数据（每行： "W 0,2 2,0 3,3"；如局末附加 '*'）
        std::vector<std::wstring> moves;

        // 为了记录完整一手，MoveAmazon 保存 from/to，ShootArrow 使用它们 + arrow
        Pos lastMoveFrom;
        Pos lastMoveTo;
    };

    // 工厂 / 全局辅助：返回可访问的全局游戏实例（便于在 WndProc 中直接访问）
    // 注意：实现文件中负责实例化
    Game& GetGlobalGame();
} // namespace AmazonChess
