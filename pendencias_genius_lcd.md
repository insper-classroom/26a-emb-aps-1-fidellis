# Pendências para Conclusão do Jogo Genius

Este documento descreve as funcionalidades e lógicas que ainda precisam ser implementadas para que o protótipo do jogo "Genius" fique completo, com foco especial na integração e uso do display LCD (Core 0).

## 1. Menus e Interface Gráfica (LCD)

Atualmente a inicialização do jogo está atrelada ao botão de evento esquerdo ("Motorzao"), forçando o nível Dificil de maneira estática. O LCD precisa ser redesenhado para ter telas (telas/estados de interface):

*   **Menu Principal:**
    *   Desenhar título do jogo (ex: "GENIUS").
    *   Criar botões na tela para as três dificuldades: **Fácil**, **Médio** e **Difícil**.
    *   A leitura do *touch* deverá mapear os limites (`X` e `Y`) desses novos botões.
    *   O toque nestes botões é o que engatilhará o `CMD_START_GAME` enviando a respectiva `seed` (`time_us_32()`) e a respectiva constante de dificuldade escolhida pelo usuário para o Core 1.
*   **HUD (Heads-Up Display) de Jogo:**
    *   Exibir a "Pontuação Atual" (Score) num local fixo e visível sem piscar a tela inteira.
    *   (Opcional) Feedback visual mostrando de quem é o turno: 
        *   Mensagem *"Preste Atenção..."* enquanto o Core 1 toca a sequência.
        *   Mensagem *"Sua Vez!"* quando for aguardado o *input* (precisará de uma nova mensagem via FIFO do Core 1, como `MSG_YOUR_TURN`, para o LCD exibir isso temporariamente).
*   **Tela de Game Over:**
    *   Ao receber `MSG_GAME_OVER`, a tela deve informar que o jogador perdeu.
    *   Mostrar a **Pontuação Final/Máxima**.
    *   Exibir um botão *"Voltar ao Menu Principal"* ou *"Jogar Novamente"*.

## 2. Limpeza da Main Antiga e Fluxo de Estados

*   **Remoção de Código Legado:** A `main copy.c` ainda carrega funções relativas às "Animações GFX D/E" e ao "Motor de Passo" (`step_motor`, variável `motorTick`). Se o Genius for o único propósito desta build final, toda essa lógica de motor deve ser limpa para poupar processamento.
*   **Lidando com os Updates Virtuais:**
    *   Atualizar o conteúdo impresso (`gfx_drawText`, `gfx_fillRect`) usando buffers ou limpando apenas os retângulos afetados para evitar artefatos de texto sobreposto e flickers (atualmente o score limpa um retângulo hardcoded de 50px).

## 3. Melhorias na Comunicação (Core 0 <-> Core 1)

O Core 1 já faz o trabalho pesado de validar o array, mas o Core 0 (Main) gerencia o display. Para uma experiência fluida:
*   Avisar o display quando a demonstração termina (ex: `multicore_fifo_push_blocking(MSG_WAIT_INPUT)`).
*   Evitar que toques equivocados na tela LCD durante o jogo interrompam ou acionem comandos sem querer (adicionar uma variável de estado do `LCD` na main: `MENU`, `IN_GAME`, `GAMEOVER`).

## 4. Aperfeiçoamentos de Hardware (Feedback)
*   **Buzzer para Botão Duplo (Modo Difícil):** O pino PWM só emite um tom ("frequência") de cada vez. Quando duas luzes acendem no Díficil (Ex: Azul + Vermelho = 440hz e 523hz), a função de som atual executa apenas a nota descrita no último `if`. No futuro, para dar um som nítido na dupla-tonalidade:
    *   Fazer um *Arpeggio* (alternar rapidíssimo as duas frequências).
    *   Ou reproduzir um som único "metálico" mapeado para eventos duplos.
*   **Aprimorar o Debounce Simultâneo:** Verificar empiricamente no protótipo se 50ms dentro de `get_user_input_blocking(...)` é suficiente para jogadores iniciantes acertarem 2 botões na mesma fração de segundo.

## 5. Medição de Tempo de Resposta e Timeout (Sem Sleep)

*   **Arquitetura Baseada em Alarmes:** Na prática, em vez do estado `STATE_WAITING_INPUT` rodar um `while` com `sleep_ms`, você configurará um alarme via `add_alarm_in_ms()`.
*   **Mecanismo de Fluxo e Medição:** Quando a interrupção de um botão acionar, você verifica se o alarme estourou. Se sim, é *Game Over*. Se não, você cancela o alarme (`cancel_alarm`), computa o tempo gasto (medindo a agilidade do jogador), toca o feedback assíncrono e agenda o próximo passo do jogo usando um novo alarme. Tudo de forma 100% assíncrona.