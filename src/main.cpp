#include "raylib.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

#pragma region imgui
#include "imgui.h"
#include "rlImGui.h"
#include "imguiThemes.h"
#pragma endregion

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

enum class Cell { Wall, Floor, Goal };

struct Vec2 {
    int row, col;
    bool operator==(const Vec2& o) const { return row == o.row && col == o.col; }
};

struct GameState {
    std::vector<std::vector<Cell>> grid;
    Vec2 player;
    std::vector<Vec2> boxes;
    int moves = 0;
};

// ---------------------------------------------------------------------------
// Level loading
// ---------------------------------------------------------------------------

// Standard .sok characters:
//   # wall   . goal   @ player   $ box   * box on goal   + player on goal   (space) floor
GameState load_level(const std::vector<std::string>& raw) {
    GameState s;
    for (int r = 0; r < (int)raw.size(); r++) {
        std::vector<Cell> row;
        for (int c = 0; c < (int)raw[r].size(); c++) {
            char ch = raw[r][c];
            if      (ch == '#')                     row.push_back(Cell::Wall);
            else if (ch == '.' || ch == '*' || ch == '+') row.push_back(Cell::Goal);
            else                                    row.push_back(Cell::Floor);

            if (ch == '@' || ch == '+') s.player = {r, c};
            if (ch == '$' || ch == '*') s.boxes.push_back({r, c});
        }
        s.grid.push_back(row);
    }
    return s;
}

// A few built-in levels
const std::vector<std::vector<std::string>> LEVELS = {
    {   // Level 1 - tutorial
        "#######",
        "#@    #",
        "# $ . #",
        "#     #",
        "#######",
    },
    {   // Level 2
        "#######",
        "#.    #",
        "#   $ #",
        "# $   #",
        "##. @ #",
        "#######",
    },
    {   // Level 3
        "  #####",
        "  #   #",
        "  #$  #",
        "###  $##",
        "#  $ $ #",
        "### # ##",
        "  #  . #",
        "  # .. #",
        "  #@ . #",
        "  ######",
    },
};

// ---------------------------------------------------------------------------
// Move logic (mirrors OCaml move function)
// ---------------------------------------------------------------------------

bool is_wall(const GameState& s, Vec2 p) {
    if (p.row < 0 || p.row >= (int)s.grid.size())    return true;
    if (p.col < 0 || p.col >= (int)s.grid[p.row].size()) return true;
    return s.grid[p.row][p.col] == Cell::Wall;
}

bool is_box(const GameState& s, Vec2 p) {
    return std::find(s.boxes.begin(), s.boxes.end(), p) != s.boxes.end();
}

bool try_move(GameState& s, Vec2 dir) {
    Vec2 next     = {s.player.row + dir.row, s.player.col + dir.col};
    Vec2 nextnext = {next.row + dir.row,     next.col + dir.col};

    if (is_wall(s, next)) return false;

    auto box_it = std::find(s.boxes.begin(), s.boxes.end(), next);
    if (box_it != s.boxes.end()) {
        if (is_wall(s, nextnext) || is_box(s, nextnext)) return false;
        *box_it = nextnext;
    }

    s.player = next;
    s.moves++;
    return true;
}

bool is_solved(const GameState& s) {
    return std::all_of(s.boxes.begin(), s.boxes.end(), [&](Vec2 b) {
        return s.grid[b.row][b.col] == Cell::Goal;
    });
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

Vec2 get_input() {
    if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) return {-1,  0};
    if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) return { 1,  0};
    if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) return { 0, -1};
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) return { 0,  1};
    return {0, 0};
}

// ---------------------------------------------------------------------------
// Undo stack
// ---------------------------------------------------------------------------

struct UndoStack {
    std::vector<GameState> history;

    void push(const GameState& s) {
        history.push_back(s);
    }

    bool can_undo() const { return !history.empty(); }

    GameState pop() {
        GameState s = history.back();
        history.pop_back();
        return s;
    }

