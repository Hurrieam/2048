#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <random>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <cmath>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// 编译时间宏
#define COMPILE_TIME __DATE__ " " __TIME__

// 游戏常量
const int BOARD_SIZE = 4;
const int TILE_SIZE = 80;
const int BOARD_MARGIN = 10;
const int WINDOW_WIDTH = 500;
const int WINDOW_HEIGHT = 500;
const uint32_t SAVE_FILE_VERSION = 1;
const char SAVE_FILE_HEADER[9] = "2048SAVE";

// 颜色定义
COLORREF TILE_COLORS[] = {
    RGB(205, 193, 180), // 0
    RGB(238, 228, 218), // 2
    RGB(237, 224, 200), // 4
    RGB(242, 177, 121), // 8
    RGB(245, 149, 99),  // 16
    RGB(246, 124, 95),  // 32
    RGB(246, 94, 59),   // 64
    RGB(237, 207, 114), // 128
    RGB(237, 204, 97),  // 256
    RGB(237, 200, 80),  // 512
    RGB(237, 197, 63),  // 1024
    RGB(237, 194, 46)   // 2048
};

COLORREF TEXT_COLORS[] = {
    RGB(119, 110, 101),
    RGB(249, 246, 242)
};

// RAII资源管理类
class GDIBrush {
private:
    HBRUSH hBrush;
public:
    explicit GDIBrush(COLORREF color) : hBrush(CreateSolidBrush(color)) {
        if (!hBrush) {
            throw std::runtime_error("Failed to create GDI brush");
        }
    }

    ~GDIBrush() {
        if (hBrush) {
            DeleteObject(hBrush);
        }
    }

    GDIBrush(GDIBrush&& other) noexcept : hBrush(other.hBrush) {
        other.hBrush = nullptr;
    }

    GDIBrush& operator=(GDIBrush&& other) noexcept {
        if (this != &other) {
            if (hBrush) DeleteObject(hBrush);
            hBrush = other.hBrush;
            other.hBrush = nullptr;
        }
        return *this;
    }

    GDIBrush(const GDIBrush&) = delete;
    GDIBrush& operator=(const GDIBrush&) = delete;

    operator HBRUSH() const { return hBrush; }
    HBRUSH Get() const { return hBrush; }
};

class GDIPen {
private:
    HPEN hPen;
public:
    explicit GDIPen(int style, int width, COLORREF color) : hPen(CreatePen(style, width, color)) {
        if (!hPen) {
            throw std::runtime_error("Failed to create GDI pen");
        }
    }

    ~GDIPen() {
        if (hPen) {
            DeleteObject(hPen);
        }
    }

    GDIPen(GDIPen&& other) noexcept : hPen(other.hPen) {
        other.hPen = nullptr;
    }

    GDIPen& operator=(GDIPen&& other) noexcept {
        if (this != &other) {
            if (hPen) DeleteObject(hPen);
            hPen = other.hPen;
            other.hPen = nullptr;
        }
        return *this;
    }

    GDIPen(const GDIPen&) = delete;
    GDIPen& operator=(const GDIPen&) = delete;

    operator HPEN() const { return hPen; }
    HPEN Get() const { return hPen; }
};

class GDIFont {
private:
    HFONT hFont;
public:
    explicit GDIFont(int height, int width = 0, DWORD charset = DEFAULT_CHARSET,
        DWORD pitchAndFamily = DEFAULT_PITCH, LPCWSTR face = L"Arial")
        : hFont(CreateFont(height, width, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            charset, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, pitchAndFamily, face)) {
        if (!hFont) {
            throw std::runtime_error("Failed to create GDI font");
        }
    }

    ~GDIFont() {
        if (hFont) {
            DeleteObject(hFont);
        }
    }

    GDIFont(GDIFont&& other) noexcept : hFont(other.hFont) {
        other.hFont = nullptr;
    }

    GDIFont& operator=(GDIFont&& other) noexcept {
        if (this != &other) {
            if (hFont) DeleteObject(hFont);
            hFont = other.hFont;
            other.hFont = nullptr;
        }
        return *this;
    }

    GDIFont(const GDIFont&) = delete;
    GDIFont& operator=(const GDIFont&) = delete;

