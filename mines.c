#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <time.h>

#define min(a, b) ((a)<(b) ? (a) : (b))
#define max(a, b) ((a)>(b) ? (a) : (b))

#define NUM_BOMBS 5
#define NUM_FAIRIES 5

#define WIDTH  6
#define HEIGHT 6

#define StaticArraySize(arr) ((sizeof(arr) / sizeof((arr)[0])))

typedef enum {
    GAME_MODE_CLASSIC,
    GAME_MODE_FAIRY,
} Game_Mode;

enum {
    COLOR_UNKNOWN_CELL         = 0x4a2222ff,
    COLOR_UNKNOWN_CELL_OUTLINE = 0x5c3030ff,
    COLOR_EMPTY_CELL           = 0x291414ff,
    COLOR_TEXT                 = 0x875b5bff,
    COLOR_TEXT_FAIRY           = COLOR_TEXT,
    //COLOR_TEXT_FAIRY           = 0xe6c38cff,
} color_theme;

typedef enum {
    CELL_CLEAR = 0,
    CELL_MINE,
    CELL_FAIRY,
} Cell_Type;

typedef enum {
    FLAG_NONE,
    FLAG_MINE,
    FLAG_FAIRY
} Flag_Type;

typedef struct {
    int x, y;
} vec2i;

typedef struct {
    bool       revealed;
    Cell_Type  type;
    Flag_Type  flag;
    bool       highlighted;
    bool       has_fairy_neighbours;
    bool       visited;
    float      neighbours;
    Rectangle  rect;
} cell_t;

typedef struct {
    Rectangle actual;
    float x, y, width, height;
    bool  size_changed;
} panel_t;

typedef struct {
    Font    font;
    Texture bomb, fairy, fairy_flag, flag;
} assets_t;

typedef struct {
    panel_t panel;
    Game_Mode game_mode;
    cell_t *grid;
    int width, height;
    int num_flags, num_bombs;
    int num_seen_fairies, num_fairies;
    int mistakes, total_mistakes;
    bool is_initialized;
} grid_t;

typedef struct {
    assets_t assets;
    panel_t  screen;
    grid_t   grid;
    bool     game_over;
} app_t;

void print_rect(Rectangle r) {
    printf("%.2f, %.2f, %.2f, %.2f\n", r.x, r.y, r.width, r.height);
}

bool compare_rects(Rectangle a, Rectangle b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

Rectangle get_cell_rect(Rectangle panel, grid_t *grid, int x, int y) {
    float dx = (float)x / grid->width;
    float dy = (float)y / grid->height;

    float width = round((float)panel.width / grid->width);

    Rectangle result;

    result.x = panel.x + dx * panel.width;
    result.y = panel.y + dy * panel.height;
    result.width = width;
    result.height = width;

    return result;
}

Rectangle smallen(Rectangle r, int amt) {
    r.x += amt;
    r.y += amt;
    r.width -= amt * 2;
    r.height -= amt * 2;
    return r;
}

int in_bounds(int x, int y, int w, int h) {
    return !(x < 0 || y < 0 || x >= w || y >= h);
}

void draw_text_centered(Font font, float font_size, const char *text, float x, float y, Color col, panel_t *screen) {
    Vector2 size = MeasureTextEx(font, text, font_size, 1);

    Vector2 pos;

    pos.x = screen->actual.x + x * screen->actual.width - size.x / 2.f;
    pos.y = screen->actual.y + y * screen->actual.height - size.y / 2.f;

    DrawTextEx(font, text, pos, font_size, 1, col);
}

cell_t *get_cell(grid_t *grid, int x, int y) {
    return &grid->grid[x+y*grid->width];
}

// ignores the direct neighbourhood of (ix, iy)
void grid_randomly_place(grid_t *grid, Cell_Type type, int ix, int iy) {
    bool repeat;

    do {
        repeat = false;

        int x = rand () % grid->width;
        int y = rand () % grid->height;

        int dx = x - ix;
        int dy = y - iy;

        if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1)
            repeat = true;
        else
            repeat = grid->grid[x+y*grid->width].type != CELL_CLEAR;

        if (!repeat)
            grid->grid[x + y * grid->width].type = type;
    } while (repeat);
}