    void clear() { history.clear(); }
};

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void draw_board(const GameState& s, int tile, int offset_x, int offset_y) {
    for (int r = 0; r < (int)s.grid.size(); r++) {
        for (int c = 0; c < (int)s.grid[r].size(); c++) {
            int x = offset_x + c * tile;
            int y = offset_y + r * tile;

            switch (s.grid[r][c]) {
                case Cell::Wall:
                    DrawRectangle(x, y, tile, tile, {40, 40, 40, 255});
                    DrawRectangleLines(x, y, tile, tile, {60, 60, 60, 255});
                    break;
                case Cell::Goal:
                    DrawRectangle(x, y, tile, tile, {50, 50, 70, 255});
                    // draw X marker for goal
                    DrawLine(x + tile/4, y + tile/4, x + 3*tile/4, y + 3*tile/4, {180, 180, 80, 255});
                    DrawLine(x + 3*tile/4, y + tile/4, x + tile/4, y + 3*tile/4, {180, 180, 80, 255});
                    break;
                case Cell::Floor:
                    DrawRectangle(x, y, tile, tile, {50, 50, 60, 255});
                    break;
            }
        }
    }

    // Draw boxes
    for (const auto& b : s.boxes) {
        int x = offset_x + b.col * tile;
        int y = offset_y + b.row * tile;
        bool on_goal = s.grid[b.row][b.col] == Cell::Goal;
        Color box_color = on_goal ? Color{100, 200, 100, 255} : Color{180, 120, 60, 255};
        Color outline    = on_goal ? Color{60,  160, 60,  255} : Color{120, 80,  40, 255};

        DrawRectangle(x + 4, y + 4, tile - 8, tile - 8, box_color);
        DrawRectangleLines(x + 4, y + 4, tile - 8, tile - 8, outline);
    }

    // Draw player
    {
        int x = offset_x + s.player.col * tile;
        int y = offset_y + s.player.row * tile;
        DrawCircle(x + tile/2, y + tile/2, tile/2 - 6, {100, 160, 255, 255});
        DrawCircleLines(x + tile/2, y + tile/2, tile/2 - 6, {60, 120, 220, 255});
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(900, 600, "Sokoban");
    SetTargetFPS(60);

#pragma region imgui
    rlImGuiSetup(true);
    imguiThemes::embraceTheDarkness();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.FontGlobalScale = 1.4f;

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.Colors[ImGuiCol_WindowBg].w = 0.92f;
    }
#pragma endregion

    int current_level = 0;
    GameState game = load_level(LEVELS[current_level]);
    UndoStack undo;
    bool won = false;
    const int TILE = 56;

    while (!WindowShouldClose()) {

        // --- Input (skip if won or imgui has focus) ---
        if (!won) {
            Vec2 dir = get_input();
            if (dir.row != 0 || dir.col != 0) {
                undo.push(game);
                if (!try_move(game, dir))
                    undo.pop();  // no move happened, discard
                else
                    won = is_solved(game);
            }

            if (IsKeyPressed(KEY_Z) && undo.can_undo()) {
                game = undo.pop();
                won  = false;
            }

            if (IsKeyPressed(KEY_R)) {
                game = load_level(LEVELS[current_level]);
                undo.clear();
                won = false;
            }
        }

        // --- Draw ---
        BeginDrawing();
        ClearBackground({30, 30, 38, 255});

#pragma region imgui
        rlImGuiBegin();
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {});
        ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, {});
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
        ImGui::PopStyleColor(2);
#pragma endregion

        // --- Game panel ---
        ImGui::Begin("Sokoban", nullptr, ImGuiWindowFlags_NoScrollbar);

        // Board offset to center it inside the ImGui window
        ImVec2 win_pos  = ImGui::GetWindowPos();
        ImVec2 win_size = ImGui::GetWindowSize();

        int board_cols = game.grid.empty() ? 1 : (int)game.grid[0].size();
        int board_rows = (int)game.grid.size();
        int board_w    = board_cols * TILE;
        int board_h    = board_rows * TILE;
        int offset_x   = (int)(win_pos.x + (win_size.x - board_w) / 2);
        int offset_y   = (int)(win_pos.y + (win_size.y - board_h) / 2 + 20);

        draw_board(game, TILE, offset_x, offset_y);

        if (won) {
            DrawText("YOU WIN!", offset_x + board_w/2 - 80, offset_y + board_h/2 - 20, 40, GREEN);
        }

        ImGui::End();

        // --- Controls panel ---
        ImGui::Begin("Controls");

        ImGui::Text("Level %d / %d", current_level + 1, (int)LEVELS.size());
        ImGui::Text("Moves: %d", game.moves);
        ImGui::Separator();

        ImGui::Text("Arrow keys / WASD  move");
        ImGui::Text("Z                  undo");
        ImGui::Text("R                  restart");
        ImGui::Separator();

        if (ImGui::Button("Restart", {120, 30})) {
            game = load_level(LEVELS[current_level]);
            undo.clear();
            won = false;
        }

        if (ImGui::Button("Undo", {120, 30}) && undo.can_undo()) {
            game = undo.pop();
            won  = false;
        }

        ImGui::Separator();
        ImGui::Text("Levels:");

        for (int i = 0; i < (int)LEVELS.size(); i++) {
            char label[32];
            snprintf(label, sizeof(label), "Level %d", i + 1);
            if (ImGui::Button(label, {120, 28})) {
                current_level = i;
                game = load_level(LEVELS[current_level]);
                undo.clear();
                won = false;
            }
        }

        if (won) {
            ImGui::Separator();
            ImGui::TextColored({0.4f, 1.0f, 0.4f, 1.0f}, "Solved!");
            int next = current_level + 1;
            if (next < (int)LEVELS.size()) {
                if (ImGui::Button("Next Level >>", {120, 30})) {
                    current_level = next;
                    game = load_level(LEVELS[current_level]);
                    undo.clear();
                    won = false;
                }
            }
        }

        ImGui::End();

#pragma region imgui
        rlImGuiEnd();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
#pragma endregion

        EndDrawing();
    }

#pragma region imgui
    rlImGuiShutdown();
#pragma endregion

    CloseWindow();
    return 0;
}