#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "tft_lcd_ili9341/gfx/gfx_ili9341.h"
#include "tft_lcd_ili9341/ili9341/ili9341.h"
#include "tft_lcd_ili9341/touch_resistive/touch_resistive.h"

#include "feedback.h"               //Header com as definições de LEDs, Botões e Sons

#include "genius.h"               //Header com a lógica do jogo

// Propriedades do LCD
#define SCREEN_ROTATION 1           // 0 = RETRATO, 1 = PAISAGEM
const int width = 320;             // Variável global definida em gfx_ili9341.c que armazena a largura da tela
const int height = 240;            // Variável global definida em gfx_ili9341.c que armazena a altura da tela

typedef enum {
    UI_START,
    UI_SELECT_DIFFICULTY,
    UI_PLAYING,
    UI_RESULT
} ui_state_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} rect_t;

static const rect_t BTN_START = {90, 100, 140, 50};
static const rect_t BTN_EASY = {20, 110, 85, 50};
static const rect_t BTN_MEDIUM = {117, 110, 85, 50};
static const rect_t BTN_HARD = {214, 110, 85, 50};
static const rect_t BTN_RESTART = {80, 160, 160, 45};

static bool point_in_rect(int x, int y, rect_t r) {
    return x >= r.x && x <= (r.x + r.w) && y >= r.y && y <= (r.y + r.h);
}

static const char *difficulty_name(int difficulty) {
    if (difficulty == LEVEL_EASY) return "FACIL";
    if (difficulty == LEVEL_MEDIUM) return "MEDIO";
    return "DIFICIL";
}

static void draw_button(rect_t r, const char *label) {
    gfx_drawRect(r.x, r.y, r.w, r.h, 0xFFFF, 2);
    gfx_setTextSize(2);
    gfx_drawText(r.x + 12, r.y + (r.h / 2) - 8, label);
}

static void draw_start_screen(void) {
    gfx_clear();
    gfx_setTextColor(0xFFFF);
    gfx_setTextSize(3);
    gfx_drawText(95, 40, "GENIUS");
    gfx_setTextSize(1);
    gfx_drawText(75, 80, "Toque para iniciar o jogo");
    draw_button(BTN_START, "START");
}

static void draw_difficulty_screen(void) {
    gfx_clear();
    gfx_setTextColor(0xFFFF);
    gfx_setTextSize(2);
    gfx_drawText(42, 55, "ESCOLHA DIFICULDADE");
    draw_button(BTN_EASY, "FACIL");
    draw_button(BTN_MEDIUM, "MEDIO");
    draw_button(BTN_HARD, "DIFICIL");
}

static void draw_game_screen(int score, int difficulty) {
    char line[32];
    gfx_clear();
    gfx_setTextColor(0xFFFF);
    gfx_setTextSize(2);
    snprintf(line, sizeof(line), "NIVEL: %s", difficulty_name(difficulty));
    gfx_drawText(20, 35, line);
    snprintf(line, sizeof(line), "SCORE: %d", score);
    gfx_drawText(20, 85, line);
    gfx_setTextSize(1);
    gfx_drawText(20, 140, "Repita a sequencia nos botoes fisicos");
}

static void draw_result_screen(bool victory, int score) {
    char line[32];
    gfx_clear();
    gfx_setTextColor(0xFFFF);
    gfx_setTextSize(2);
    if (victory) {
        gfx_drawText(110, 55, "VOCE VENCEU!");
    } else {
        gfx_drawText(95, 55, "GAME OVER");
    }
    snprintf(line, sizeof(line), "SCORE FINAL: %d", score);
    gfx_drawText(75, 105, line);
    draw_button(BTN_RESTART, "RECOMECAR");
}

static uint32_t make_seed_from_touch(int x, int y) {
    uint32_t t = time_us_32();
    uint32_t seed = t ^ ((uint32_t)x << 16) ^ ((uint32_t)y << 1);

    seed ^= (seed >> 16);
    seed *= 0x7feb352d;
    seed ^= (seed >> 15);

    return seed;
}