void grid_generate(grid_t *grid, int ix, int iy) {
    for (int i = 0; i < grid->num_bombs; i++) {
        grid_randomly_place(grid, CELL_MINE, ix, iy);
    }

    if (grid->game_mode == GAME_MODE_FAIRY) {
        for (int i = 0; i < grid->num_fairies; i++) {
            grid_randomly_place(grid, CELL_FAIRY, ix, iy);
        }
    }

    for (int i = 0; i < grid->width * grid->height; i++) {
        int x = i % grid->width;
        int y = i / grid->width;

        float neighbours = 0;

        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;

                int nx = x + dx;
                int ny = y + dy;

                if (ny < 0 || nx < 0 || nx >= grid->width || ny >= grid->height)
                    continue;

                Cell_Type type = grid->grid[nx + ny * grid->width].type;

                if (type == CELL_MINE) {
                    neighbours++;
                } else if (type == CELL_FAIRY) {
                    neighbours += 0.5f;
                    grid->grid[i].has_fairy_neighbours = true;
                }
            }
        }

        grid->grid[i].neighbours = neighbours;
    }
}

void click(grid_t *grid, int x, int y) {
    if (!grid->is_initialized) {
        grid_generate(grid, x, y);
        grid->is_initialized = true;
    }

    int w = grid->width;
    int h = grid->height;

    cell_t *first = &grid->grid[x+y*w];

    if (first->revealed) return;

    if (first->neighbours > 0) {
        first->revealed = true;
        return;
    }

    for (int i = 0; i < w*h; i++)
        grid->grid[x+y*w].visited = false;

    int *index_queue = calloc(w * h, sizeof(int));
    int head = 0;
    int tail = 0;

    index_queue[0] = x + y * w;

    while (head <= tail) {
        int curr_idx = index_queue[head++];

        cell_t *cell = grid->grid + curr_idx;

        if (cell->revealed) continue;

        cell->revealed = true;

        if (cell->neighbours == 0) {
            int x = curr_idx % w;
            int y = curr_idx / w;

            vec2i neighbours[] = {
                {x,y+1},
                {x,y-1},
                {x+1,y},
                {x-1,y},

                {x-1,y+1},
                {x+1,y-1},
                {x+1,y+1},
                {x-1,y-1},
            };

            for (int i = 0; i < StaticArraySize(neighbours); i++) {
                vec2i p = neighbours[i];

                cell_t *neighbour = get_cell(grid, p.x, p.y);

                if (in_bounds(p.x, p.y, w, h)) {
                    if (!neighbour->type == CELL_MINE && !neighbour->visited) {
                        index_queue[++tail] = p.x + p.y * w;
                    }

                    neighbour->visited = true;
                }
            }

            assert(tail < w*h);
        }
    }

    free(index_queue);
}

void grid_init(grid_t *grid, Game_Mode mode, int width, int height, int num_bombs, int num_fairies) {
    panel_t *panel = &grid->panel;

    panel->width  = 0.8f;
    panel->height = panel->width;
    panel->x      = (1 - panel->width) / 2.f;
    panel->y      = (1 - panel->width) / 2.f;

    /////

    grid->game_mode = mode;
    grid->total_mistakes = 2;

    grid->num_bombs = num_bombs;
    grid->num_fairies = num_fairies;

    grid->width = WIDTH;
    grid->height = HEIGHT;
    grid->grid = calloc(grid->width * grid->height, sizeof(cell_t));

    assert(num_bombs < grid->width * grid->height);
}

// returns 0 if game over, 1 if good, 2 if mistook a flag
int grid_tick(panel_t *panel, grid_t *grid) {
    int result = 1;

    Vector2 mouse = GetMousePosition();

    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            cell_t *cell = grid->grid + (x + y * grid->width);

            if (panel->size_changed) {
                cell->rect = get_cell_rect(panel->actual, grid, x, y);
            }

            cell->highlighted = CheckCollisionPointRec(mouse, cell->rect);

            if (cell->highlighted) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_SPACE)) {
                    click(grid, x, y);

                    if (cell->type != CELL_CLEAR) {
                        result = 0;
                        goto after_loop;
                    }
                } else if (grid->is_initialized && !cell->revealed) {
                    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_B)) {
                        if (cell->flag != FLAG_MINE) {
                            cell->flag = FLAG_MINE;

                            if (cell->type != CELL_MINE) {
                                result = 2;
                                goto after_loop;
                            }
                        } else {
                            cell->flag = FLAG_NONE;
                        }
                    } else if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) || IsKeyPressed(KEY_F)) {
                        if (cell->flag != FLAG_FAIRY) {
                            cell->flag = FLAG_FAIRY;

                            if (cell->type != CELL_FAIRY) {
                                result = 2;
                                goto after_loop;
                            }
                        } else {
                            cell->flag = FLAG_NONE;
                        }
                    }
                }
            }
        }
    }

