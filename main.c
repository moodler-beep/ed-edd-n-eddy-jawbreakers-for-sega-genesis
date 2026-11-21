#include <gba.h>
#include <stdbool.h>
#include <stdint.h>

#define SCREEN_W 240
#define SCREEN_H 160
#define TARGET_GEMS 10
#define MAX_LIVES 3

#define COLOR(r, g, b) ((u16)(((r) & 31) | (((g) & 31) << 5) | (((b) & 31) << 10)))

static const u16 sky = COLOR(23, 27, 31);
static const u16 dark = COLOR(3, 5, 10);
static const u16 cream = COLOR(31, 29, 21);
static const u16 teal = COLOR(5, 24, 25);
static const u16 green = COLOR(10, 28, 12);
static const u16 gold = COLOR(31, 23, 5);
static const u16 red = COLOR(30, 7, 7);
static const u16 pink = COLOR(31, 12, 19);

static const u8 font[36][7] = {
    {0, 0, 0, 0, 0, 0, 0},
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
    {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30},
    {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
    {14, 4, 4, 4, 4, 4, 14}, {7, 2, 2, 2, 18, 18, 12},
    {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
    {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
    {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30}, {31, 4, 4, 4, 4, 4, 4},
    {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},
    {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4}, {31, 1, 2, 4, 8, 16, 31},
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
    {14, 17, 1, 2, 4, 8, 31}, {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2}, {31, 16, 16, 30, 1, 1, 30},
    {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
    {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14},
};

static const char glyphs[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static u16 *const framebuffer = (u16 *)VRAM;

static void wait_vblank(void) {
    while (REG_VCOUNT >= 160) {
    }
    while (REG_VCOUNT < 160) {
    }
}

static void clear_screen(u16 color) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
        framebuffer[i] = color;
    }
}

static void rect(int x, int y, int width, int height, u16 color) {
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (x + width > SCREEN_W) width = SCREEN_W - x;
    if (y + height > SCREEN_H) height = SCREEN_H - y;
    if (width <= 0 || height <= 0) return;

    for (int row = 0; row < height; ++row) {
        u16 *line = framebuffer + (y + row) * SCREEN_W + x;
        for (int col = 0; col < width; ++col) line[col] = color;
    }
}

static void text(int x, int y, const char *value, u16 color, int scale) {
    for (; *value; ++value, x += 6 * scale) {
        char c = *value;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        int index = 0;
        while (glyphs[index] && glyphs[index] != c) ++index;
        if (!glyphs[index]) continue;
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (font[index][row] & (1u << (4 - col))) {
                    rect(x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
    }
}

typedef struct {
    int x;
    int y;
    int speed;
    bool bomb;
    bool active;
} Item;

static int basket_x = 104;
static int gems = 0;
static int lives = MAX_LIVES;
static int frame = 0;
static uint32_t random_state = 0xC0FFEEu;
static Item item;

typedef enum { TITLE, PLAYING, WON, LOST } GameState;
static GameState state = TITLE;

static uint32_t next_random(void) {
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}

static void reset_game(void) {
    basket_x = 104;
    gems = 0;
    lives = MAX_LIVES;
    frame = 0;
    item.active = false;
    state = PLAYING;
}

static void draw_item(void) {
    if (!item.active) return;
    if (item.bomb) {
        rect(item.x + 3, item.y, 8, 10, dark);
        rect(item.x, item.y + 3, 14, 7, dark);
        rect(item.x + 4, item.y + 3, 2, 2, pink);
        rect(item.x + 9, item.y + 3, 2, 2, pink);
    } else {
        rect(item.x + 5, item.y, 6, 4, cream);
        rect(item.x + 2, item.y + 3, 12, 7, gold);
        rect(item.x + 5, item.y + 10, 6, 4, cream);
        rect(item.x + 6, item.y + 5, 4, 3, pink);
    }
}

static void draw_hud(void) {
    rect(0, 0, SCREEN_W, 24, dark);
    text(8, 8, "GEMS", cream, 1);
    rect(42, 8, TARGET_GEMS * 9, 8, teal);
    for (int i = 0; i < gems; ++i) rect(44 + i * 9, 10, 6, 4, gold);
    text(158, 8, "LIVES", cream, 1);
    for (int i = 0; i < lives; ++i) {
        rect(193 + i * 13, 8, 9, 8, red);
        rect(195 + i * 13, 6, 5, 3, pink);
    }
}

static void draw_playfield(void) {
    clear_screen(sky);
    rect(0, 24, SCREEN_W, 1, cream);
    rect(0, 145, SCREEN_W, 15, green);
    rect(0, 145, SCREEN_W, 2, teal);
    draw_hud();
    draw_item();
    rect(basket_x, 132, 32, 5, cream);
    rect(basket_x + 2, 137, 28, 6, gold);
    rect(basket_x + 5, 139, 22, 2, red);
}

static void spawn_item(void) {
    item.x = 10 + (int)(next_random() % (SCREEN_W - 24));
    item.y = 31;
    item.speed = 1 + gems / 4;
    item.bomb = (next_random() % 5u) == 0u;
    item.active = true;
}

static void update_game(u16 keys) {
    if (keys & KEY_LEFT) basket_x -= 3;
    if (keys & KEY_RIGHT) basket_x += 3;
    if (basket_x < 0) basket_x = 0;
    if (basket_x > SCREEN_W - 32) basket_x = SCREEN_W - 32;

    if (!item.active) {
        if (++frame >= 30) {
            frame = 0;
            spawn_item();
        }
        return;
    }

    item.y += item.speed;
    const bool overlaps = item.x + 14 >= basket_x && item.x <= basket_x + 32;
    if (item.y + 14 >= 132 && overlaps) {
        if (item.bomb) {
            --lives;
        } else if (++gems >= TARGET_GEMS) {
            state = WON;
        }
        item.active = false;
    } else if (item.y > 145) {
        if (!item.bomb) --lives;
        item.active = false;
    }

    if (lives <= 0) state = LOST;
}

static void draw_title(void) {
    clear_screen(sky);
    rect(0, 116, SCREEN_W, 44, green);
    rect(0, 116, SCREEN_W, 3, teal);
    text(48, 34, "POCKET", gold, 3);
    text(42, 62, "CATCH", cream, 3);
    text(72, 101, "GBA GAME", cream, 1);
    rect(95, 125, 50, 8, gold);
    rect(99, 127, 42, 4, cream);
    text(54, 143, "PRESS START", cream, 1);
}

static void draw_end_screen(bool won) {
    clear_screen(won ? teal : dark);
    text(won ? 66 : 72, 43, won ? "YOU WIN" : "GAME OVER", won ? gold : pink, 2);
    text(60, 75, "GEMS", cream, 1);
    for (int i = 0; i < gems; ++i) rect(94 + i * 10, 75, 7, 7, gold);
    text(54, 111, "PRESS START", cream, 1);
}

int main(void) {
    REG_DISPCNT = MODE3 | BG2_ENABLE;

    while (1) {
        wait_vblank();
        scanKeys();
        const u16 keys = keysHeld();

        if (state == TITLE) {
            draw_title();
            if (keys & KEY_START) reset_game();
        } else if (state == PLAYING) {
            update_game(keys);
            draw_playfield();
        } else {
            draw_end_screen(state == WON);
            if (keys & KEY_START) reset_game();
        }
    }
}
