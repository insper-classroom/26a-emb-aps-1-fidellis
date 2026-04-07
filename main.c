#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"

#include "tft_lcd_ili9341/gfx/gfx_ili9341.h"
#include "tft_lcd_ili9341/ili9341/ili9341.h"
#include "tft_lcd_ili9341/touch_resistive/touch_resistive.h"

#include "image_bitmap.h"           //Header com os bitmaps dos botões
#include "feedback.h"               //Header com as definições de LEDs, Botões e Sons

#include "genius.h"               //Header com a lógica do jogo

// Propriedades do LCD
#define SCREEN_ROTATION 1           // 0 = RETRATO, 1 = PAISAGEM
const int width = 320;             // Variável global definida em gfx_ili9341.c que armazena a largura da tela
const int height = 240;            // Variável global definida em gfx_ili9341.c que armazena a altura da tela

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
    gfx_clear();                        // Limpa a tela

    gfx_setTextSize(2);                 // Define o tamanho do texto
    gfx_setTextColor(0x07E0);           // Define a cor do texto (verde)

    gfx_drawText(
        width/6,                        // Posição horizontal do texto
        10,                             // Posição vertical do texto
        "GENIUS"                      // Texto a ser exibido
    );

    gfx_setTextSize(1);
    gfx_drawText(20, 60, "ESQ: MEDIO");
    gfx_drawText(235, 60, "DIR: DIFICIL");

    // Desenha os botões estáticos esquerdo e direito
    gfx_drawBitmap(26,  77, image_left_btn,  53, 77, 0xFFFF);
    gfx_drawBitmap(247, 70, image_right_btn, 61, 90, 0xFFFF);

    bool touchWasDown = false;
    bool isPlaying = false; // Indica se estamos jogando

    while (true) {
        // Verifica mensagens do Genius
        if (multicore_fifo_rvalid()) {
            uint32_t msg = multicore_fifo_pop_blocking();
            if (msg == MSG_GAME_OVER) {
                isPlaying = false;
                gfx_fillRect(0, 0, width, height, 0x0000);
                gfx_drawText(width/6, 120, "GAME OVER");
            } else if (msg == MSG_UPDATE_SCORE) {
                uint32_t val = multicore_fifo_pop_blocking();
                char str[20];
                snprintf(str, sizeof(str), "Score: %lu", val);
                gfx_fillRect(0, 0, width, 50, 0x0000);
                gfx_drawText(20, 10, str);
            }
        }
        // Gerencia feedbacks (som, led) de formar não bloqueante
        process_feedback();

        int touchRawX, touchRawY;               // Variaveis para armazenar as coordenadas brutas do toque
        int screenTouchX = 0, screenTouchY = 0; // Variaveis para armazenar as coordenadas do toque transformadas para a tela

        int touchDetected = readPoint(&touchRawX, &touchRawY);  // Lê as coordenadas do toque e armazena em touchRawX e touchRawY,
                                                                // a função retorna 1 se um toque for detectado ou 0 caso contrário

        if (touchDetected) {
            gfx_touchTransform(SCREEN_ROTATION,                 // Se um toque for detectado, transforma as coordenadas brutas do toque
                               touchRawX, touchRawY,            // para as coordenadas da tela considerando a rotação
                               &screenTouchX, &screenTouchY);

            // Dispara apenas no início do toque para evitar múltiplos eventos por pressão.
            if (!touchWasDown) {
                bool onLeft =
                    screenTouchX >= 26 && screenTouchX <= (26 + 53) &&
                    screenTouchY >= 77 && screenTouchY <= (77 + 77);

                bool onRight =
                    screenTouchX >= 247 && screenTouchX <= (247 + 61) &&
                    screenTouchY >= 70 && screenTouchY <= (70 + 90);

                if (onLeft) {
                    if (!isPlaying) {
                        uint32_t seed = make_seed_from_touch(screenTouchX, screenTouchY);

                        isPlaying = true;
                        multicore_fifo_push_blocking(CMD_START_GAME);
                        multicore_fifo_push_blocking(seed);
                        multicore_fifo_push_blocking(LEVEL_MEDIUM);
                    }
                }
                else if (onRight) {
                    if (!isPlaying) {
                        uint32_t seed = make_seed_from_touch(screenTouchX, screenTouchY);

                        isPlaying = true;
                        multicore_fifo_push_blocking(CMD_START_GAME);
                        multicore_fifo_push_blocking(seed);
                        multicore_fifo_push_blocking(LEVEL_HARD);
                    }
                }
            }
        }
        touchWasDown = touchDetected;

        sleep_ms(50);    // 1ms por iteração = motor no dobro da velocidade
    }

    return 0;
}
