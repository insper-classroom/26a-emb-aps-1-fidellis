#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "tft_lcd_ili9341/gfx/gfx_ili9341.h"
#include "tft_lcd_ili9341/ili9341/ili9341.h"
#include "tft_lcd_ili9341/touch_resistive/touch_resistive.h"

#include "image_bitmap.h"           //Header com os bitmaps dos botões

// Propriedades do LCD
#define SCREEN_ROTATION 1           // 0 = RETRATO, 1 = PAISAGEM
const int width = 320;             // Variável global definida em gfx_ili9341.c que armazena a largura da tela
const int height = 240;            // Variável global definida em gfx_ili9341.c que armazena a altura da tela

// Pinos do motor de passo
#define A 3
#define B 7
#define C 8
#define D 9

// Sequência full-step
const int passos[4][4] = {
    {1, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 1},
    {1, 0, 0, 1},
};

#define MOTOR_INTERVAL 1   // 1 * 1ms = 1ms por passo
#define ANIM_INTERVAL 4    // 4 * 1ms por frame

// Avança o motor um passo na direção indicada
void step_motor(int direction, int *motorStep) {
    if (direction == 1)                     // esquerda = anti-horário
        *motorStep = (*motorStep + 1) % 4;
    else if (direction == 2)               // direita = horário
        *motorStep = (*motorStep + 3) % 4;   // equivale a -1 mod 4

    gpio_put(A, passos[*motorStep][0]);
    gpio_put(B, passos[*motorStep][1]);
    gpio_put(C, passos[*motorStep][2]);
    gpio_put(D, passos[*motorStep][3]);
}

// Desliga todas as bobinas do motor
void stop_motor() {
    gpio_put(A, 0); gpio_put(B, 0);
    gpio_put(C, 0); gpio_put(D, 0);
}

// Desenha a animação do botão esquerdo conforme o frame
void drawLeftAnim(int state) {
    // Limpa área que cobre os dois frames da esquerda
    gfx_fillRect(122, 67, 70, 85, 0x0000);

    if (state == 0)
        gfx_drawBitmap(126, 67,  image_left_anim_1, 67, 26, 0xFFFF);
    else
        gfx_drawBitmap(122, 121, image_left_anim_2, 68, 27, 0xFFFF);
}

// Desenha a animação do botão direito conforme o frame
void drawRightAnim(int state) {
    // Limpa área que cobre os dois frames da direita
    gfx_fillRect(129, 67, 70, 80, 0x0000);

    if (state == 0)
        gfx_drawBitmap(129, 67,  image_right_anim_1, 67, 34, 0xFFFF);
    else
        gfx_drawBitmap(130, 114, image_right_anim_2, 67, 32, 0xFFFF);
}

int main() {
    stdio_init_all();

    //### Motor de passo
    gpio_init(A); gpio_set_dir(A, GPIO_OUT);
    gpio_init(B); gpio_set_dir(B, GPIO_OUT);
    gpio_init(C); gpio_set_dir(C, GPIO_OUT);
    gpio_init(D); gpio_set_dir(D, GPIO_OUT);

    //### LCD
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
        "Motorzao"                    // Texto a ser exibido
    );

    // Desenha os botões estáticos esquerdo e direito
    gfx_drawBitmap(26,  77, image_left_btn,  53, 77, 0xFFFF);
    gfx_drawBitmap(247, 70, image_right_btn, 61, 90, 0xFFFF);

    int motorStep = 0;
    int motorTick = 0;
    int animDirection = 0;   // 0 = parado, 1 = esquerda, 2 = direita
    int animTick = 0;
    int leftAnimState = 0;
    int rightAnimState = 0;
    bool touchWasDown = false;

    while (true) {
        int touchRawX, touchRawY;               // Variaveis para armazenar as coordenadas brutas do toque
        int screenTouchX, screenTouchY  = 0;    // Variaveis para armazenar as coordenadas do toque transformadas para a tela

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

                if (onLeft)
                    animDirection = 1;
                else if (onRight)
                    animDirection = 2;
            }
        }
        touchWasDown = touchDetected;

        // Controle não-bloqueante do motor (avança 1 passo por iteração)
        motorTick++;
        if (motorTick >= MOTOR_INTERVAL) {
            motorTick = 0;
            if (animDirection != 0)
                step_motor(animDirection, &motorStep);  // gira na direção ativa
            else
                stop_motor();               // para quando animDirection == 0
        }

        // Controle da animação (alterna frames no intervalo definido)
        animTick++;
        if (animTick >= ANIM_INTERVAL) {
            animTick = 0;

            if (animDirection == 1) {
                leftAnimState = !leftAnimState;     // alterna frame automaticamente
                drawLeftAnim(leftAnimState);
            } else if (animDirection == 2) {
                rightAnimState = !rightAnimState;   // alterna frame automaticamente
                drawRightAnim(rightAnimState);
            }
        }

        sleep_ms(50);    // 1ms por iteração = motor no dobro da velocidade
    }

    return 0;
}
