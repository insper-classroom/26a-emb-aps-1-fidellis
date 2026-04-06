#ifndef FEEDBACK_H
#define FEEDBACK_H

#include "pico/stdlib.h"

// Pinos dos LEDs
extern const int BLUE_LED;
extern const int RED_LED;
extern const int GREEN_LED;
extern const int YELLOW_LED;

// Pinos dos botões
extern const int BLUE_BUTTON;
extern const int RED_BUTTON;
extern const int GREEN_BUTTON;
extern const int YELLOW_BUTTON;

// Pino do buzzer
extern const int BUZZER;

// Inicializa o hardware (LEDs, botões, PWM pro Buzzer)
void init_feedback_hardware(void);

// Processa as interrupções de forma não bloqueante
void process_feedback(void);

// Toca uma máscara de cores por um tempo
void play_feedback_mask(uint8_t mask, int duration_ms);

// Funcções bloqueantes de controle e leitura
void flush_button_inputs(void);
uint8_t get_user_input_blocking(uint8_t expected_mask);
void play_game_over_sound(void);

#endif // FEEDBACK_H