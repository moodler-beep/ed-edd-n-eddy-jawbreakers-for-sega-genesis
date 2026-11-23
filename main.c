#include <genesis.h>

#define SCREEN_W 320
#define SCREEN_H 224
#define TARGET_GEMS 10
#define MAX_LIVES 3

#define COL_SKY      PAL0 + 1
#define COL_DARK     PAL0 + 2
#define COL_CREAM    PAL0 + 3
#define COL_TEAL     PAL0 + 4
#define COL_GREEN    PAL0 + 5
#define COL_GOLD     PAL0 + 6
#define COL_RED      PAL0 + 7
#define COL_PINK     PAL0 + 8

typedef struct {
    s16 x;
    s16 y;
    u16 speed;
    bool bomb;
    bool active;
} Item;

typedef enum { TITLE, PLAYING, WON, LOST } GameState;

static const u16 palette[] = {
    0x0000, 0x0EEE, 0x0002, 0x0EEE, 0x0A88, 0x0480,
    0x0ED0, 0x0A00, 0x0E22
};

static int basket_x = 140;
static int gems = 0;
static int lives = MAX_LIVES;
static int frame = 0;
static u32 random_state = 0xC0FFEEu;
static Item item;
static GameState state = TITLE;

static u32 next_random(void) {
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}

static void reset_game(void) {
    basket_x = 140;
    gems = 0;
    lives = MAX_LIVES;
    frame = 0;
    item.active = FALSE;
    state = PLAYING;
}

static void spawn_item(void) {
    item.x = 10 + (s16)(next_random() % (SCREEN_W - 34));
    item.y = 32;
    item.speed = 1 + gems / 4;
    item.bomb = (next_random() % 5u) == 0u;
    item.active = TRUE;
}

static void draw_text_centered(const char *text, s16 y) {
    s16 len = strlen(text);
    s16 x = (SCREEN_W / 8 - len) / 2;
    VDP_drawText(text, x, y / 8);
}

static void draw_title(void) {
    VDP_clearPlane(BG_A, TRUE);
    VDP_drawFillRect(0, 0, SCREEN_W, SCREEN_H, COL_SKY);
    VDP_drawFillRect(0, 164, SCREEN_W, 60, COL_GREEN);
    VDP_drawFillRect(0, 164, SCREEN_W, 4, COL_TEAL);

    VDP_setTextPalette(PAL0);
    VDP_setTextPalette(PAL0);
    draw_text_centered("POCKET CATCH", 52);
    draw_text_centered("SEGA GENESIS", 92);
    VDP_drawText("PRESS START", 14, 21);
}

static void draw_hud(void) {
    VDP_drawFillRect(0, 0, SCREEN_W, 24, COL_DARK);
    VDP_drawText("GEMS", 1, 1);
    VDP_drawFillRect(48, 8, TARGET_GEMS * 10, 8, COL_TEAL);
    for (int i = 0; i < gems; ++i)
        VDP_drawFillRect(50 + i * 10, 10, 7, 4, COL_GOLD);

    VDP_drawText("LIVES", 30, 1);
    for (int i = 0; i < lives; ++i)
        VDP_drawFillRect(276 + i * 14, 8, 9, 8, COL_RED);
}

static void draw_item(void) {
    if (!item.active) return;

    if (item.bomb) {
        VDP_drawFillRect(item.x + 4, item.y, 12, 14, COL_DARK);
        VDP_drawFillRect(item.x, item.y + 5, 20, 10, COL_DARK);
        VDP_drawFillRect(item.x + 5, item.y + 6, 3, 3, COL_PINK);
        VDP_drawFillRect(item.x + 12, item.y + 6, 3, 3, COL_PINK);
    } else {
        VDP_drawFillRect(item.x + 6, item.y, 9, 5, COL_CREAM);
        VDP_drawFillRect(item.x + 3, item.y + 4, 16, 10, COL_GOLD);
        VDP_drawFillRect(item.x + 7, item.y + 14, 8, 5, COL_CREAM);
        VDP_drawFillRect(item.x + 9, item.y + 7, 5, 4, COL_PINK);
    }
}

static void draw_playfield(void) {
    VDP_clearPlane(BG_A, TRUE);
    VDP_drawFillRect(0, 24, SCREEN_W, 140, COL_SKY);
    VDP_drawFillRect(0, 24, SCREEN_W, 2, COL_CREAM);
    VDP_drawFillRect(0, 202, SCREEN_W, 22, COL_GREEN);
    VDP_drawFillRect(0, 202, SCREEN_W, 3, COL_TEAL);

    draw_hud();
    draw_item();
    VDP_drawFillRect(basket_x, 184, 44, 7, COL_CREAM);
    VDP_drawFillRect(basket_x + 3, 191, 38, 9, COL_GOLD);
    VDP_drawFillRect(basket_x + 7, 194, 30, 3, COL_RED);
}

static void update_game(u16 joy) {
    if (joy & BUTTON_LEFT) basket_x -= 4;
    if (joy & BUTTON_RIGHT) basket_x += 4;
    if (basket_x < 0) basket_x = 0;
    if (basket_x > SCREEN_W - 44) basket_x = SCREEN_W - 44;

    if (!item.active) {
        if (++frame >= 30) {
            frame = 0;
            spawn_item();
        }
        return;
    }

    item.y += item.speed;
    bool overlaps = item.x + 20 >= basket_x && item.x <= basket_x + 44;
    if (item.y + 18 >= 184 && overlaps) {
        if (item.bomb) {
            --lives;
        } else if (++gems >= TARGET_GEMS) {
            state = WON;
        }
        item.active = FALSE;
    } else if (item.y > 202) {
        if (!item.bomb) --lives;
        item.active = FALSE;
    }

    if (lives <= 0) state = LOST;
}

static void draw_end_screen(bool won) {
    VDP_clearPlane(BG_A, TRUE);
    VDP_drawFillRect(0, 0, SCREEN_W, SCREEN_H, won ? COL_TEAL : COL_DARK);
    draw_text_centered(won ? "YOU WIN" : "GAME OVER", 48);

    VDP_drawText("GEMS", 12, 10);
    for (int i = 0; i < gems; ++i)
        VDP_drawFillRect(64 + i * 12, 78, 9, 9, COL_GOLD);

    VDP_drawText("PRESS START", 13, 19);
}

int main(bool hard) {
    VDP_setScreenWidth320();
    VDP_setPalette(PAL0, palette);
    VDP_setTextPalette(PAL0);

    while (TRUE) {
        u16 joy = JOY_readJoypad(JOY_1);

        if (state == TITLE) {
            draw_title();
            if (joy & BUTTON_START) reset_game();
        } else if (state == PLAYING) {
            update_game(joy);
            draw_playfield();
        } else {
            draw_end_screen(state == WON);
            if (joy & BUTTON_START) reset_game();
        }

        SYS_doVBlankProcess();
    }

    return 0;
}
