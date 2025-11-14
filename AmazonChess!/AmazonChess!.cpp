// AmazonChess!.cpp : 定义应用程序的入口点，并包含 Game 的实现，使用 GDI+ 绘制。
// 使用 C++14
#include "framework.h"
#include "AmazonChess!.h"

#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <chrono>
#include <string>
#include <fstream>
#include <sstream>
#include <locale>
#include <codecvt>
#include <commdlg.h>
#include "AmazonAI.h"

using namespace AmazonChess;
using namespace Gdiplus;

#pragma comment(lib, "gdiplus.lib")

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInst;                                // 当前实例
static HWND g_hMainWnd = nullptr;               // 新增：主窗口句柄，InitInstance 中设置
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

// GDI+ token
static ULONG_PTR g_gdiplusToken = 0;

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

static bool g_isReplaying = false; // 当从记谱文件重放时设为 true，避免触发 AI

//
// GDI+ 初始化/清理
//
static void GdiPlusInit()
{
    if (!g_gdiplusToken)
    {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);
    }
}

static void GdiPlusShutdown()
{
    if (g_gdiplusToken)
    {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 初始化 GDI+
    GdiPlusInit();

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_AMAZONCHESS, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        GdiPlusShutdown();
        return FALSE;
    }

    // 载入资源并重置游戏
    GetGlobalGame().LoadResources(hInstance);
    GetGlobalGame().Reset();

    // 启动时立即执行一次 New：若存在 Initialization.acp 则作为初始局面载入（若失败则保持默认 Reset）
    {
        // 尝试静默载入项目目录下的 Initialization.acp
        std::wstring initFile = L"Initialization.acp";
        // 使用 LoadFromFile，会在内部将 g_isReplaying 设置/清除
        GetGlobalGame().LoadFromFile(initFile);
        // 强制首次重绘，确保界面更新（若窗口已创建）
        if (g_hMainWnd) InvalidateRect(g_hMainWnd, NULL, TRUE);
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_AMAZONCHESS));

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // 退出前清理
    GdiPlusShutdown();

    return (int) msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_AMAZONCHESS));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_AMAZONCHESS);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 800, 640, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   g_hMainWnd = hWnd; // 保存主窗口句柄

   return TRUE;
}

//
//  Game 类实现（在此文件中）
//

namespace AmazonChess
{
    // UIResources::Release
    void UIResources::Release()
    {
        if (hBoardBitmap) { DeleteObject(hBoardBitmap); hBoardBitmap = nullptr; }
        if (hWhiteAmazon) { DeleteObject(hWhiteAmazon); hWhiteAmazon = nullptr; }
        if (hBlackAmazon) { DeleteObject(hBlackAmazon); hBlackAmazon = nullptr; }
        if (hArrow) { DeleteObject(hArrow); hArrow = nullptr; }
    }

    // 单例实现
    static std::unique_ptr<Game> g_gameInstance;

    Game& GetGlobalGame()
    {
        if (!g_gameInstance) g_gameInstance.reset(new Game());
        return *g_gameInstance;
    }

    // 辅助：把 Pos 格式化为 "x,y"
    static std::wstring PosToString(const Pos& p)
    {
        std::wostringstream ss;
        ss << p.x << L"," << p.y;
        return ss.str();
    }

    // 辅助：从 "x,y" 解析 Pos
    static bool StringToPos(const std::wstring& s, Pos& out)
    {
        size_t comma = s.find(L',');
        if (comma == std::wstring::npos) return false;
        std::wstring sx = s.substr(0, comma);
        std::wstring sy = s.substr(comma + 1);
        try {
            int x = std::stoi(sx);
            int y = std::stoi(sy);
            out = Pos(x, y);
            return out.IsValid();
        } catch (...) {
            return false;
        }
    }

    // Game 方法
    Game::Game()
    {
        boardRect = { 0,0,0,0 };
        currentPlayer = Player::White;
        phase = TurnPhase::SelectAmazon;
        selected = Pos(-1,-1);
        lastMoveFrom = Pos(-1,-1);
        lastMoveTo = Pos(-1,-1);
        ClearHighlights();
        // 初始化 board
        for (int y = 0; y < BOARD_SIZE; ++y)
            for (int x = 0; x < BOARD_SIZE; ++x)
                board[y][x] = Cell();
    }

