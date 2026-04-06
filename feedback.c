#include "feedback.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// Definições dos pinos (movidas da main)
const int BLUE_LED = 15;
const int RED_LED = 14;
const int GREEN_LED = 13;
const int YELLOW_LED = 12;

const int BLUE_BUTTON = 11;
const int RED_BUTTON = 10;
const int GREEN_BUTTON = 9;
const int YELLOW_BUTTON = 8;

const int BUZZER = 7;

#include "genius.h" // For masks
// Variáveis voláteis (flags)
volatile bool btn_blue_flag = false;
volatile bool btn_red_flag = false;
volatile bool btn_green_flag = false;
volatile bool btn_yellow_flag = false;

// Estado interno do feedback
static int active_feedback = 0; // 0=Nenhum, 1=Azul, 2=Vermelho, 3=Verde, 4=Amarelo
static int feedback_timer = 0;  // Contador regressivo para o feedback (para não bloquear a main)
const int FEEDBACK_DURATION_TICKS = 6; // Tempo ativo do feddback em loops (depende do delay principal)

// Emite um tom no buzzer utilizando PWM
static void play_tone(uint pin, uint frequency) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    if (frequency == 0) {
        pwm_set_gpio_level(pin, 0); // Desliga som
        pwm_set_enabled(slice_num, false);
        return;
    }
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t divider = clock_freq / (frequency * 4096); 
    if (divider == 0) divider = 1;

    pwm_set_clkdiv(slice_num, divider);
    pwm_set_wrap(slice_num, 4095);
    pwm_set_gpio_level(pin, 2048); // Duty cycle de 50%
    pwm_set_enabled(slice_num, true);
}

// Callback de interrupção unificado
static void btn_callback(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_FALL) {
        if (gpio == BLUE_BUTTON) btn_blue_flag = true;
        else if (gpio == RED_BUTTON) btn_red_flag = true;
        else if (gpio == GREEN_BUTTON) btn_green_flag = true;
        else if (gpio == YELLOW_BUTTON) btn_yellow_flag = true;
    }
}

void init_feedback_hardware(void) {
    // Configuração dos LEDs
    const int leds[] = {BLUE_LED, RED_LED, GREEN_LED, YELLOW_LED};
    for (int i = 0; i < 4; i++) {
        gpio_init(leds[i]);
        gpio_set_dir(leds[i], GPIO_OUT);
        gpio_put(leds[i], 0); // Inicia desligado
    }

    // Configuração dos botões (com interrupções)
    const int btns[] = {BLUE_BUTTON, RED_BUTTON, GREEN_BUTTON, YELLOW_BUTTON};
    for (int i = 0; i < 4; i++) {
        gpio_init(btns[i]);
        gpio_set_dir(btns[i], GPIO_IN);
        gpio_pull_up(btns[i]);
        gpio_set_irq_enabled_with_callback(btns[i], GPIO_IRQ_EDGE_FALL, true, &btn_callback);
    }

    // Configuração do Buzzer
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
}

void process_feedback(void) {
    // Se nenhum feedback estiver rodando, verifica as flags dos botões
    if (active_feedback == 0) {
        if (btn_blue_flag) {
            active_feedback = 1;
            gpio_put(BLUE_LED, 1);
            play_tone(BUZZER, 440); // Nota A4
            btn_blue_flag = false;
        } else if (btn_red_flag) {
            active_feedback = 2;
            gpio_put(RED_LED, 1);
            play_tone(BUZZER, 523); // Nota C5
            btn_red_flag = false;
        } else if (btn_green_flag) {
            active_feedback = 3;
            gpio_put(GREEN_LED, 1);
            play_tone(BUZZER, 659); // Nota E5
            btn_green_flag = false;
        } else if (btn_yellow_flag) {
            active_feedback = 4;
            gpio_put(YELLOW_LED, 1);
            play_tone(BUZZER, 784); // Nota G5
            btn_yellow_flag = false;
        }
        
        // Se ativou algum, setar o timer
        if (active_feedback != 0) {
            feedback_timer = FEEDBACK_DURATION_TICKS;
        }
    } else {
        // Limpar fila para n ficar armazenando se o botão for pressionado enquanto o som toca
        btn_blue_flag = false;
        btn_red_flag = false;
        btn_green_flag = false;
        btn_yellow_flag = false;
    }

    // Processa o timer de desativação (sem usar sleep_ms)
    // Se o timer acabar, desliga os visuais e som
    if (active_feedback != 0) {
        feedback_timer--;
        if (feedback_timer <= 0) {
// Desliga todos os visuais e som
            gpio_put(BLUE_LED, 0);
            gpio_put(RED_LED, 0);
            gpio_put(GREEN_LED, 0);
            gpio_put(YELLOW_LED, 0);
            play_tone(BUZZER, 0); // Para o som
            
            active_feedback = 0;
        }
    }
}

void play_feedback_mask(uint8_t mask, int duration_ms) {
    if (mask & COLOR_BLUE) {
        gpio_put(BLUE_LED, 1);
        play_tone(BUZZER, 440);
    }
    if (mask & COLOR_RED) {
        gpio_put(RED_LED, 1);
        play_tone(BUZZER, 523);
    }
    if (mask & COLOR_GREEN) {
        gpio_put(GREEN_LED, 1);
        play_tone(BUZZER, 659);
    }
    if (mask & COLOR_YELLOW) {
        gpio_put(YELLOW_LED, 1);
        play_tone(BUZZER, 784);
    }
    
    // Multiple tones isn't possible with 1 buzzer, so the highest bit/color played wins.
    sleep_ms(duration_ms);
    
    gpio_put(BLUE_LED, 0);
    gpio_put(RED_LED, 0);
    gpio_put(GREEN_LED, 0);
    gpio_put(YELLOW_LED, 0);
    play_tone(BUZZER, 0);
}

void flush_button_inputs(void) {
    btn_blue_flag = false;
    btn_red_flag = false;
    btn_green_flag = false;
    btn_yellow_flag = false;
}

// Retorna uma máscara do que o usuário inseriu (se duplo, espera para juntar)
uint8_t get_user_input_blocking(uint8_t expected_mask) {
    uint8_t collected = 0;
    while(collected == 0) {
        if (btn_blue_flag) collected |= COLOR_BLUE;
        if (btn_red_flag) collected |= COLOR_RED;
        if (btn_green_flag) collected |= COLOR_GREEN;
        if (btn_yellow_flag) collected |= COLOR_YELLOW;
        
        if (collected != 0) {
            // Se o nível exigir 2 cliques e a gente detectou 1
            if (expected_mask != 0 && (expected_mask & (expected_mask - 1)) != 0) {
                sleep_ms(50); // Debounce longo pra ver se mais um botão entrou na interrupção
                if (btn_blue_flag) collected |= COLOR_BLUE;
                if (btn_red_flag) collected |= COLOR_RED;
                if (btn_green_flag) collected |= COLOR_GREEN;
                if (btn_yellow_flag) collected |= COLOR_YELLOW;
            }
            break;
        }
        sleep_ms(5); 
    }
    flush_button_inputs();
    return collected;
}

void play_game_over_sound(void) {
    play_tone(BUZZER, 150); // Som grosso "morto"
    gpio_put(RED_LED, 1);
    sleep_ms(1000);
    gpio_put(RED_LED, 0);
    play_tone(BUZZER, 0);
}