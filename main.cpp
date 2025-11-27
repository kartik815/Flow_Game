#include "raylib.h"
#include <vector>
#include <map>
#include <utility>
#include <cmath>
#include <cstdio> 

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

static bool aiAllowed = true;
static map<unsigned int, bool> pathLocked;

static vector<pair<int,int>> currentPath;
static map<unsigned int, vector<pair<int,int>>> savedPaths;

static bool dragging = false;
static Color currentColor = {0,0,0,0};

static const vector<pair<int,int>> AI_INTENDED_PATH = {
    {1,2},{1,3},{1,4},{1,5},
    {2,5},{3,5},{4,5},{4,4}
};

inline bool isInside(int r, int c){
    return r>=0 && r<GRID && c>=0 && c<GRID;
}

inline bool isNeighbor(int r1, int c1, int r2, int c2){
    return (abs(r1-r2) + abs(c1-c2)) == 1;
}

inline unsigned int colorKey(const Color &c){
    return ((unsigned int)c.r << 24) |
           ((unsigned int)c.g << 16) |
           ((unsigned int)c.b << 8 ) |
            (unsigned int)c.a;
}

inline Color colorFromKey(unsigned int k){
    return Color {
        (unsigned char)((k >> 24) & 0xFF),
        (unsigned char)((k >> 16) & 0xFF),
        (unsigned char)((k >> 8 ) & 0xFF),
        (unsigned char)(k & 0xFF)
    };
}

inline bool colorEqual(const Color &a, const Color &b){
    return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;
}

pair<int,int> mouseCell() {
    int mx = GetMouseX();
    int my = GetMouseY();
    int c = mx / CELL_SIZE;
    int r = my / CELL_SIZE;
    if (r<0) r=0; if (r>=GRID) r=GRID-1;
    if (c<0) c=0; if (c>=GRID) c=GRID-1;
    return {r, c};
}

void drawPipePath(const vector<pair<int,int>>& path, const Color &col) {
    if (path.empty()) return;

    float thickness = CELL_SIZE * 0.45f;

    if (path.size() == 1) {
        int r = path[0].first, c = path[0].second;
        Vector2 p = { c*CELL_SIZE + CELL_SIZE*0.5f,
                      r*CELL_SIZE + CELL_SIZE*0.5f };
        DrawCircleV(p, thickness*0.5f, col);
        return;
    }

    for (size_t i=0;i<path.size()-1;++i) {
        int r1=path[i].first, c1=path[i].second;
        int r2=path[i+1].first, c2=path[i+1].second;

        Vector2 p1 = { c1*CELL_SIZE + CELL_SIZE*0.5f,
                       r1*CELL_SIZE + CELL_SIZE*0.5f };
        Vector2 p2 = { c2*CELL_SIZE + CELL_SIZE*0.5f,
                       r2*CELL_SIZE + CELL_SIZE*0.5f };

        DrawLineEx(p1, p2, thickness, col);
        DrawCircleV(p1, thickness*0.5f, col);
        if (i == path.size()-2) DrawCircleV(p2, thickness*0.5f, col);
    }
}

void clearGridPipeColorsForPath(const vector<pair<int,int>>& path) {
    for (auto &p : path) {
        int r=p.first, c=p.second;
        if (!grid[r][c].isDot) {
            grid[r][c].hasPipe = false;
            grid[r][c].pipeColor = {0,0,0,0};
        }
    }
}

int indexInPath(const vector<pair<int,int>>& path, int r, int c) {
    for (int i=0;i<(int)path.size(); ++i) {
        if (path[i].first==r && path[i].second==c) return i;
    }
    return -1;
}

bool doesPathBlockAI(const vector<pair<int,int>>& path) {
    for (auto &p : path) {
        int pr = p.first, pc = p.second;

        if (grid[pr][pc].isDot) continue;
        for (auto &a : AI_INTENDED_PATH) {
            if (pr==a.first && pc==a.second) return true;
        }
    }
    return false;
}