    Game::~Game()
    {
        resources.Release();
    }

    void Game::Reset()
    {
        // 清空
        for (int y = 0; y < BOARD_SIZE; ++y)
            for (int x = 0; x < BOARD_SIZE; ++x)
                board[y][x].type = PieceType::None;

        // 白 Amazon 初始位置: (0,2),(2,0),(5,0),(7,2)
        AddPiece(PieceType::WhiteAmazon, Pos(0,2));
        AddPiece(PieceType::WhiteAmazon, Pos(2,0));
        AddPiece(PieceType::WhiteAmazon, Pos(5,0));
        AddPiece(PieceType::WhiteAmazon, Pos(7,2));

        // 黑 Amazon 初始位置: (0,5),(2,7),(5,7),(7,5)
        AddPiece(PieceType::BlackAmazon, Pos(0,5));
        AddPiece(PieceType::BlackAmazon, Pos(2,7));
        AddPiece(PieceType::BlackAmazon, Pos(5,7));
        AddPiece(PieceType::BlackAmazon, Pos(7,5));

        currentPlayer = Player::White;
        phase = TurnPhase::SelectAmazon;
        selected = Pos(-1,-1);
        lastMoveFrom = Pos(-1,-1);
        lastMoveTo = Pos(-1,-1);
        ClearHighlights();
        moves.clear();
    }

    void Game::LoadResources(HINSTANCE /*hInst*/)
    {
        // 当前实现使用 GDI+ 绘制简单形状，因此不强制加载位图。
        // 占位：若日后有资源 ID，可用 GDI+ Bitmap::FromResource 或从文件加载。
        resources.Release();
    }

    Pos Game::PixelToCell(POINT pt) const
    {
        if (boardRect.right <= boardRect.left || boardRect.bottom <= boardRect.top)
            return Pos(-1,-1);

        int boardWidth = boardRect.right - boardRect.left;
        int boardHeight = boardRect.bottom - boardRect.top;
        int cellSize = std::min(boardWidth, boardHeight) / BOARD_SIZE;
        if (cellSize <= 0) return Pos(-1,-1);

        int left = boardRect.left;
        int top = boardRect.top;
        int right = left + cellSize * BOARD_SIZE;
        int bottom = top + cellSize * BOARD_SIZE;

        if (pt.x < left || pt.x >= right || pt.y < top || pt.y >= bottom) return Pos(-1,-1);

        int cx = (pt.x - left) / cellSize;
        // 注意棋盘坐标以左下为 (0,0)，而屏幕 y 向下为正
        int cy = (bottom - 1 - pt.y) / cellSize; // bottom-1 确保边界映射正确

        if (cx < 0 || cx >= BOARD_SIZE || cy < 0 || cy >= BOARD_SIZE) return Pos(-1,-1);
        return Pos(cx, cy);
    }

    RECT Game::CellToRect(const Pos& p) const
    {
        RECT rc = {0,0,0,0};
        if (!p.IsValid()) return rc;
        if (boardRect.right <= boardRect.left || boardRect.bottom <= boardRect.top) return rc;

        int boardWidth = boardRect.right - boardRect.left;
        int boardHeight = boardRect.bottom - boardRect.top;
        int cellSize = std::min(boardWidth, boardHeight) / BOARD_SIZE;
        if (cellSize <= 0) return rc;

        int left = boardRect.left;
        int top = boardRect.top;
        int bottom = top + cellSize * BOARD_SIZE;

        int x = left + p.x * cellSize;
        int y = bottom - (p.y + 1) * cellSize;

        rc.left = x;
        rc.top = y;
        rc.right = x + cellSize;
        rc.bottom = y + cellSize;
        return rc;
    }

    void Game::SetBoardRect(const RECT& rcBoard)
    {
        boardRect = rcBoard;
    }

