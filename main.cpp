#include "raylib.h"
#include <vector>
#include <map>
#include <utility>
#include <cmath>

using std::vector;
using std::pair;
using std::map;

const int GRID = 6;             
const int CELL_SIZE = 80;         

struct Cell {
    bool isDot = false;
    Color dotColor = {0,0,0,0};

    bool hasPipe = false;
    Color pipeColor = {0,0,0,0};
};

static Cell grid[GRID][GRID];

static vector<pair<int,int>> currentPath;

static map<unsigned int, vector<pair<int,int>>> savedPaths;

static bool dragging = false;
static Color currentColor = {0,0,0,0};

inline bool isInside(int r, int c){
    return r >= 0 && r < GRID && c >= 0 && c < GRID;
}

inline bool isNeighbor(int r1, int c1, int r2, int c2){
    return (abs(r1-r2) + abs(c1-c2)) == 1;
}

inline unsigned int colorKey(const Color &c){
    return ( (unsigned int)c.r << 24 ) | ( (unsigned int)c.g << 16 ) | ( (unsigned int)c.b << 8 ) | (unsigned int)c.a;
}

inline Color colorFromKey(unsigned int k){
    unsigned char r = (k >> 24) & 0xFF;
    unsigned char g = (k >> 16) & 0xFF;
    unsigned char b = (k >> 8)  & 0xFF;
    unsigned char a = (k)       & 0xFF;
    return Color{ r, g, b, a };
}

inline bool colorEqual(const Color &a, const Color &b){
    return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;
}

pair<int,int> mouseCell() {
    int mx = GetMouseX();
    int my = GetMouseY();
    int c = mx / CELL_SIZE;
    int r = my / CELL_SIZE;
    if (r < 0) r = 0; if (r >= GRID) r = GRID-1;
    if (c < 0) c = 0; if (c >= GRID) c = GRID-1;
    return {r, c};
}
void drawPipePath(const vector<pair<int,int>>& path, const Color &col) {
    if (path.empty()) return;

    float thickness = CELL_SIZE * 0.45f;
    if (path.size() == 1) {
        int r = path[0].first, c = path[0].second;
        Vector2 p = { c*CELL_SIZE + CELL_SIZE*0.5f, r*CELL_SIZE + CELL_SIZE*0.5f };
        DrawCircleV(p, thickness*0.5f, col);
        return;
    }

    for (size_t i = 0; i < path.size()-1; ++i) {
        int r1 = path[i].first, c1 = path[i].second;
        int r2 = path[i+1].first, c2 = path[i+1].second;
        Vector2 p1 = { c1*CELL_SIZE + CELL_SIZE*0.5f, r1*CELL_SIZE + CELL_SIZE*0.5f };
        Vector2 p2 = { c2*CELL_SIZE + CELL_SIZE*0.5f, r2*CELL_SIZE + CELL_SIZE*0.5f };

        DrawLineEx(p1, p2, thickness, col);
        DrawCircleV(p1, thickness*0.5f, col);
        if (i == path.size()-2) DrawCircleV(p2, thickness*0.5f, col);
    }
}

void clearGridPipeColorsForPath(const vector<pair<int,int>>& path) {
    for (auto &p : path) {
        int r = p.first, c = p.second;
        if (isInside(r,c) && !grid[r][c].isDot) {
            grid[r][c].hasPipe = false;
            grid[r][c].pipeColor = {0,0,0,0};
        }
    }
}
int indexInPath(const vector<pair<int,int>>& path, int r, int c) {
    for (int i = 0; i < (int)path.size(); ++i) {
        if (path[i].first == r && path[i].second == c) return i;
    }
    return -1;
}

void tryExtendPath(int nr, int nc) {
    if (currentPath.empty()) return;
    auto last = currentPath.back();

    if (!isNeighbor(last.first, last.second, nr, nc)) return;
    if (currentPath.size() >= 2) {
        auto secondLast = currentPath[currentPath.size()-2];
        if (secondLast.first == nr && secondLast.second == nc) {
            auto lastCell = currentPath.back();
            if (!grid[lastCell.first][lastCell.second].isDot) {
                grid[lastCell.first][lastCell.second].hasPipe = false;
                grid[lastCell.first][lastCell.second].pipeColor = {0,0,0,0};
            }
            currentPath.pop_back();
            return;
        }
    }

    if (grid[nr][nc].hasPipe) {
        if (!colorEqual(grid[nr][nc].pipeColor, currentColor)) {
            return;
        } else {
            unsigned int k = colorKey(grid[nr][nc].pipeColor);
            auto it = savedPaths.find(k);
            if (it != savedPaths.end()) {
                vector<pair<int,int>> &other = it->second;
                int idx = indexInPath(other, nr, nc);
                if (idx >= 0) {
                    for (int j = idx+1; j < (int)other.size(); ++j) {
                        int rr = other[j].first, cc = other[j].second;
                        if (!grid[rr][cc].isDot) {
                            grid[rr][cc].hasPipe = false;
                            grid[rr][cc].pipeColor = {0,0,0,0};
                        }
                    }
                    other.resize(idx+1);
                } else {
                    clearGridPipeColorsForPath(other);
                    other.clear();
                }
            } else {
                grid[nr][nc].hasPipe = false;
                grid[nr][nc].pipeColor = {0,0,0,0};
            }
            return;
        }
    }

    if (grid[nr][nc].isDot && colorEqual(grid[nr][nc].dotColor, currentColor)) {
        currentPath.push_back({nr, nc});
        return;
    }

    currentPath.push_back({nr, nc});
    if (!grid[nr][nc].isDot) {
        grid[nr][nc].hasPipe = true;
        grid[nr][nc].pipeColor = currentColor;
    }
}