after_loop:
    grid->num_flags = 0;
    grid->num_seen_fairies = 0;

    for (int i = 0; i < grid->width * grid->height; i++) {
        switch (grid->grid[i].flag) {
            case FLAG_FAIRY: grid->num_seen_fairies++; break;
            case FLAG_MINE:  grid->num_flags++; break;
        }
    }

    return result;
}

void draw_texture_on_cell(Rectangle rect, Texture texture, bool flash) {
    float size = rect.height * 3.f / 4.f;

    Vector2 pos = Vector2Add((Vector2){rect.x,rect.y}, (Vector2){rect.width/2,rect.height/2});

    pos.x -= size/2;
    pos.y -= size/2;

    Rectangle src = { 0, 0, texture.width, texture.height };
    Rectangle dst = { pos.x, pos.y, size, size };

    Color c = WHITE;

    if (flash) {
        c = (int)(10*GetTime()) % 2 == 0 ? WHITE : BLACK;
    }

    DrawTexturePro(texture, src, dst, (Vector2){}, 0, c);
}

void grid_draw(assets_t *assets, grid_t *grid, panel_t *screen) {
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            cell_t *cell = grid->grid + (x + y * grid->width);

            Rectangle rect = smallen(cell->rect, 5);

            const float roundness = 0.1f;

            Color main_col, outline_col;

            if (!cell->revealed) {
                main_col = GetColor(COLOR_UNKNOWN_CELL);
                outline_col = GetColor(COLOR_UNKNOWN_CELL_OUTLINE);
            } else {
                main_col = GetColor(COLOR_EMPTY_CELL);
                outline_col = GetColor(COLOR_UNKNOWN_CELL_OUTLINE);
            }

            if (cell->highlighted) {
                int darken = 10;

                main_col.r -= darken;
                main_col.g -= darken;
                main_col.b -= darken;

                outline_col.r -= darken;
                outline_col.g -= darken;
                outline_col.b -= darken;
            }

            DrawRectangleRounded(rect, roundness, 5, main_col);
            DrawRectangleRoundedLinesEx(rect, roundness, 5, 3, outline_col);

            if (cell->revealed) {
                if (cell->type) {
                    switch (cell->type) {
                        case CELL_MINE:
                            draw_texture_on_cell(rect, assets->bomb, false);
                            break;
                        case CELL_FAIRY:
                            draw_texture_on_cell(rect, assets->fairy, false);
                            break;
                    }
                } else if (cell->neighbours) {
                    const char *str = 0;

                    if ((int)(cell->neighbours * 2) % 2 == 0) {
                        str = TextFormat("%d", (int)cell->neighbours);
                    } else {
                        str = TextFormat("%.1f", cell->neighbours);
                    }

                    int size = rect.height * 3.f / 5.f;

                    Vector2 wh = MeasureTextEx(assets->font, str, size, 0);

                    Vector2 pos = Vector2Add((Vector2){rect.x,rect.y}, (Vector2){rect.width/2,rect.height/2});
                    pos.x -= wh.x / 2;
                    pos.y -= wh.y / 2;

                    int col = COLOR_TEXT;

                    if (cell->has_fairy_neighbours)
                        col = COLOR_TEXT_FAIRY;

                    DrawTextEx(assets->font, str, pos, size, 0, GetColor(col));
                }
            } else if (cell->flag) {
                bool flash = false;

                if (cell->flag == FLAG_MINE && cell->type != CELL_MINE)
                    flash = true;

                if (cell->flag == FLAG_FAIRY && cell->type != CELL_FAIRY)
                    flash = true;

                switch (cell->flag) {
                    case FLAG_FAIRY: draw_texture_on_cell(rect, assets->fairy_flag, flash); break;
                    case FLAG_MINE:  draw_texture_on_cell(rect, assets->flag, flash); break;
                }
            }
        }
    }

    Rectangle out = grid->panel.actual;
    out = smallen(out, -5);
    DrawRectangleLinesEx(out, 4, (Color){255,0,0,64});

    {
        float font_size = grid->panel.y * 0.5f * screen->actual.width;
        const char *text = TextFormat("Bombs: %d/%d    Fairies: %d/%d    Mistakes: %d/%d", grid->num_flags, grid->num_bombs, grid->num_seen_fairies, grid->num_fairies, grid->mistakes, grid->total_mistakes);
        draw_text_centered(assets->font, font_size, text, 0.5f, grid->panel.y / 2.f, WHITE, screen);
    }
}