    bool Game::OnLButtonDown(HWND /*hWnd*/, int x, int y)
    {
        POINT pt = { x, y };
        Pos cell = PixelToCell(pt);
        if (!cell.IsValid()) return false;

        if (phase == TurnPhase::SelectAmazon)
        {
            PieceType p = GetPieceAt(cell);
            if ((currentPlayer == Player::White && p == PieceType::WhiteAmazon) ||
                (currentPlayer == Player::Black && p == PieceType::BlackAmazon))
            {
                selected = cell;
                highlighted = GetReachableFrom(cell);
                phase = TurnPhase::MoveAmazon;
                return true;
            }
            // 点击其他格，忽略
            return false;
        }
        else if (phase == TurnPhase::MoveAmazon)
        {
            // 如果点击的是己方另一个 Amazon 则切换选择
            PieceType clicked = GetPieceAt(cell);
            if ((currentPlayer == Player::White && clicked == PieceType::WhiteAmazon) ||
                (currentPlayer == Player::Black && clicked == PieceType::BlackAmazon))
            {
                selected = cell;
                highlighted = GetReachableFrom(cell);
                return true;
            }

            // 如果点击的是可达格则移动
            auto it = std::find_if(highlighted.begin(), highlighted.end(),
                [&cell](const Pos& p){ return p == cell; });
            if (it != highlighted.end())
            {
                Pos from = selected;
                if (MoveAmazon(from, cell))
                {
                    // MoveAmazon will set phase to ShootArrow and selected to new pos
                    highlighted = GetReachableFrom(cell);
                    return true;
                }
            }

            return false;
        }
        else if (phase == TurnPhase::ShootArrow)
        {
            // 发箭：必须在 highlighted 内，且目标为空
            auto it = std::find_if(highlighted.begin(), highlighted.end(),
                [&cell](const Pos& p){ return p == cell; });
            if (it != highlighted.end() && IsCellEmpty(cell))
            {
                if (ShootArrow(cell))
                {
                    // 回合已切换，清理选择
                    selected = Pos(-1,-1);
                    highlighted.clear();
                    return true;
                }
            }
            // 点击己方 Amazon 可以重新选择并进入 MoveAmazon
            PieceType clicked = GetPieceAt(cell);
            if ((currentPlayer == Player::White && clicked == PieceType::WhiteAmazon) ||
                (currentPlayer == Player::Black && clicked == PieceType::BlackAmazon))
            {
                selected = cell;
                highlighted = GetReachableFrom(cell);
                phase = TurnPhase::MoveAmazon;
                return true;
            }

            return false;
        }
        return false;
    }

    bool Game::OnMouseMove(HWND /*hWnd*/, int /*x*/, int /*y*/)
    {
        // 当前实现不需要响应鼠标移动
        return false;
    }

    bool Game::OnLButtonUp(HWND /*hWnd*/, int /*x*/, int /*y*/)
    {
        return false;
    }

    void Game::OnPaint(HDC hdc, const RECT& clientRect)
    {
        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeHighQuality);

        // 背景
        SolidBrush back(Color(255, 240, 240, 240));
        RectF clientRectF(
            static_cast<REAL>(clientRect.left),
            static_cast<REAL>(clientRect.top),
            static_cast<REAL>(clientRect.right - clientRect.left),
            static_cast<REAL>(clientRect.bottom - clientRect.top)
        );
        g.FillRectangle(&back, clientRectF);

        // 计算棋盘绘制矩形：居中，使用最小边长的正方形
        int clientW = clientRect.right - clientRect.left;
        int clientH = clientRect.bottom - clientRect.top;
        int boardSizePx = std::min(clientW, clientH) - 20; // 留边距
        if (boardSizePx < 8) boardSizePx = std::min(clientW, clientH);
        int left = clientRect.left + (clientW - boardSizePx) / 2;
        int top = clientRect.top + (clientH - boardSizePx) / 2;
        RECT rcBoard = { left, top, left + boardSizePx, top + boardSizePx };
        SetBoardRect(rcBoard);

        // 单元格尺寸
        int cellSize = boardSizePx / BOARD_SIZE;
        // 重新对齐右/bottom 以避免舍入问题
        rcBoard.right = rcBoard.left + cellSize * BOARD_SIZE;
        rcBoard.bottom = rcBoard.top + cellSize * BOARD_SIZE;
        SetBoardRect(rcBoard);