void tryMoveAI() {
    if (!aiAllowed) return;

    unsigned int BLUEKEY = colorKey(BLUE);

    if (pathLocked[BLUEKEY]) {
        printf("[AI] blocked: BLUE path is locked already.\n");
        aiAllowed = false;
        return;
    }
    if (savedPaths.count(BLUEKEY) && !savedPaths[BLUEKEY].empty()) {
        printf("[AI] blue path already exists; AI will not move.\n");
        aiAllowed = false;
        return;
    }

    for (auto &cell : AI_INTENDED_PATH) {
        int rr = cell.first, cc = cell.second;
        if (grid[rr][cc].hasPipe && !colorEqual(grid[rr][cc].pipeColor, BLUE)) {
            printf("[AI] cannot move: AI path is blocked by player's pipe at (%d,%d). AI disabled.\n", rr, cc);
            aiAllowed = false;
            return;
        }

        if (grid[rr][cc].isDot && !colorEqual(grid[rr][cc].dotColor, BLUE)) {

            printf("[AI] cannot move: AI path cell is a terminal of other color at (%d,%d). AI disabled.\n", rr, cc);
            aiAllowed = false;
            return;
        }
    }

    savedPaths[BLUEKEY] = AI_INTENDED_PATH;
    for (auto &p : AI_INTENDED_PATH) {
        if (!grid[p.first][p.second].isDot) {
            grid[p.first][p.second].hasPipe = true;
            grid[p.first][p.second].pipeColor = BLUE;
        }
    }
    pathLocked[BLUEKEY] = true;
    aiAllowed = false;
    printf("[AI] placed BLUE path.\n");
}