    operator HFONT() const { return hFont; }
    HFONT Get() const { return hFont; }
};

// 游戏状态结构
#pragma pack(push, 1)
struct GameState {
    int board[BOARD_SIZE][BOARD_SIZE];
    int score;
    bool gameOver;
    bool won;
    uint32_t checksum;
};
#pragma pack(pop)

class Game2048 {
private:
    GameState state;
    HWND hwnd;
    std::unique_ptr<GDIFont> hMainFont;
    bool keyboardEnabled;
    bool keyProcessed; // 跟踪当前按键是否已处理

public:
    Game2048() : hwnd(nullptr), keyboardEnabled(true), keyProcessed(false) {
        ResetState();
    }

    void Initialize(HWND window) {
        hwnd = window;
        try {
            hMainFont = std::make_unique<GDIFont>(24);
            NewGame();
        }
        catch (const std::exception&) {
            throw;
        }
    }

    void ResetState() {
        memset(&state, 0, sizeof(state));
        state.gameOver = false;
        state.won = false;
        state.score = 0;
        state.checksum = 0;
        keyProcessed = false;
    }

    void NewGame() {
        ResetState();
        keyboardEnabled = true;

        try {
            AddRandomTile();
            AddRandomTile();
        }
        catch (const std::exception&) {
            throw;
        }

        InvalidateRect(hwnd, NULL, TRUE);
    }

    void EnableKeyboard() {
        keyboardEnabled = true;
        SetFocus(hwnd);
    }

    bool IsValidTileValue(int value) const {
        if (value == 0) return true;
        if (value < 0) return false;
        return (value & (value - 1)) == 0;
    }

