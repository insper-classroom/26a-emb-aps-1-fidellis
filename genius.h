#ifndef GENIUS_H
#define GENIUS_H

#include "pico/stdlib.h"

// Níveis de dificuldade
#define LEVEL_EASY 0
#define LEVEL_MEDIUM 1
#define LEVEL_HARD 2

// Tamanho máximo do jogo
#define MAX_SEQUENCE 100

// Bitmasks para os botões/cores
#define COLOR_BLUE   (1 << 0)
#define COLOR_RED    (1 << 1)
#define COLOR_GREEN  (1 << 2)
#define COLOR_YELLOW (1 << 3)

// Estados do jogo
typedef enum {
    STATE_WAITING_START,
    STATE_PLAYING_SEQ,
    STATE_WAITING_INPUT,
    STATE_GAME_OVER
} game_state_t;

// Tipos de mensagens via FIFO para o Core 0
#define MSG_GAME_READY 0x10
#define MSG_UPDATE_SCORE 0x11
#define MSG_GAME_OVER 0x12

// Tipos de mensagens via FIFO para o Core 1
#define CMD_START_GAME 0xA0

void genius_core1_main(void);

#endif // GENIUS_H