        // 绘制格子（注意交替颜色）
        for (int bx = 0; bx < BOARD_SIZE; ++bx)
        {
            for (int by = 0; by < BOARD_SIZE; ++by)
            {
                int screenX = rcBoard.left + bx * cellSize;
                int screenY = rcBoard.top + (BOARD_SIZE - 1 - by) * cellSize;
                bool dark = ((bx + by) % 2) == 1;
                Color col = dark ? Color(255, 181, 136, 99) : Color(255, 244, 214, 159);
                SolidBrush brush(col);
                RectF cellRectF(
                    static_cast<REAL>(screenX),
                    static_cast<REAL>(screenY),
                    static_cast<REAL>(cellSize),
                    static_cast<REAL>(cellSize)
                );
                g.FillRectangle(&brush, cellRectF);
            }
        }

        // 绘制高亮可达格
        SolidBrush greenHighlight(Color(120, 30, 200, 30)); // 半透明绿
        for (const Pos& p : highlighted)
        {
            RECT rc = CellToRect(p);
            RectF hf(
                static_cast<REAL>(rc.left),
                static_cast<REAL>(rc.top),
                static_cast<REAL>(rc.right - rc.left),
                static_cast<REAL>(rc.bottom - rc.top)
            );
            g.FillRectangle(&greenHighlight, hf);
        }

        // 绘制选中 Amazon 边框
        if (selected.IsValid())
        {
            RECT rc = CellToRect(selected);
            Pen selPen(Color(200, 220, 180, 10 + 220), 3.0f);
            RectF selRect(
                static_cast<REAL>(rc.left + 2),
                static_cast<REAL>(rc.top + 2),
                static_cast<REAL>(rc.right - rc.left - 4),
                static_cast<REAL>(rc.bottom - rc.top - 4)
            );
            g.DrawRectangle(&selPen, selRect);
        }

        // 绘制棋子与箭，使用圆点或菱形表示
        for (int y = 0; y < BOARD_SIZE; ++y)
        {
            for (int x = 0; x < BOARD_SIZE; ++x)
            {
                Pos p(x, y);
                PieceType pt = board[y][x].type;
                if (pt == PieceType::None) continue;
                RECT rc = CellToRect(p);
                float cx = (rc.left + rc.right) / 2.0f;
                float cy = (rc.top + rc.bottom) / 2.0f;
                float radius = (rc.right - rc.left) * 0.35f;

                if (pt == PieceType::WhiteAmazon)
                {
                    SolidBrush b(Color(255, 250, 250, 250));
                    Pen pen(Color(255, 60, 60, 60), 2.0f);
                    g.FillEllipse(&b, cx - radius, cy - radius, radius * 2, radius * 2);
                    g.DrawEllipse(&pen, cx - radius, cy - radius, radius * 2, radius * 2);
                }
                else if (pt == PieceType::BlackAmazon)
                {
                    SolidBrush b(Color(255, 40, 40, 40));
                    Pen pen(Color(255, 220, 220, 220), 2.0f);
                    g.FillEllipse(&b, cx - radius, cy - radius, radius * 2, radius * 2);
                    g.DrawEllipse(&pen, cx - radius, cy - radius, radius * 2, radius * 2);
                }
                else if (pt == PieceType::Arrow)
                {
                    // 放大箭（菱形）并加粗描边，增加中心点以提高可见性
                    const float arrowScale = 0.85f; // 由原来的 ~0.6 放大到 0.85
                    PointF pts[4];
                    pts[0].X = cx;                      pts[0].Y = cy - radius * arrowScale;
                    pts[1].X = cx + radius * arrowScale; pts[1].Y = cy;
                    pts[2].X = cx;                      pts[2].Y = cy + radius * arrowScale;
                    pts[3].X = cx - radius * arrowScale; pts[3].Y = cy;

                    SolidBrush b(Color(255, 180, 110, 60)); // 更醒目的填充色
                    g.FillPolygon(&b, pts, 4);

                    Pen pen(Color(255, 120, 80, 40), 2.0f); // 更粗的边框
                    g.DrawPolygon(&pen, pts, 4);
                }
            }
        }