    bool ValidateGameState() const {
        if (state.score < 0) {
            return false;
        }

        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (!IsValidTileValue(state.board[i][j])) {
                    return false;
                }
            }
        }

        return true;
    }

    uint32_t CalculateChecksum() const {
        uint32_t checksum = 0;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&state);
        size_t dataSize = sizeof(state) - sizeof(state.checksum);

        for (size_t i = 0; i < dataSize; ++i) {
            checksum = (checksum << 5) + checksum + data[i];
        }

        return checksum;
    }

    void AddRandomTile() {
        std::vector<std::pair<int, int>> emptyCells;

        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (state.board[i][j] == 0) {
                    emptyCells.push_back(std::make_pair(i, j));
                }
            }
        }

        if (emptyCells.empty()) {
            throw std::runtime_error("No empty cells available for new tile");
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, static_cast<int>(emptyCells.size()) - 1);
        int index = dis(gen);
        int row = emptyCells[index].first;
        int col = emptyCells[index].second;

        std::uniform_real_distribution<> prob(0.0, 1.0);
        state.board[row][col] = (prob(gen) < 0.9) ? 2 : 4;

        if (!ValidateGameState()) {
            throw std::runtime_error("Game state invalid after adding random tile");
        }
    }

    bool MoveLeft() {
        bool moved = false;

        for (int i = 0; i < BOARD_SIZE; i++) {
            int writePos = 0;
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (state.board[i][j] != 0) {
                    if (j != writePos) moved = true;
                    state.board[i][writePos++] = state.board[i][j];
                }
            }
            while (writePos < BOARD_SIZE) {
                state.board[i][writePos++] = 0;
            }

            for (int j = 0; j < BOARD_SIZE - 1; j++) {
                if (state.board[i][j] != 0 && state.board[i][j] == state.board[i][j + 1]) {
                    state.board[i][j] *= 2;
                    state.score += state.board[i][j];
                    state.board[i][j + 1] = 0;

                    if (state.board[i][j] == 2048 && !state.won) {
                        state.won = true;
                    }

                    moved = true;

                    for (int k = j + 1; k < BOARD_SIZE - 1; k++) {
                        state.board[i][k] = state.board[i][k + 1];
                    }
                    state.board[i][BOARD_SIZE - 1] = 0;
                }
            }
        }

        return moved;
    }

    bool MoveRight() {
        ReverseRows();
        bool moved = MoveLeft();
        ReverseRows();
        return moved;
    }

    bool MoveUp() {
        Transpose();
        bool moved = MoveLeft();
        Transpose();
        return moved;
    }

    bool MoveDown() {
        Transpose();
        ReverseRows();
        bool moved = MoveLeft();
        ReverseRows();
        Transpose();
        return moved;
    }

    void ReverseRows() {
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE / 2; j++) {
                std::swap(state.board[i][j], state.board[i][BOARD_SIZE - 1 - j]);
            }
        }
    }

    void Transpose() {
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = i + 1; j < BOARD_SIZE; j++) {
                std::swap(state.board[i][j], state.board[j][i]);
            }
        }
    }

    bool CanMove() {
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (state.board[i][j] == 0) {
                    return true;
                }
            }
        }

        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if ((j < BOARD_SIZE - 1 && state.board[i][j] == state.board[i][j + 1]) ||
                    (i < BOARD_SIZE - 1 && state.board[i][j] == state.board[i + 1][j])) {
                    return true;
                }
            }
        }

        return false;
    }

    void CheckGameOver() {
        if (!CanMove()) {
            state.gameOver = true;
        }
    }

    void Draw(HDC hdc) {
        try {
            RECT clientRect;
            if (!GetClientRect(hwnd, &clientRect)) {
                return;
            }

            GDIBrush bgBrush(RGB(187, 173, 160));
            FillRect(hdc, &clientRect, bgBrush);

            HFONT hOldFont = (HFONT)SelectObject(hdc, *hMainFont);
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkMode(hdc, TRANSPARENT);

            std::wstring scoreText = L"分数: " + std::to_wstring(state.score);
            RECT scoreRect = { 10, 10, 200, 50 };
            DrawText(hdc, scoreText.c_str(), -1, &scoreRect, DT_LEFT | DT_VCENTER);

            int boardX = (clientRect.right - (BOARD_SIZE * TILE_SIZE + (BOARD_SIZE + 1) * BOARD_MARGIN)) / 2;
            int boardY = 60;

            for (int i = 0; i < BOARD_SIZE; i++) {
                for (int j = 0; j < BOARD_SIZE; j++) {
                    int x = boardX + j * (TILE_SIZE + BOARD_MARGIN);
                    int y = boardY + i * (TILE_SIZE + BOARD_MARGIN);
                    DrawTile(hdc, x, y, state.board[i][j]);
                }
            }

            if (state.gameOver) {
                DrawGameOver(hdc, clientRect);
            }
            else if (state.won) {
                DrawWinMessage(hdc, clientRect);
            }

            SelectObject(hdc, hOldFont);
        }
        catch (const std::exception&) {
            TextOut(hdc, 10, 10, L"绘制错误", 4);
        }
    }

    void DrawTile(HDC hdc, int x, int y, int value) {
        try {
            int colorIndex = 0;
            if (value > 0) {
                colorIndex = static_cast<int>(log(value) / log(2));
                if (colorIndex < 0) colorIndex = 0;
                if (colorIndex >= sizeof(TILE_COLORS) / sizeof(COLORREF)) {
                    colorIndex = sizeof(TILE_COLORS) / sizeof(COLORREF) - 1;
                }
            }

            GDIBrush tileBrush(TILE_COLORS[colorIndex]);
            RECT tileRect = { x, y, x + TILE_SIZE, y + TILE_SIZE };
            FillRect(hdc, &tileRect, tileBrush);

            GDIPen borderPen(PS_SOLID, 2, RGB(187, 173, 160));
            HPEN hOldPen = (HPEN)SelectObject(hdc, borderPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, x, y, x + TILE_SIZE, y + TILE_SIZE);
            SelectObject(hdc, hOldBrush);
            SelectObject(hdc, hOldPen);

            if (value > 0) {
                COLORREF textColor = (value <= 4) ? TEXT_COLORS[0] : TEXT_COLORS[1];
                SetTextColor(hdc, textColor);

                std::wstring text = std::to_wstring(value);
                RECT textRect = { x, y, x + TILE_SIZE, y + TILE_SIZE };

                int fontSize = 32;
                if (value >= 1000) fontSize = 24;
                else if (value >= 100) fontSize = 28;

                GDIFont tileFont(fontSize);
                HFONT hOldFont = (HFONT)SelectObject(hdc, tileFont);
                DrawText(hdc, text.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, hOldFont);
            }
        }
        catch (const std::exception&) {
        }
    }

    void DrawGameOver(HDC hdc, RECT& clientRect) {
        try {
            GDIFont bigFont(36);
            HFONT hOldFont = (HFONT)SelectObject(hdc, bigFont);

            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkMode(hdc, TRANSPARENT);

            std::wstring message = L"游戏结束!";
            RECT messageRect = clientRect;
            messageRect.top = clientRect.bottom - 100;
            DrawText(hdc, message.c_str(), -1, &messageRect, DT_CENTER | DT_VCENTER);

            SelectObject(hdc, hOldFont);
        }
        catch (const std::exception&) {
        }
    }

    void DrawWinMessage(HDC hdc, RECT& clientRect) {
        try {
            GDIFont bigFont(36);
            HFONT hOldFont = (HFONT)SelectObject(hdc, bigFont);

            SetTextColor(hdc, RGB(255, 215, 0));
            SetBkMode(hdc, TRANSPARENT);

            std::wstring message = L"恭喜获胜!";
            RECT messageRect = clientRect;
            messageRect.top = clientRect.bottom - 100;
            DrawText(hdc, message.c_str(), -1, &messageRect, DT_CENTER | DT_VCENTER);

            SelectObject(hdc, hOldFont);
        }
        catch (const std::exception&) {
        }
    }

    std::wstring GetDefaultSaveFileName() {
        try {
            time_t now = time(nullptr);
            tm tm;
            if (localtime_s(&tm, &now) != 0) {
                return L"2048-backup.bin";
            }

            std::wstringstream ss;
            ss << L"2048-"
                << std::setw(4) << (tm.tm_year + 1900)
                << std::setw(2) << std::setfill(L'0') << (tm.tm_mon + 1)
                << std::setw(2) << std::setfill(L'0') << tm.tm_mday
                << std::setw(2) << std::setfill(L'0') << tm.tm_hour
                << std::setw(2) << std::setfill(L'0') << tm.tm_min
                << std::setw(2) << std::setfill(L'0') << tm.tm_sec
                << L".bin";

            return ss.str();
        }
        catch (const std::exception&) {
            return L"2048-backup.bin";
        }
    }

    bool SaveGameWithDialog() {
        try {
            if (!ValidateGameState()) {
                MessageBox(hwnd, L"游戏状态无效，无法保存", L"错误", MB_OK | MB_ICONERROR);
                return false;
            }

            wchar_t filename[MAX_PATH];
            wcscpy_s(filename, GetDefaultSaveFileName().c_str());

            OPENFILENAME ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"2048游戏文件 (*.bin)\0*.bin\0所有文件 (*.*)\0*.*\0";
            ofn.lpstrDefExt = L"bin";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

            if (GetSaveFileName(&ofn)) {
                std::ofstream file(filename, std::ios::binary);
                if (!file.is_open()) {
                    MessageBox(hwnd, L"无法创建文件", L"错误", MB_OK | MB_ICONERROR);
                    return false;
                }

                file.write(SAVE_FILE_HEADER, sizeof(SAVE_FILE_HEADER));
                file.write(reinterpret_cast<const char*>(&SAVE_FILE_VERSION), sizeof(SAVE_FILE_VERSION));

                state.checksum = CalculateChecksum();
                file.write(reinterpret_cast<const char*>(&state), sizeof(state));

                if (file.fail()) {
                    file.close();
                    _wremove(filename);
                    MessageBox(hwnd, L"文件写入失败", L"错误", MB_OK | MB_ICONERROR);
                    return false;
                }

                file.close();
                return true;
            }
            return false;
        }
        catch (const std::exception&) {
            MessageBox(hwnd, L"保存游戏时发生错误", L"错误", MB_OK | MB_ICONERROR);
            return false;
        }
    }

    bool LoadGameWithDialog() {
        try {
            wchar_t filename[MAX_PATH] = L"";

            OPENFILENAME ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"2048游戏文件 (*.bin)\0*.bin\0所有文件 (*.*)\0*.*\0";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                std::ifstream file(filename, std::ios::binary | std::ios::ate);
                if (!file.is_open()) {
                    MessageBox(hwnd, L"无法打开文件", L"错误", MB_OK | MB_ICONERROR);
                    return false;
                }

                std::streamsize fileSize = file.tellg();
                size_t expectedSize = sizeof(SAVE_FILE_HEADER) + sizeof(SAVE_FILE_VERSION) + sizeof(GameState);

                if (fileSize != static_cast<std::streamsize>(expectedSize)) {
                    MessageBox(hwnd, L"文件大小不匹配", L"错误", MB_OK | MB_ICONERROR);
                    return false;
                }

                file.seekg(0, std::ios::beg);

                char header[sizeof(SAVE_FILE_HEADER)];
                file.read(header, sizeof(header));
                if (memcmp(header, SAVE_FILE_HEADER, sizeof(SAVE_FILE_HEADER)) != 0) {
                    MessageBox(hwnd, L"无效的文件格式", L"错误", MB_OK | MB_ICONERROR);
                    return false;
                }

                uint32_t fileVersion;
                file.read(reinterpret_cast<char*>(&fileVersion), sizeof(fileVersion));
                if (fileVersion != SAVE_FILE_VERSION) {
                    MessageBox(hwnd, L"不支持的文件版本", L"错误", MB_OK | MB_ICONERROR);
                    return false;
                }

                GameState loadedState;
                file.read(reinterpret_cast<char*>(&loadedState), sizeof(loadedState));

                if (file.fail()) {
                    MessageBox(hwnd, L"文件读取失败", L"错误", MB_OK | MB_ICONERROR);
                    return false;
                }

                file.close();

                uint32_t calculatedChecksum = loadedState.checksum;
                loadedState.checksum = 0;
                uint32_t expectedChecksum = CalculateChecksum(loadedState);

                if (calculatedChecksum != expectedChecksum) {
                    MessageBox(hwnd, L"文件校验失败", L"错误", MB_OK | MB_ICONERROR);
                    return false;
                }

                if (!ValidateGameState(loadedState)) {
                    MessageBox(hwnd, L"加载的游戏状态无效", L"错误", MB_OK | MB_ICONERROR);
                    return false;
                }

                state = loadedState;
                keyboardEnabled = true;
                keyProcessed = false;
                SetFocus(hwnd);

                InvalidateRect(hwnd, NULL, TRUE);
                return true;
            }
            return false;
        }
        catch (const std::exception&) {
            MessageBox(hwnd, L"加载游戏时发生错误", L"错误", MB_OK | MB_ICONERROR);
            return false;
        }
    }

    uint32_t CalculateChecksum(const GameState& gameState) const {
        uint32_t checksum = 0;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&gameState);
        size_t dataSize = sizeof(gameState) - sizeof(gameState.checksum);

        for (size_t i = 0; i < dataSize; ++i) {
            checksum = (checksum << 5) + checksum + data[i];
        }

        return checksum;
    }

    bool ValidateGameState(const GameState& gameState) const {
        if (gameState.score < 0) {
            return false;
        }

        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (!IsValidTileValue(gameState.board[i][j])) {
                    return false;
                }
            }
        }

        return true;
    }

    void HandleKeyPress(WPARAM wParam, LPARAM lParam) {
        if (state.gameOver || !keyboardEnabled) return;

        // 检查是否是重复按键消息（第30位表示重复计数）
        if (lParam & 0x40000000) {
            return;
        }

        try {
            bool moved = false;

            switch (wParam) {
            case VK_LEFT:
            case 'A':
                moved = MoveLeft();
                break;
            case VK_RIGHT:
            case 'D':
                moved = MoveRight();
                break;
            case VK_UP:
            case 'W':
                moved = MoveUp();
                break;
            case VK_DOWN:
            case 'S':
                moved = MoveDown();
                break;
            default:
                return;
            }

            if (moved) {
                AddRandomTile();
                CheckGameOver();
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        catch (const std::exception&) {
            MessageBox(hwnd, L"游戏操作失败，建议重新开始游戏", L"错误", MB_OK | MB_ICONERROR);
        }
    }

    void Cleanup() {
        hMainFont.reset();
    }
};

Game2048 g_Game;
HINSTANCE g_hInstance;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RegisterWindowClass(HINSTANCE hInstance);
HWND CreateMainWindow(HINSTANCE hInstance);
void CreateControls(HWND hwnd);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    RegisterWindowClass(hInstance);

    HWND hwnd = CreateMainWindow(hInstance);
    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_Game.Cleanup();
    return (int)msg.wParam;
}

