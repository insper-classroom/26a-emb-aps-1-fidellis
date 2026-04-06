#include "genius.h"
#include "feedback.h"
#include "pico/multicore.h"
#include <stdlib.h>
#include <stdio.h>

static uint8_t sequence[MAX_SEQUENCE];
static int current_level = 0;
static int step_index = 0;
static game_state_t state = STATE_WAITING_START;
static int difficulty = LEVEL_EASY;

static uint8_t random_step() {
    uint8_t step = 0;
    int base_color = rand() % 4;
    step = (1 << base_color);

    // No nível Dificil, há 30% de chance de acender 2 leds ao mesmo tempo
    if (difficulty == LEVEL_HARD && (rand() % 100) < 30) {
        int second_color = rand() % 4;
        while (second_color == base_color) {
            second_color = rand() % 4;
        }
        step |= (1 << second_color);
    }
    return step;
}

void genius_core1_main() {
    multicore_fifo_push_blocking(MSG_GAME_READY);
    
    while(true) {
        switch(state) {
            case STATE_WAITING_START:
                if (multicore_fifo_rvalid()) {
                    uint32_t cmd = multicore_fifo_pop_blocking();
                    if (cmd == CMD_START_GAME) {
                        uint32_t seed = multicore_fifo_pop_blocking();
                        difficulty = multicore_fifo_pop_blocking();
                        srand(seed);
                        
                        current_level = 0;
                        sequence[current_level] = random_step();
                        state = STATE_PLAYING_SEQ;
                    }
                }
                break;
                
            case STATE_PLAYING_SEQ:
                sleep_ms(500); // small delay before sequence
                
                int time_ms = 1000;
                if (difficulty == LEVEL_MEDIUM) time_ms = 700;
                else if (difficulty == LEVEL_HARD) time_ms = 500;
                
                for (int i = 0; i <= current_level; i++) {
                    play_feedback_mask(sequence[i], time_ms);
                    sleep_ms(200); // gap between notes
                }
                
                step_index = 0;
                state = STATE_WAITING_INPUT;
                flush_button_inputs(); // Clear any inputs during play
                break;
                
            case STATE_WAITING_INPUT: {
                uint8_t current_expected_mask = sequence[step_index];
                uint8_t user_input = get_user_input_blocking(current_expected_mask); 
                
                if (user_input == current_expected_mask) {
                    // Certo
                    play_feedback_mask(user_input, 300);
                    step_index++;
                    
                    if (step_index > current_level) {
                        // Passou de nivel
                        multicore_fifo_push_blocking(MSG_UPDATE_SCORE);
                        multicore_fifo_push_blocking(current_level + 1);
                        
                        current_level++;
                        if (current_level >= MAX_SEQUENCE) {
                            // Win? Or endless
                        } else {
                            sequence[current_level] = random_step();
                            sleep_ms(500);
                            state = STATE_PLAYING_SEQ;
                        }
                    }
                } else {
                    // Errou
                    state = STATE_GAME_OVER;
                }
                break;
            }
                
            case STATE_GAME_OVER:
                play_game_over_sound();
                multicore_fifo_push_blocking(MSG_GAME_OVER);
                state = STATE_WAITING_START;
                break;
        }
    }
}