        // 若一方被封死，则在中心显示提示
        Player winner = GetWinner();
        if (winner != Player::None)
        {
            std::wstring msg;
            if (winner == Player::White) msg = L"白方胜利";
            else msg = L"黑方胜利";
            FontFamily fontFamily(L"Segoe UI");
            Font font(&fontFamily, 28, FontStyleBold, UnitPixel);
            SolidBrush txtBrush(Color(200, 255, 255, 255));
            SolidBrush shadow(Color(160, 0, 0, 0));
            RectF layout(
                static_cast<REAL>(rcBoard.left),
                static_cast<REAL>(rcBoard.top + boardSizePx / 2 - 20),
                static_cast<REAL>(boardSizePx),
                40.0f
            );
            g.DrawString(msg.c_str(), -1, &font, PointF(layout.X + 2, layout.Y + 2), &shadow);
            g.DrawString(msg.c_str(), -1, &font, PointF(layout.X, layout.Y), &txtBrush);
        }
    }

    PieceType Game::GetPieceAt(const Pos& p) const
    {
        if (!IsWithinBoard(p)) return PieceType::None;
        return board[p.y][p.x].type;
    }

    bool Game::AddPiece(PieceType type, const Pos& p)
    {
        if (!IsWithinBoard(p)) return false;
        if (!IsCellEmpty(p)) return false;
        board[p.y][p.x].type = type;
        return true;
    }

    bool Game::RemovePiece(const Pos& p)
    {
        if (!IsWithinBoard(p)) return false;
        board[p.y][p.x].type = PieceType::None;
        return true;
    }

    std::vector<Pos> Game::GetReachableFrom(const Pos& from) const
    {
        std::vector<Pos> result;
        if (!IsWithinBoard(from)) return result;
        // 八个方向
        static const int dirs[8][2] = {
            {1,0},{-1,0},{0,1},{0,-1},
            {1,1},{1,-1},{-1,1},{-1,-1}
        };
        for (int i = 0; i < 8; ++i)
        {
            int dx = dirs[i][0];
            int dy = dirs[i][1];
            int x = from.x + dx;
            int y = from.y + dy;
            while (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE)
            {
                if (!IsCellEmpty(Pos(x,y)))
                {
                    break;
                }
                result.emplace_back(x, y);
                x += dx; y += dy;
            }
        }
        return result;
    }

    void Game::RecordMove(Player player, const Pos& from, const Pos& to, const Pos& arrow, bool gameEnd)
    {
        std::wostringstream ss;
        ss << (player == Player::White ? L"W" : L"B") << L" "
           << PosToString(from) << L" "
           << PosToString(to) << L" "
           << PosToString(arrow);
        if (gameEnd) ss << L"*";
        moves.push_back(ss.str());
    }

    bool Game::MoveAmazon(const Pos& from, const Pos& to)
    {
        if (!IsWithinBoard(from) || !IsWithinBoard(to)) return false;
        PieceType src = GetPieceAt(from);
        if (!((currentPlayer == Player::White && src == PieceType::WhiteAmazon) ||
              (currentPlayer == Player::Black && src == PieceType::BlackAmazon))) return false;
        if (!IsCellEmpty(to)) return false;
        auto reachable = GetReachableFrom(from);
        auto it = std::find_if(reachable.begin(), reachable.end(), [&to](const Pos& p){ return p == to; });
        if (it == reachable.end()) return false;

        board[to.y][to.x].type = src;
        board[from.y][from.x].type = PieceType::None;

        lastMoveFrom = from;
        lastMoveTo = to;

        selected = to;
        phase = TurnPhase::ShootArrow;
        highlighted = GetReachableFrom(selected);
        return true;
    }

    bool Game::ShootArrow(const Pos& target)
    {
        if (!IsWithinBoard(target)) return false;
        if (!IsCellEmpty(target)) return false;
        // target 必须在 highlighted 中（调用处保证）
        board[target.y][target.x].type = PieceType::Arrow;

        // 在切换玩家前记录本手：使用 currentPlayer（当前执行此发箭动作的玩家）
        bool gameEnd = (GetWinner() != Player::None); // 部署箭后可能导致对方被封死
        RecordMove(currentPlayer, lastMoveFrom, lastMoveTo, target, gameEnd);

        // 回合结束，切换玩家
        ToggleNextPlayer();
        phase = TurnPhase::SelectAmazon;
        selected = Pos(-1,-1);
        highlighted.clear();

        // 如果现在是黑方回合且不是在重放，从 AI 取得落子并执行
        if (!g_isReplaying && currentPlayer == Player::Black)
        {
            // 构造 PieceType 数组供 AI 使用
            std::array<std::array<PieceType, BOARD_SIZE>, BOARD_SIZE> pboard;
            for (int yy = 0; yy < BOARD_SIZE; ++yy)
                for (int xx = 0; xx < BOARD_SIZE; ++xx)
                    pboard[yy][xx] = board[yy][xx].type;

            auto best = GetBestMove(pboard, currentPlayer);
            if (best.first != -1)
            {
                const int squareCount = BOARD_SIZE * BOARD_SIZE;
                int movePacked = best.first;
                int fromIndex = movePacked / squareCount;
                int toIndex = movePacked % squareCount;
                Pos fromPos(fromIndex % BOARD_SIZE, fromIndex / BOARD_SIZE);
                Pos toPos(toIndex % BOARD_SIZE, toIndex / BOARD_SIZE);

                // 执行 AI 的移动 - MoveAmazon 会设 lastMoveFrom/To
                if (MoveAmazon(fromPos, toPos))
                {
                    // 若 AI 返回了箭位置则放箭；否则尝试选择第一个可达格作为箭（防防万一）
                    if (best.second >= 0)
                    {
                        int arrowIdx = best.second;
                        Pos arrowPos(arrowIdx % BOARD_SIZE, arrowIdx / BOARD_SIZE);
                        // 直接调用 ShootArrow —— 这将记录该手并切换回人类
                        ShootArrow(arrowPos);
                    }
                    else
                    {
                        // 如果没有箭位置（极少见），选择第一个 highlighted 作为箭
                        if (!highlighted.empty())
                        {
                            ShootArrow(highlighted.front());
                        }
                        else
                        {
                            // 没有合法箭位，直接切换回玩家（虽然规则上应不会发生）
                            ToggleNextPlayer();
                        }
                    }
                }
            }
        }

        return true;
    }

    bool Game::IsPlayerTrapped(Player player) const
    {
        PieceType amazonType = (player == Player::White) ? PieceType::WhiteAmazon : PieceType::BlackAmazon;
        for (int y = 0; y < BOARD_SIZE; ++y)
        {
            for (int x = 0; x < BOARD_SIZE; ++x)
            {
                if (board[y][x].type == amazonType)
                {
                    Pos p(x,y);
                    auto reach = const_cast<Game*>(this)->GetReachableFrom(p);
                    if (!reach.empty()) return false;
                }
            }
        }
        return true;
    }

    Player Game::GetWinner() const
    {
        if (IsPlayerTrapped(Player::White)) return Player::Black;
        if (IsPlayerTrapped(Player::Black)) return Player::White;
        return Player::None;
    }

    bool Game::IsCellEmpty(const Pos& p) const
    {
        if (!IsWithinBoard(p)) return false;
        return board[p.y][p.x].type == PieceType::None;
    }

    bool Game::IsWithinBoard(const Pos& p) const
    {
        return p.x >= 0 && p.x < BOARD_SIZE && p.y >= 0 && p.y < BOARD_SIZE;
    }

    void Game::ClearHighlights()
    {
        highlighted.clear();
    }

    void Game::ToggleNextPlayer()
    {
        if (currentPlayer == Player::White) currentPlayer = Player::Black;
        else currentPlayer = Player::White;
    }

    // 保存记谱到指定文件（UTF-8）
    bool Game::SaveToFile(const std::wstring& path) const
    {
        std::wofstream ofs(path, std::ios::binary);
        if (!ofs.is_open()) return false;
        ofs.imbue(std::locale(ofs.getloc(), new std::codecvt_utf8<wchar_t>));
        for (const auto& line : moves)
        {
            ofs << line << L"\n";
        }
        ofs.close();
        return true;
    }

    // 从文件读取并重放（从初始局面开始）
    bool Game::LoadFromFile(const std::wstring& path)
    {
        std::wifstream ifs(path, std::ios::binary);
        if (!ifs.is_open()) return false;
        ifs.imbue(std::locale(ifs.getloc(), new std::codecvt_utf8<wchar_t>));

        // 标记为重放，防止在重放期触发 AI
        g_isReplaying = true;

        // 从初始局面开始重放
        Reset();
        moves.clear();

        std::wstring line;
        bool success = true;
        while (std::getline(ifs, line))
        {
            // trim CRLF and spaces
            while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n' || line.back() == L' ' || line.back() == L'\t')) line.pop_back();
            if (line.empty()) continue;

            bool hadStar = false;
            if (!line.empty() && line.back() == L'*') { hadStar = true; line.pop_back(); }

            std::wistringstream iss(line);
            std::wstring playerToken, fromStr, toStr, arrowStr;
            if (!(iss >> playerToken >> fromStr >> toStr >> arrowStr)) { success = false; break; }

            Player p = (playerToken == L"W") ? Player::White : Player::Black;

            Pos from, to, arrow;
            if (!StringToPos(fromStr, from) || !StringToPos(toStr, to) || !StringToPos(arrowStr, arrow)) { success = false; break; }

            // 为了让 MoveAmazon 校验正常，设置 currentPlayer 成为该行的玩家
            currentPlayer = p;

            if (!MoveAmazon(from, to)) { success = false; break; }
            if (!ShootArrow(arrow)) { success = false; break; }

            if (hadStar) break;
        }

        g_isReplaying = false;
        return success;
    }

    const std::vector<std::wstring>& Game::GetMoveList() const { return moves; }
    void Game::ClearMoveList() { moves.clear(); }
} // namespace AmazonChess

