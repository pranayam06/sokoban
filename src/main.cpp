#include "raylib.h"
#include <vector>
#include <algorithm>
#include <string>
#include <cstdio>

#include "imgui.h"
#include "rlImGui.h"
#include "imguiThemes.h"

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
    Vec2 player = {0, 0};
    std::vector<Vec2> boxes;
    int moves = 0;
};

// ---------------------------------------------------------------------------
// Levels
// ---------------------------------------------------------------------------

const std::vector<std::vector<std::string>> LEVELS = {
    {
        "#######",
        "#@    #",
        "# $   #",
        "#   . #",
        "# $   #",
        "#   . #",
        "#######",
    },
    {
        "#######",
        "#.    #",
        "#   $ #",
        "# $   #",
        "##. @ #",
        "#######",
    },
    {
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
// Logic
// ---------------------------------------------------------------------------

GameState load_level(const std::vector<std::string>& raw) {
    GameState s;
    for (int r = 0; r < (int)raw.size(); r++) {
        std::vector<Cell> row;
        for (int c = 0; c < (int)raw[r].size(); c++) {
            char ch = raw[r][c];
            if      (ch == '#')                             row.push_back(Cell::Wall);
            else if (ch == '.' || ch == '*' || ch == '+')  row.push_back(Cell::Goal);
            else                                            row.push_back(Cell::Floor);
            if (ch == '@' || ch == '+') s.player = {r, c};
            if (ch == '$' || ch == '*') s.boxes.push_back({r, c});
        }
        s.grid.push_back(row);
    }
    return s;
}

bool cell_is_wall(const GameState& s, Vec2 p) {
    if (p.row < 0 || p.row >= (int)s.grid.size())         return true;
    if (p.col < 0 || p.col >= (int)s.grid[p.row].size())  return true;
    return s.grid[p.row][p.col] == Cell::Wall;
}

bool cell_has_box(const GameState& s, Vec2 p) {
    return std::find(s.boxes.begin(), s.boxes.end(), p) != s.boxes.end();
}

bool try_move(GameState& s, Vec2 dir) {
    Vec2 next     = {s.player.row + dir.row, s.player.col + dir.col};
    Vec2 nextnext = {next.row + dir.row,     next.col + dir.col};
    if (cell_is_wall(s, next)) return false;
    auto box_it = std::find(s.boxes.begin(), s.boxes.end(), next);
    if (box_it != s.boxes.end()) {
        if (cell_is_wall(s, nextnext) || cell_has_box(s, nextnext)) return false;
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
// Drawing  (pure Raylib, no ImGui)
// ---------------------------------------------------------------------------

void draw_board(const GameState& s, int tile, int ox, int oy) {
    // grid
    for (int r = 0; r < (int)s.grid.size(); r++) {
        for (int c = 0; c < (int)s.grid[r].size(); c++) {
            int x = ox + c * tile;
            int y = oy + r * tile;
            switch (s.grid[r][c]) {
                case Cell::Wall:
                    DrawRectangle(x, y, tile, tile, {40, 40, 40, 255});
                    DrawRectangleLines(x, y, tile, tile, {60, 60, 60, 255});
                    break;
                case Cell::Goal:
                    DrawRectangle(x, y, tile, tile, {50, 50, 70, 255});
                    DrawLine(x+tile/4, y+tile/4, x+3*tile/4, y+3*tile/4, {180,180,80,255});
                    DrawLine(x+3*tile/4, y+tile/4, x+tile/4, y+3*tile/4, {180,180,80,255});
                    break;
                case Cell::Floor:
                    DrawRectangle(x, y, tile, tile, {50, 50, 60, 255});
                    break;
            }
        }
    }
    // boxes
    for (const auto& b : s.boxes) {
        int x = ox + b.col * tile;
        int y = oy + b.row * tile;
        bool on_goal = s.grid[b.row][b.col] == Cell::Goal;
        Color col  = on_goal ? Color{100,200,100,255} : Color{180,120,60,255};
        Color edge = on_goal ? Color{60,160,60,255}   : Color{120,80,40,255};
        DrawRectangle(x+4, y+4, tile-8, tile-8, col);
        DrawRectangleLines(x+4, y+4, tile-8, tile-8, edge);
    }
    // player
    int px = ox + s.player.col * tile;
    int py = oy + s.player.row * tile;
    DrawCircle(px+tile/2, py+tile/2, tile/2-6, {100,160,255,255});
    DrawCircleLines(px+tile/2, py+tile/2, tile/2-6, {60,120,220,255});
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void) {
    const int WIN_W  = 1000;
    const int WIN_H  = 700;
    const int SIDE_W = 160;   // ImGui panel width
    const int TILE   = 60;

    InitWindow(WIN_W, WIN_H, "Sokoban");
    SetTargetFPS(60);

    rlImGuiSetup(true);
    imguiThemes::embraceTheDarkness();
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.3f;
    // disable docking and viewports — not needed
    io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;

    int current_level = 0;
    GameState game = load_level(LEVELS[current_level]);
    std::vector<GameState> undo_stack;
    bool won = false;

    while (!WindowShouldClose()) {

        // --- input (always active) ---
        if (!won) {
            Vec2 dir = {0, 0};
            if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) dir = {-1,  0};
            if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) dir = { 1,  0};
            if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) dir = { 0, -1};
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) dir = { 0,  1};

            if (dir.row != 0 || dir.col != 0) {
                undo_stack.push_back(game);
                if (!try_move(game, dir))
                    undo_stack.pop_back();
                else
                    won = is_solved(game);
            }
            if (IsKeyPressed(KEY_Z) && !undo_stack.empty()) {
                game = undo_stack.back();
                undo_stack.pop_back();
                won = false;
            }
            if (IsKeyPressed(KEY_R)) {
                game = load_level(LEVELS[current_level]);
                undo_stack.clear();
                won = false;
            }
        }

        // --- draw ---
        BeginDrawing();
        ClearBackground({30, 30, 38, 255});

        // board centered in the left (WIN_W - SIDE_W) pixels
        int game_area_w = WIN_W - SIDE_W;
        int board_w = (int)game.grid[0].size() * TILE;
        int board_h = (int)game.grid.size()    * TILE;
        int ox = (game_area_w - board_w) / 2;
        int oy = (WIN_H       - board_h) / 2;
        draw_board(game, TILE, ox, oy);

        if (won)
            DrawText("YOU WIN!", ox + board_w/2 - 70, oy - 40, 32, GREEN);

        // --- ImGui side panel ---
        rlImGuiBegin();

        ImGui::SetNextWindowPos ({(float)(WIN_W - SIDE_W), 0},       ImGuiCond_Always);
        ImGui::SetNextWindowSize({(float)SIDE_W,           (float)WIN_H}, ImGuiCond_Always);
        ImGui::Begin("##side", nullptr,
            ImGuiWindowFlags_NoTitleBar  |
            ImGuiWindowFlags_NoResize    |
            ImGuiWindowFlags_NoMove      |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::Text("Moves");
        ImGui::Text("%d", game.moves);
        ImGui::Separator();

        for (int i = 0; i < (int)LEVELS.size(); i++) {
            char label[32];
            snprintf(label, sizeof(label), "Level %d", i + 1);
            bool sel = (i == current_level);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, {0.2f,0.5f,0.2f,1.f});
            if (ImGui::Button(label, {(float)(SIDE_W-20), 28})) {
                current_level = i;
                game = load_level(LEVELS[current_level]);
                undo_stack.clear();
                won = false;
            }
            if (sel) ImGui::PopStyleColor();
        }

        if (won) {
            ImGui::Separator();
            ImGui::TextColored({0.4f,1.f,0.4f,1.f}, "Solved!");
            if (current_level + 1 < (int)LEVELS.size()) {
                if (ImGui::Button("Next >>", {(float)(SIDE_W-20), 28})) {
                    current_level++;
                    game = load_level(LEVELS[current_level]);
                    undo_stack.clear();
                    won = false;
                }
            }
        }

        ImGui::End();
        rlImGuiEnd();

        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}