void RegisterWindowClass(HINSTANCE hInstance) {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"Game2048Window";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        throw std::runtime_error("Window class registration failed");
    }
}

HWND CreateMainWindow(HINSTANCE hInstance) {
    std::wstring windowTitle = L"2048 游戏 - 作者: 快速的飓风 - 编译时间: " +
        std::wstring(COMPILE_TIME, COMPILE_TIME + strlen(COMPILE_TIME));

    return CreateWindowEx(
        0,
        L"Game2048Window",
        windowTitle.c_str(),
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL
    );
}

void CreateControls(HWND hwnd) {
    int buttonWidth = 80;
    int buttonHeight = 30;
    int buttonSpacing = 10;
    int startX = 10;
    int startY = WINDOW_HEIGHT - 80;

    CreateWindow(
        L"BUTTON", L"新游戏",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        startX, startY, buttonWidth, buttonHeight,
        hwnd, (HMENU)1, g_hInstance, NULL
    );

    CreateWindow(
        L"BUTTON", L"保存",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        startX + buttonWidth + buttonSpacing, startY, buttonWidth, buttonHeight,
        hwnd, (HMENU)2, g_hInstance, NULL
    );

    CreateWindow(
        L"BUTTON", L"加载",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        startX + 2 * (buttonWidth + buttonSpacing), startY, buttonWidth, buttonHeight,
        hwnd, (HMENU)3, g_hInstance, NULL
    );

    CreateWindow(
        L"BUTTON", L"关于",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        startX + 3 * (buttonWidth + buttonSpacing), startY, buttonWidth, buttonHeight,
        hwnd, (HMENU)4, g_hInstance, NULL
    );

    CreateWindow(
        L"BUTTON", L"退出",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        startX + 4 * (buttonWidth + buttonSpacing), startY, buttonWidth, buttonHeight,
        hwnd, (HMENU)5, g_hInstance, NULL
    );
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    try {
        switch (msg) {
        case WM_CREATE:
            g_Game.Initialize(hwnd);
            CreateControls(hwnd);
            break;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            g_Game.Draw(hdc);
            EndPaint(hwnd, &ps);
        }
        break;

        case WM_KEYDOWN:
            g_Game.HandleKeyPress(wParam, lParam);
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case 1:
                g_Game.NewGame();
                SetFocus(hwnd);
                break;

            case 2:
                if (g_Game.SaveGameWithDialog()) {
                    MessageBox(hwnd, L"游戏已保存!", L"成功", MB_OK | MB_ICONINFORMATION);
                }
                SetFocus(hwnd);
                break;

            case 3:
                if (g_Game.LoadGameWithDialog()) {
                    MessageBox(hwnd, L"游戏已加载!", L"成功", MB_OK | MB_ICONINFORMATION);
                }
                SetFocus(hwnd);
                break;

            case 4:
            {
                std::wstring aboutText =
                    L"2048 游戏\n"
                    L"作者: 快速的飓风\n"
                    L"时间: 2025年\n"
                    L"编译时间: " +
                    std::wstring(COMPILE_TIME, COMPILE_TIME + strlen(COMPILE_TIME)) +
                    L"\n\n"
                    L"使用方向键或WASD移动方块\n"
                    L"相同数字的方块碰撞时会合并!";

                MessageBox(hwnd, aboutText.c_str(), L"关于", MB_OK | MB_ICONINFORMATION);
                SetFocus(hwnd);
            }
            break;

            case 5:
                PostQuitMessage(0);
                break;
            }
            break;

        case WM_SETFOCUS:
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
        return 0;
    }
    catch (const std::exception&) {
        MessageBox(hwnd, L"发生意外错误，程序将退出", L"严重错误", MB_OK | MB_ICONERROR);
        PostQuitMessage(1);
        return 0;
    }
}