void core_1_entry() {
    genius_core1_main(); // O Core 1 agora roda exclusivamente a logica do genius
}

int main() {
    stdio_init_all();

    multicore_launch_core1(core_1_entry);

    // Inicializa LEDs, Botões (com interrupção) e Buzzer
    init_feedback_hardware();
    LCD_initDisplay();
    LCD_setRotation(SCREEN_ROTATION);   // Ajusta a rotação da tela conforme definido

    //### TOUCH
    configure_touch();                  // Configura o touch resistivo

    //### GFX
    gfx_init();                         // Inicializa o GFX
    gfx_setTextColor(0xFFFF);
    draw_start_screen();

    ui_state_t ui_state = UI_START;
    bool touchWasDown = false;
    bool isPlaying = false;
    bool victory = false;
    int current_score = 0;
    int selected_difficulty = LEVEL_MEDIUM;

    while (true) {
        // Verifica mensagens do Genius (Core 1 -> Core 0)
        if (multicore_fifo_rvalid()) {
            uint32_t msg = multicore_fifo_pop_blocking();
            if (msg == MSG_GAME_OVER) {
                isPlaying = false;
                victory = false;
                ui_state = UI_RESULT;
                draw_result_screen(victory, current_score);
            } else if (msg == MSG_UPDATE_SCORE) {
                uint32_t val = multicore_fifo_pop_blocking();
                current_score = (int)val;
                if (ui_state == UI_PLAYING) {
                    draw_game_screen(current_score, selected_difficulty);
                }
            } else if (msg == MSG_GAME_WIN) {
                isPlaying = false;
                victory = true;
                ui_state = UI_RESULT;
                draw_result_screen(victory, current_score);
            }
        }

        // Gerencia feedbacks (som, led) de formar não bloqueante
        process_feedback();

        int touchRawX, touchRawY;               // Variaveis para armazenar as coordenadas brutas do toque
        int screenTouchX = 0, screenTouchY = 0; // Variaveis para armazenar as coordenadas do toque transformadas para a tela

        int touchDetected = readPoint(&touchRawX, &touchRawY);  // Lê as coordenadas do toque e armazena em touchRawX e touchRawY,
                                                                // a função retorna 1 se um toque for detectado ou 0 caso contrário

        if (touchDetected) {
            gfx_touchTransform(SCREEN_ROTATION,
                               touchRawX, touchRawY,
                               &screenTouchX, &screenTouchY);

            if (!touchWasDown) {
                if (ui_state == UI_START) {
                    if (point_in_rect(screenTouchX, screenTouchY, BTN_START)) {
                        ui_state = UI_SELECT_DIFFICULTY;
                        draw_difficulty_screen();
                    }
                } else if (ui_state == UI_SELECT_DIFFICULTY && !isPlaying) {
                    bool choose_easy = point_in_rect(screenTouchX, screenTouchY, BTN_EASY);
                    bool choose_medium = point_in_rect(screenTouchX, screenTouchY, BTN_MEDIUM);
                    bool choose_hard = point_in_rect(screenTouchX, screenTouchY, BTN_HARD);

                    if (choose_easy || choose_medium || choose_hard) {
                        uint32_t seed = make_seed_from_touch(screenTouchX, screenTouchY);
                        selected_difficulty = choose_easy ? LEVEL_EASY : (choose_medium ? LEVEL_MEDIUM : LEVEL_HARD);
                        current_score = 0;
                        victory = false;
                        isPlaying = true;
                        ui_state = UI_PLAYING;
                        draw_game_screen(current_score, selected_difficulty);

                        multicore_fifo_push_blocking(CMD_START_GAME);
                        multicore_fifo_push_blocking(seed);
                        multicore_fifo_push_blocking((uint32_t)selected_difficulty);
                    }
                } else if (ui_state == UI_RESULT) {
                    if (point_in_rect(screenTouchX, screenTouchY, BTN_RESTART)) {
                        ui_state = UI_SELECT_DIFFICULTY;
                        draw_difficulty_screen();
                    }
                }
            }
        }
        touchWasDown = touchDetected;

        sleep_ms(50);
    }

    return 0;
}