void app_init(app_t *app) {
    srand(time(0));

    int window_width = 1000;
    int window_height = 1000;

    int num_bombs = NUM_BOMBS;

    SetTraceLogLevel(LOG_ERROR);
    InitWindow(window_width, window_height, "Minesweeper Deluxe: PREPARE TO BLEED!");
    SetWindowState(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);

    app->assets.font = LoadFontEx("DroidSans.ttf", 100, nullptr, 0);
    app->assets.flag = LoadTexture("flag.png");
    app->assets.bomb = LoadTexture("bomb.png");
    app->assets.fairy = LoadTexture("fairy.png");
    app->assets.fairy_flag = LoadTexture("fairy_flag.png");

    app->screen.x = 0;
    app->screen.y = 0;
    app->screen.width = 1;
    app->screen.height = 1;

    assert(IsTextureValid(app->assets.flag));
    assert(IsFontValid(app->assets.font));

    grid_init(&app->grid, GAME_MODE_FAIRY, WIDTH, HEIGHT, num_bombs, NUM_FAIRIES);
}

void tick_screen_panel(panel_t *screen, int render_width, int render_height) {
    Rectangle result = {};

    if (render_width >= render_height) {
        result.x = render_width / 2 - render_height / 2;
        result.y = 0;
        result.width = render_height;
        result.height = render_height;
    } else {
        result.x = 0;
        result.y = render_height / 2 - render_width / 2;
        result.width = render_width;
        result.height = render_width;
    }

    screen->size_changed = !compare_rects(screen->actual, result);

    if (screen->size_changed) {
        screen->actual = result;
    }
}

void panel_tick(panel_t *panel, panel_t *screen) {
    panel->size_changed = screen->size_changed;

    if (screen->size_changed) {
        panel->actual.x      = panel->x * screen->actual.width;
        panel->actual.y      = panel->y * screen->actual.height;
        panel->actual.width  = panel->width * screen->actual.width;
        panel->actual.height = panel->height * screen->actual.height;

        panel->actual.x += screen->actual.x;
        panel->actual.y += screen->actual.y;
    }
}

void app_tick(app_t *app) {
    tick_screen_panel(&app->screen, GetRenderWidth(), GetRenderHeight());
    panel_tick(&app->grid.panel, &app->screen);

    /*
    if (IsKeyPressed(KEY_EQUAL)) {
        app->grid.total_mistakes++; // cheat ;)
    }
    */

    int result = grid_tick(&app->grid.panel, &app->grid);

    bool game_over = (result == 0);

    if (result == 2) {
        app->grid.mistakes++;

        if (app->grid.mistakes > app->grid.total_mistakes) {
            game_over = true;
        }
    }

    if (game_over) {
        app->game_over = true;
        for (int i = 0; i < app->grid.width * app->grid.height; i++) {
            app->grid.grid[i].revealed = true;
        }
    }
}

void app_draw(app_t *app) {
    ClearBackground(BLACK);
    DrawRectangleRec(app->screen.actual, (Color){16,0,0,255});
    grid_draw(&app->assets, &app->grid, &app->screen);
}

void app_cleanup(app_t *app) {
    free(app->grid.grid);

    UnloadFont(app->assets.font);
    UnloadTexture(app->assets.flag);
    UnloadTexture(app->assets.bomb);

    CloseWindow();
}

int main(void) {
    app_t app = {};
    app_init(&app);

    while (!WindowShouldClose()) {
        app_tick(&app);
        BeginDrawing();
        app_draw(&app);
        EndDrawing();
    }

    app_cleanup(&app);
}