//
//  窗口过程：将鼠标与绘制消息转发到 Game，以及处理菜单命令（包括保存/载入）
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 分析菜单选择:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case IDM_SAVE:
                {
                    WCHAR szFile[MAX_PATH] = {0};
                    OPENFILENAMEW ofn = {};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFilter = L"AmazonChess 棋谱 (*.acp)\0*.acp\0All Files\0*.*\0";
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = _countof(szFile);
                    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
                    ofn.lpstrDefExt = L"acp";
                    if (GetSaveFileNameW(&ofn))
                    {
                        if (!GetGlobalGame().SaveToFile(szFile))
                        {
                            MessageBoxW(hWnd, L"保存失败", L"错误", MB_ICONERROR);
                        }
                    }
                }
                break;
            case IDM_LOAD:
                {
                    WCHAR szFile[MAX_PATH] = {0};
                    OPENFILENAMEW ofn = {};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFilter = L"AmazonChess 棋谱 (*.acp)\0*.acp\0All Files\0*.*\0";
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = _countof(szFile);
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (GetOpenFileNameW(&ofn))
                    {
                        if (!GetGlobalGame().LoadFromFile(szFile))
                        {
                            MessageBoxW(hWnd, L"载入棋谱失败：格式或内容不正确。", L"加载失败", MB_ICONERROR);
                        }
                        // 不擦除背景，减少闪烁
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                }
                break;
            case IDM_NEW:
                {
                    GetGlobalGame().Reset();
                    // 尝试加载 Initialization.acp（若不存在则保持 Reset）
                    GetGlobalGame().LoadFromFile(std::wstring(L"Initialization.acp"));
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

    case WM_ERASEBKGND:
        // 阻止默认背景擦除以减少闪烁（由 OnPaint 完整绘制）
        return 1;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            GetGlobalGame().OnPaint(hdc, clientRect);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (GetGlobalGame().OnLButtonDown(hWnd, x, y))
            {
                // 不擦除背景，直接重绘
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        break;
    case WM_MOUSEMOVE:
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (GetGlobalGame().OnMouseMove(hWnd, x, y))
            {
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        break;
    case WM_LBUTTONUP:
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (GetGlobalGame().OnLButtonUp(hWnd, x, y))
            {
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)   
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}