void lockCurrentPathIfValid() {
    if (currentPath.size() < 2) return;
    auto last = currentPath.back();
    int lr = last.first, lc = last.second;
    if (isInside(lr,lc) && grid[lr][lc].isDot && colorEqual(grid[lr][lc].dotColor, currentColor)) {
        unsigned int k = colorKey(currentColor);
        if (savedPaths.count(k)) {
            clearGridPipeColorsForPath(savedPaths[k]);
        }
        savedPaths[k] = currentPath;
        for (auto &p : currentPath) {
            int r = p.first, c = p.second;
            if (!grid[r][c].isDot) {
                grid[r][c].hasPipe = true;
                grid[r][c].pipeColor = currentColor;
            }
        }
    } else {
        if (currentPath.size() <= 1) {
            for (auto &p : currentPath) {
                int r = p.first, c = p.second;
                if (isInside(r,c) && !grid[r][c].isDot) {
                    grid[r][c].hasPipe = false;
                    grid[r][c].pipeColor = {0,0,0,0};
                }
            }
        }
    }
}
void loadSampleLevel() {
    for (int r=0;r<GRID;++r) for (int c=0;c<GRID;++c) grid[r][c] = Cell();
    grid[0][0].isDot = true; grid[0][0].dotColor = RED;
    grid[5][3].isDot = true; grid[5][3].dotColor = RED;

    grid[1][2].isDot = true; grid[1][2].dotColor = BLUE;
    grid[4][4].isDot = true; grid[4][4].dotColor = BLUE;

    grid[2][1].isDot = true; grid[2][1].dotColor = ORANGE;
    grid[3][5].isDot = true; grid[3][5].dotColor = ORANGE;
}

int main() {
    InitWindow(GRID * CELL_SIZE, GRID * CELL_SIZE, "Flow Premium - Clean");
    SetTargetFPS(60);

    loadSampleLevel();

    while (!WindowShouldClose()) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            auto [r, c] = mouseCell();
            if (!isInside(r,c)) {; }
            else if (grid[r][c].isDot) {
                dragging = true;
                currentColor = grid[r][c].dotColor;
                unsigned int k = colorKey(currentColor);

                if (savedPaths.count(k)) {
                    clearGridPipeColorsForPath(savedPaths[k]);
                    savedPaths[k].clear();
                }
                for (int rr=0; rr<GRID; ++rr) for (int cc=0; cc<GRID; ++cc) {
                    if (grid[rr][cc].hasPipe && colorEqual(grid[rr][cc].pipeColor, currentColor) && !grid[rr][cc].isDot) {
                        grid[rr][cc].hasPipe = false;
                        grid[rr][cc].pipeColor = {0,0,0,0};
                    }
                }

                currentPath.clear();
                currentPath.push_back({r,c});
            }
            else if (grid[r][c].hasPipe) {
                dragging = true;
                currentColor = grid[r][c].pipeColor;
                unsigned int k = colorKey(currentColor);
                if (savedPaths.count(k) && !savedPaths[k].empty()) {
                    currentPath = savedPaths[k];
                } else {
                    currentPath.clear();
                    currentPath.push_back({r,c});
                }
            }
        }

        if (dragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            auto [r, c] = mouseCell();
            if (isInside(r,c)) {
                if (!currentPath.empty()) {
                    auto last = currentPath.back();
                    if (last.first == r && last.second == c) {
                    } else {
                        if (isNeighbor(last.first, last.second, r, c)) {
                            tryExtendPath(r, c);
                        }
                    }
                }
            }
        }

        if (dragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            lockCurrentPathIfValid();
            dragging = false;
            currentColor = {0,0,0,0};
            currentPath.clear();
        }

        BeginDrawing();
        ClearBackground((Color){18,18,18,255});

        Color gridLine = (Color){55,55,55,255};
        for (int r=0; r<GRID; ++r) {
            for (int c=0; c<GRID; ++c) {
                DrawRectangleLines(c*CELL_SIZE, r*CELL_SIZE, CELL_SIZE, CELL_SIZE, gridLine);
            }
        }

        for (auto &kv : savedPaths) {
            if (kv.second.empty()) continue;
            unsigned int k = kv.first;
            Color col = colorFromKey(k);
            drawPipePath(kv.second, col);
        }

        if (!currentPath.empty() && currentColor.a != 0) {
            drawPipePath(currentPath, currentColor);
        }

        float dotRadius = CELL_SIZE * 0.35f;
        for (int r=0; r<GRID; ++r) {
            for (int c=0; c<GRID; ++c) {
                if (grid[r][c].isDot) {
                    Vector2 pos = { c*CELL_SIZE + CELL_SIZE*0.5f, r*CELL_SIZE + CELL_SIZE*0.5f };
                    DrawCircleV(pos, dotRadius + 4, Fade(grid[r][c].dotColor, 0.08f));
                    DrawCircleV(pos, dotRadius, grid[r][c].dotColor);
                }
            }
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