void tryExtendPath(int nr, int nc) {
    if (currentPath.empty()) return;

    auto last = currentPath.back();
    if (!isNeighbor(last.first, last.second, nr, nc)) return;

    if (grid[nr][nc].isDot && !colorEqual(grid[nr][nc].dotColor, currentColor)) {

        return;
    }

    if (currentPath.size()>=2) {
        auto secondLast = currentPath[currentPath.size()-2];
        if (secondLast.first==nr && secondLast.second==nc) {
            auto lastCell=currentPath.back();
            if (!grid[lastCell.first][lastCell.second].isDot) {
                grid[lastCell.first][lastCell.second].hasPipe = false;
                grid[lastCell.first][lastCell.second].pipeColor = {0,0,0,0};
            }
            currentPath.pop_back();
            return;
        }
    }

    if (grid[nr][nc].hasPipe) {
        if (!colorEqual(grid[nr][nc].pipeColor, currentColor)) return;

        unsigned int k = colorKey(grid[nr][nc].pipeColor);
        auto &other = savedPaths[k];

        int idx = indexInPath(other, nr, nc);
        if (idx >= 0) {
            for (int j=idx+1;j<(int)other.size();++j) {
                int rr=other[j].first, cc=other[j].second;
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
        return;
    }

    if (grid[nr][nc].isDot && colorEqual(grid[nr][nc].dotColor, currentColor)) {
        currentPath.push_back({nr,nc});
        return;
    }

    currentPath.push_back({nr,nc});
    if (!grid[nr][nc].isDot) {
        grid[nr][nc].hasPipe = true;
        grid[nr][nc].pipeColor = currentColor;
    }
}

void lockCurrentPathIfValid() {
    if (currentPath.size() < 2) return;

    auto last = currentPath.back();
    int r=last.first, c=last.second;

    if (grid[r][c].isDot && colorEqual(grid[r][c].dotColor, currentColor)) {

        if (doesPathBlockAI(currentPath)) {

            for (auto&p: currentPath) {
                int rr=p.first, cc=p.second;
                if (!grid[rr][cc].isDot) {
                    grid[rr][cc].hasPipe=false;
                    grid[rr][cc].pipeColor={0,0,0,0};
                }
            }
            printf("[Invalid move] You blocked the AI path. Move cancelled. AI disabled.\n");
            aiAllowed = false; 
            currentPath.clear();
            return;
        }

        unsigned int k=colorKey(currentColor);

        if (savedPaths.count(k))
            clearGridPipeColorsForPath(savedPaths[k]);

        savedPaths[k] = currentPath;

        for (auto &p : currentPath) {
            if (!grid[p.first][p.second].isDot) {
                grid[p.first][p.second].hasPipe = true;
                grid[p.first][p.second].pipeColor = currentColor;
            }
        }

        pathLocked[k] = true;

        if (colorEqual(currentColor, RED)) {
            aiAllowed = true;
            tryMoveAI();
        }

        return;
    }

    for (auto&p: currentPath) {
        int rr=p.first, cc=p.second;
        if (!grid[rr][cc].isDot) {
            grid[rr][cc].hasPipe=false;
            grid[rr][cc].pipeColor={0,0,0,0};
        }
    }
}

void loadSampleLevel() {
    for (int r=0;r<GRID;++r)
        for (int c=0;c<GRID;++c)
            grid[r][c] = Cell();

    grid[0][0].isDot = true; grid[0][0].dotColor = RED;
    grid[5][3].isDot = true; grid[5][3].dotColor = RED;

    grid[1][2].isDot = true; grid[1][2].dotColor = BLUE;
    grid[4][4].isDot = true; grid[4][4].dotColor = BLUE;
}

int main() {
    InitWindow(GRID*CELL_SIZE, GRID*CELL_SIZE, "Flow + AI Move (blocked detection)");
    SetTargetFPS(60);

    loadSampleLevel();

    while (!WindowShouldClose()) {

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            auto [r,c] = mouseCell();

            if (grid[r][c].isDot) {

                unsigned int k = colorKey(grid[r][c].dotColor);

                if (pathLocked[k]) {
                    dragging = false;
                    goto next_frame;
                }

                dragging = true;
                currentColor = grid[r][c].dotColor;

                if (savedPaths.count(k))
                    clearGridPipeColorsForPath(savedPaths[k]);

                currentPath.clear();
                currentPath.push_back({r,c});
                goto next_frame;
            }

            if (grid[r][c].hasPipe) {

                unsigned int k = colorKey(grid[r][c].pipeColor);

                if (pathLocked[k]) {
                    dragging = false;
                    goto next_frame;
                }

                dragging = true;
                currentColor = grid[r][c].pipeColor;

                if (savedPaths.count(k))
                    currentPath = savedPaths[k];
                else {
                    currentPath.clear();
                    currentPath.push_back({r,c});
                }
                goto next_frame;
            }
        }

        if (dragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            auto[r,c]=mouseCell();
            if (!currentPath.empty()) {
                auto last=currentPath.back();
                if (!(last.first==r && last.second==c)) {
                    if (isNeighbor(last.first,last.second,r,c))
                        tryExtendPath(r,c);
                }
            }
        }

        if (dragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            lockCurrentPathIfValid();
            dragging = false;
            currentColor={0,0,0,0};
            currentPath.clear();
        }

        next_frame:
        BeginDrawing();
        ClearBackground((Color){18,18,18,255});

        Color gridLine = (Color){55,55,55,255};
        for (int r=0;r<GRID;++r)
            for (int c=0;c<GRID;++c)
                DrawRectangleLines(c*CELL_SIZE, r*CELL_SIZE, CELL_SIZE, CELL_SIZE, gridLine);

        for (auto &kv : savedPaths)
            drawPipePath(kv.second, colorFromKey(kv.first));

        if (!currentPath.empty())
            drawPipePath(currentPath, currentColor);

        float rad = CELL_SIZE * 0.35f;
        for (int r=0;r<GRID;++r)
            for (int c=0;c<GRID;++c)
                if (grid[r][c].isDot) {
                    Vector2 pos = { c*CELL_SIZE + CELL_SIZE*0.5f,
                                    r*CELL_SIZE + CELL_SIZE*0.5f };
                    DrawCircleV(pos, rad+4, Fade(grid[r][c].dotColor, 0.08f));
                    DrawCircleV(pos, rad, grid[r][c].dotColor);
                }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}