// jogoshiftystones.cpp : Este arquivo contém a função 'main'. A execução do programa começa e termina ali.
//

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

/*
  Versão simples do "Shifting Stones" com cores ANSI na consola.
  Nível: simples / universitário inicial (conforme pedido).
*/

/* ----------------- constantes ----------------- */
#define N_PLAYERS 2
#define BOARD_N 9
#define BOARD_SIDE 3
#define MAX_HAND 8
#define START_HAND 4
#define MAX_DECK 100
#define SAVEFILE "savegame.dat"
#define TARGET_SCORED_CARDS 10

/* cores (0..7) mapeadas para códigos ANSI */
const char* ANSI_COL[] = {
    "\x1b[41m", /* 0 - Red background */
    "\x1b[42m", /* 1 - Green bg */
    "\x1b[43m", /* 2 - Yellow bg */
    "\x1b[44m", /* 3 - Blue bg */
    "\x1b[45m", /* 4 - Magenta bg */
    "\x1b[46m", /* 5 - Cyan bg */
    "\x1b[47m", /* 6 - White bg */
    "\x1b[100m" /* 7 - Bright black (grey) bg */
};
const char* ANSI_RESET = "\x1b[0m";

/* ----------------- tipos ----------------- */
typedef struct {
    int front;   /* cor da face frontal (0..7) */
    int back;    /* cor do verso (0..7) */
    int flipped; /* 0 => mostrar front, 1 => mostrar back */
} Stone;

typedef struct {
    int positions[4]; /* posições do padrão (0..😎. Usamos até 4 slots; n_pos define quant.) */
    int n_pos;
    int required[4];  /* cor exigida para cada posição, correspondendo ao visível */
    int points;       /* pontos da carta (1..3) */
    int scored;       /* 0 = não pontuada, 1 = pontuada */
    char name[32];
} Card;

typedef struct {
    char name[32];
    int is_bot; /* 0 humano, 1 bot */
    Card hand[MAX_HAND];
    int hand_count;
    Card scored[MAX_DECK];
    int scored_count;
    int passed_last; /* 1 se passou no turno anterior */
} Player;

/* ----------------- estado global ----------------- */
Stone board[BOARD_N];
Card deck[MAX_DECK];
int deck_count = 0;
int deck_top = 0; /* índice do topo */

Player players[N_PLAYERS];
int current_player = 0;
int round_first_player = 0;

/* ----------------- utilitários ----------------- */
int rand_range(int a, int b) { return a + rand() % (b - a + 1); }

void pause_and_clear() {
#ifdef _WIN32
    system("pause");
    system("cls");
#else
    printf("Pressione Enter para continuar...");
    getchar();
    printf("\x1b[2J\x1b[H"); /* clear terminal */
#endif
}

void initRandom() { srand((unsigned)time(NULL)); }

/* ----------------- inicialização ----------------- */
void initStones() {
    /* atribui faces (exemplo simples): para variar, geramos random */
    for (int i = 0; i < BOARD_N; ++i) {
        board[i].front = rand_range(0, 7);
        board[i].back = rand_range(0, 7);
        board[i].flipped = 0;
    }
}

void make_card(Card* c, int npos, int* poss, int* req, int pts, const char* name) {
    c->n_pos = npos;
    for (int i = 0; i < npos; ++i) { c->positions[i] = poss[i]; c->required[i] = req[i]; }
    c->points = pts;
    c->scored = 0;
    strncpy(c->name, name, sizeof(c->name) - 1);
    c->name[sizeof(c->name) - 1] = '\0';
}

/* cria um deck simples e embaralha */
void create_deck() {
    deck_count = 0;
    /* vamos criar cartas com padrões de 2 ou 3 pedras e pontos 1..3 */
    int idx = 0;
    for (int rep = 0; rep < 6; ++rep) {
        /* algumas cartas de 1 ponto (pares, cores variadas) */
        for (int i = 0; i < 6 && deck_count < MAX_DECK; ++i) {
            Card c; int pos[2]; int req[2];
            pos[0] = rand_range(0, 8); pos[1] = rand_range(0, 8);
            if (pos[0] == pos[1]) pos[1] = (pos[0] + 1) % 9;
            req[0] = rand_range(0, 7); req[1] = rand_range(0, 7);
            make_card(&c, 2, pos, req, 1, "Pair-1pt");
            deck[deck_count++] = c;
        }
        /* algumas cartas de 2 pontos (trios) */
        for (int i = 0; i < 4 && deck_count < MAX_DECK; ++i) {
            Card c; int pos[3]; int req[3];
            pos[0] = rand_range(0, 8);
            pos[1] = (pos[0] + 1) % 9;
            pos[2] = (pos[0] + 2) % 9;
            req[0] = rand_range(0, 7); req[1] = rand_range(0, 7); req[2] = rand_range(0, 7);
            make_card(&c, 3, pos, req, 2, "Tri-2pt");
            deck[deck_count++] = c;
        }
        /* algumas cartas de 3 pontos (quatro posições) */
        for (int i = 0; i < 2 && deck_count < MAX_DECK; ++i) {
            Card c; int pos[4]; int req[4];
            pos[0] = rand_range(0, 8);
            for (int k = 1; k < 4; ++k) pos[k] = (pos[0] + k) % 9;
            for (int k = 0; k < 4; ++k) req[k] = rand_range(0, 7);
            make_card(&c, 4, pos, req, 3, "Quad-3pt");
            deck[deck_count++] = c;
        }
    }
    /* duplicate some for larger deck */
    int orig = deck_count;
    for (int i = 0; i < orig && deck_count < MAX_DECK; ++i) deck[deck_count++] = deck[i];
    /* shuffle */
    for (int i = deck_count - 1; i > 0; --i) {
        int j = rand_range(0, i);
        Card tmp = deck[i]; deck[i] = deck[j]; deck[j] = tmp;
    }
    deck_top = 0;
}

Card draw_card() {
    Card empty; empty.n_pos = 0;
    if (deck_top >= deck_count) return empty;
    return deck[deck_top++];
}

/* inicializa jogadores pedindo nomes e se são bots */
void initPlayers() {
    for (int p = 0; p < N_PLAYERS; ++p) {
        printf("Nome do Jogador %d: ", p + 1);
        if (!fgets(players[p].name, sizeof(players[p].name), stdin)) players[p].name[0] = '\0';
        players[p].name[strcspn(players[p].name, "\n")] = '\0';
        if (strlen(players[p].name) == 0) snprintf(players[p].name, sizeof(players[p].name), "Jogador %d", p + 1);

        char buf[8];
        printf("E bot? (s/n): ");
        if (!fgets(buf, sizeof(buf), stdin)) strcpy(buf, "n");
        players[p].is_bot = (buf[0] == 's' || buf[0] == 'S');

        players[p].hand_count = 0;
        players[p].scored_count = 0;
        players[p].passed_last = 0;
    }
}

/* deal inicial 4 cartas a cada jogador */
void deal_initial_hands() {
    for (int p = 0; p < N_PLAYERS; ++p) {
        players[p].hand_count = 0;
        for (int k = 0; k < START_HAND; ++k) {
            Card c = draw_card();
            if (c.n_pos > 0 && players[p].hand_count < MAX_HAND) players[p].hand[players[p].hand_count++] = c;
        }
    }
}

/* ----------------- display ----------------- */
/* mostra uma pedra como quadrado colorido com o número da posição dentro */
void show_stone_colored(int pos) {
    int vis = board[pos].flipped ? board[pos].back : board[pos].front;
    /* mostramos um bloco preenchido com cor, e colocamos o índice pos+1 no meio (modo simples) */
    printf("%s  %2d  %s", ANSI_COL[vis], pos + 1, ANSI_RESET);
}

void print_board() {
    printf("\nTABULEIRO (posicao):\n\n");
    for (int r = 0; r < BOARD_SIDE; ++r) {
        for (int c = 0; c < BOARD_SIDE; ++c) {
            int idx = r * BOARD_SIDE + c;
            show_stone_colored(idx);
            if (c < BOARD_SIDE - 1) printf(" ");
        }
        printf("\n");
    }
    printf("\n(Quadrado colorido mostra cor visivel; numero indica posicao 1..9)\n");
}

void print_hand(int p) {
    printf("\n%s - Mao (%d cartas):\n", players[p].name, players[p].hand_count);
    for (int i = 0; i < players[p].hand_count; ++i) {
        Card* c = &players[p].hand[i];
        printf(" %d) %s [pts:%d] pos:", i + 1, c->name, c->points);
        for (int k = 0; k < c->n_pos; ++k) printf(" %d", c->positions[k] + 1);
        printf(" req:");
        for (int k = 0; k < c->n_pos; ++k) printf(" %d", c->required[k]);
        printf("\n");
    }
}

void print_scored(int p) {
    printf("\n%s - Cartas pontuadas (%d):\n", players[p].name, players[p].scored_count);
    for (int i = 0; i < players[p].scored_count; ++i)
        printf("  - %s (pts:%d)\n", players[p].scored[i].name, players[p].scored[i].points);
}

/* ----------------- regras do jogo ----------------- */
int adjacent(int a, int b) {
    int ar = a / BOARD_SIDE, ac = a % BOARD_SIDE;
    int br = b / BOARD_SIDE, bc = b % BOARD_SIDE;
    int dr = abs(ar - br), dc = abs(ac - bc);
    return (dr + dc == 1); /* não diagonais */
}

/* troca posições das pedras */
void swap_stones(int a, int b) {
    Stone tmp = board[a];
    board[a] = board[b];
    board[b] = tmp;
}

/* vira stone */
void flip_stone(int pos) { board[pos].flipped = !board[pos].flipped; }

/* verifica se carta corresponde ao tabuleiro (orientação correta) */
int card_matches_board(Card* c) {
    for (int i = 0; i < c->n_pos; ++i) {
        int pos = c->positions[i];
        int vis = board[pos].flipped ? board[pos].back : board[pos].front;
        if (vis != c->required[i]) return 0;
    }
    return 1;
}

/* remove carta da mão (índice) */
Card remove_card_from_hand(int p, int idx) {
    Card c = players[p].hand[idx];
    for (int i = idx; i < players[p].hand_count - 1; ++i) players[p].hand[i] = players[p].hand[i + 1];
    players[p].hand_count--;
    return c;
}

void add_card_to_hand(int p, Card c) {
    if (c.n_pos == 0) return;
    if (players[p].hand_count < MAX_HAND) players[p].hand[players[p].hand_count++] = c;
}

/* pontuar carta da mão (index) */
void score_card_from_hand(int p, int idx) {
    Card c = players[p].hand[idx];
    c.scored = 1;
    players[p].scored[players[p].scored_count++] = c;
    remove_card_from_hand(p, idx);
}

/* ----------------- salvamento / carregamento ----------------- */
void autosave() {
    FILE* f = fopen(SAVEFILE, "wb");
    if (!f) { printf("Aviso: nao foi possível gravar jogo.\n"); return; }
    fwrite(board, sizeof(Stone), BOARD_N, f);
    fwrite(&deck_count, sizeof(deck_count), 1, f);
    fwrite(&deck_top, sizeof(deck_top), 1, f);
    fwrite(deck, sizeof(Card), deck_count, f);
    fwrite(&current_player, sizeof(current_player), 1, f);
    fwrite(&round_first_player, sizeof(round_first_player), 1, f);
    for (int p = 0; p < N_PLAYERS; ++p) {
        fwrite(players[p].name, sizeof(players[p].name), 1, f);
        fwrite(&players[p].is_bot, sizeof(players[p].is_bot), 1, f);
        fwrite(&players[p].hand_count, sizeof(players[p].hand_count), 1, f);
        fwrite(players[p].hand, sizeof(Card), players[p].hand_count, f);
        fwrite(&players[p].scored_count, sizeof(players[p].scored_count), 1, f);
        fwrite(players[p].scored, sizeof(Card), players[p].scored_count, f);
        fwrite(&players[p].passed_last, sizeof(players[p].passed_last), 1, f);
    }
    fclose(f);
    printf("(Jogo gravado em %s)\n", SAVEFILE);
}

int load_game() {
    FILE* f = fopen(SAVEFILE, "rb");
    if (!f) return 0;
    fread(board, sizeof(Stone), BOARD_N, f);
    fread(&deck_count, sizeof(deck_count), 1, f);
    fread(&deck_top, sizeof(deck_top), 1, f);
    fread(deck, sizeof(Card), deck_count, f);
    fread(&current_player, sizeof(current_player), 1, f);
    fread(&round_first_player, sizeof(round_first_player), 1, f);
    for (int p = 0; p < N_PLAYERS; ++p) {
        fread(players[p].name, sizeof(players[p].name), 1, f);
        fread(&players[p].is_bot, sizeof(players[p].is_bot), 1, f);
        fread(&players[p].hand_count, sizeof(players[p].hand_count), 1, f);
        fread(players[p].hand, sizeof(Card), players[p].hand_count, f);
        fread(&players[p].scored_count, sizeof(players[p].scored_count), 1, f);
        fread(players[p].scored, sizeof(Card), players[p].scored_count, f);
        fread(&players[p].passed_last, sizeof(players[p].passed_last), 1, f);
    }
    fclose(f);
    printf("Jogo carregado de %s\n", SAVEFILE);
    return 1;
}

/* ----------------- bot simples ----------------- */
void bot_turn(int p) {
    /* tenta pontuar se tiver carta que corresponde */
    int acted = 0;
    while (1) {
        int did_score = 0;
        for (int i = 0; i < players[p].hand_count; ++i) {
            if (card_matches_board(&players[p].hand[i])) {
                printf("%s (BOT) pontua carta %s\n", players[p].name, players[p].hand[i].name);
                score_card_from_hand(p, i);
                did_score = 1; acted = 1; break;
            }
        }
        if (did_score) continue;
        /* se não pontuou, e tem cartas, descarta uma para virar uma pedra (se isso for possível) */
        if (players[p].hand_count > 0) {
            int discard = rand_range(0, players[p].hand_count - 1);
            int target = rand_range(0, BOARD_N - 1);
            printf("%s (BOT) descarta %s e vira pedra %d\n", players[p].name, players[p].hand[discard].name, target + 1);
            remove_card_from_hand(p, discard);
            flip_stone(target);
            acted = 1;
            continue;
        }
        break;
    }
    /* terminar turno: comprar até START_HAND */
    while (players[p].hand_count < START_HAND) {
        Card c = draw_card(); if (c.n_pos == 0) break; add_card_to_hand(p, c);
    }
    players[p].passed_last = 0;
}

/* ----------------- turno humano ----------------- */
void player_turn(int p) {
    char op[8];
    while (1) {
        printf("\n--- Turno de %s ---\n", players[p].name);
        print_board();
        print_hand(p);
        print_scored(p);
        printf("\nOpções:\n");
        printf(" a) Trocar duas Pedras (descartar 1 carta)\n");
        printf(" b) Virar uma Pedra (descartar 1 carta)\n");
        printf(" c) Pontuar uma Carta\n");
        printf(" d) Terminar o turno (comprar até %d)\n", START_HAND);
        printf(" e) Passar a vez (+2 cartas) [não pode passar 2x seguidas]\n");
        printf(" f) Sair do jogo (gravar e sair)\n");
        printf("Escolha: ");
        if (!fgets(op, sizeof(op), stdin)) return;
        if (op[0] == 'a') {
            if (players[p].hand_count == 0) { printf("Nao tens cartas para descartar.\n"); continue; }
            printf("Escolhe indice da carta a descartar (1..%d): ", players[p].hand_count);
            int ci; if (scanf("%d%*c", &ci) != 1) { printf("Entrada invalida\n"); while (getchar() != '\n'); continue; }
            if (ci < 1 || ci > players[p].hand_count) { printf("Indice invalido\n"); continue; }
            printf("Escolhe pedra A (1..9): "); int a; if (scanf("%d%*c", &a) != 1) { while (getchar() != '\n'); continue; }
            printf("Escolhe pedra B (1..9): "); int b; if (scanf("%d%*c", &b) != 1) { while (getchar() != '\n'); continue; }
            if (a < 1 || a > 9 || b < 1 || b > 9) { printf("Posicoes invalidas\n"); continue; }
            if (!adjacent(a - 1, b - 1)) { printf("Apenas adjacentes (nao diagonais)\n"); continue; }
            printf("Descartas %s e trocas %d <-> %d\n", players[p].hand[ci - 1].name, a, b);
            remove_card_from_hand(p, ci - 1);
            swap_stones(a - 1, b - 1);
            continue;
        }
        else if (op[0] == 'b') {
            if (players[p].hand_count == 0) { printf("Nao tens cartas para descartar.\n"); continue; }
            printf("Escolhe indice da carta a descartar (1..%d): ", players[p].hand_count);
            int ci; if (scanf("%d%*c", &ci) != 1) { while (getchar() != '\n'); continue; }
            if (ci < 1 || ci > players[p].hand_count) { printf("Indice invalido\n"); continue; }
            printf("Escolhe pedra a virar (1..9): "); int pos; if (scanf("%d%*c", &pos) != 1) { while (getchar() != '\n'); continue; }
            if (pos < 1 || pos > 9) { printf("Posicao invalida\n"); continue; }
            printf("Descartas %s e viras pedra %d\n", players[p].hand[ci - 1].name, pos);
            remove_card_from_hand(p, ci - 1);
            flip_stone(pos - 1);
            continue;
        }
        else if (op[0] == 'c') {
            if (players[p].hand_count == 0) { printf("Nao tens cartas para pontuar.\n"); continue; }
            printf("Escolhe carta para pontuar (1..%d): ", players[p].hand_count);
            int ci; if (scanf("%d%*c", &ci) != 1) { while (getchar() != '\n'); continue; }
            if (ci < 1 || ci > players[p].hand_count) { printf("Índice invalido\n"); continue; }
            if (card_matches_board(&players[p].hand[ci - 1])) {
                printf("Carta corresponde! Pontuada.\n");
                score_card_from_hand(p, ci - 1);
            }
            else {
                printf("Carta nao corresponde ao tabuleiro.\n");
            }
            continue;
        }
        else if (op[0] == 'd') {
            while (players[p].hand_count < START_HAND) {
                Card c = draw_card(); if (c.n_pos == 0) break; add_card_to_hand(p, c);
            }
            players[p].passed_last = 0;
            autosave();
            break;
        }
        else if (op[0] == 'e') {
            if (players[p].passed_last) { printf("Não podes passar duas vezes seguidas.\n"); continue; }
            Card c1 = draw_card(); if (c1.n_pos > 0) add_card_to_hand(p, c1);
            Card c2 = draw_card(); if (c2.n_pos > 0) add_card_to_hand(p, c2);
            players[p].passed_last = 1;
            players[1 - p].passed_last = 0;
            autosave();
            break;
        }
        else if (op[0] == 'f') {
            autosave();
            printf("Saindo...\n");
            exit(0);
        }
        else {
            printf("Opção inválida\n");
        }
    }
}

/* ----------------- verificar fim ----------------- */
int check_endgame(int* who) {
    for (int p = 0; p < N_PLAYERS; ++p) {
        if (players[p].scored_count >= TARGET_SCORED_CARDS) { *who = p; return 1; }
    }
    return 0;
}

/* ----------------- contagem final ----------------- */
void final_scoring_and_show() {
    int points[N_PLAYERS] = { 0,0 };
    int onept[N_PLAYERS] = { 0,0 };
    for (int p = 0; p < N_PLAYERS; ++p) {
        for (int i = 0; i < players[p].scored_count; ++i) {
            points[p] += players[p].scored[i].points;
            if (players[p].scored[i].points == 1) onept[p]++;
        }
    }
    if (onept[0] > onept[1]) points[0] += 3;
    else if (onept[1] > onept[0]) points[1] += 3;

    printf("\n===== RESULTADO FINAL =====\n");
    for (int p = 0; p < N_PLAYERS; ++p) {
        printf("%s: %d pontos (cartas pontuadas: %d, 1pt: %d)\n",
            players[p].name, points[p], players[p].scored_count, onept[p]);
    }
    if (points[0] > points[1]) printf("Vencedor: %s\n", players[0].name);
    else if (points[1] > points[0]) printf("Vencedor: %s\n", players[1].name);
    else printf("Empate!\n");
}

/* ----------------- jogo (loop) ----------------- */
void play_game() {
    round_first_player = current_player;
    int winner_idx = -1;
    while (1) {
        int p = current_player;
        if (players[p].is_bot) bot_turn(p);
        else player_turn(p);

        autosave();

        if (check_endgame(&winner_idx)) {
            if (winner_idx == round_first_player) {
                printf("Jogador %s pontuou %d cartas e activa fim. O adversario ainda joga o seu turno final.\n",
                    players[winner_idx].name, players[winner_idx].scored_count);
                int opponent = 1 - winner_idx;
                if (players[opponent].is_bot) bot_turn(opponent);
                else player_turn(opponent);
                final_scoring_and_show();
                break;
            }
            else {
                final_scoring_and_show();
                break;
            }
        }

        current_player = 1 - current_player;
        if (current_player == round_first_player) round_first_player = current_player;
    }
}

/* ----------------- menu ----------------- */
void show_rules() {
    printf("\nREGRAS (resumo):\n");
    printf("- Tabuleiro 3x3 com pedras (cada pedra tem duas faces, cores 0..7).\n");
    printf("- Cada jogador começa com 4 cartas. Cada carta é um padrao (2..4 pedras) e pontos.\n");
    printf("- No turno, podes: descartar 1 carta para trocar pedras adjacentes; descartar 1 carta para virar pedra;\n");
    printf("  pontuar carta (se o padrao estiver visivel na orientacao correta); terminar turno (comprar ate 4);\n");
    printf("  passar a vez (+2 cartas), nao e permitido passar duas vezes seguidas.\n");
    printf("- O jogo termina quando um jogador pontuar 10 cartas; se for o jogador que iniciou a ronda, o oponente ainda joga o seu turno final.\n");
    printf("- Contagem final: soma dos pontos das cartas; jogador com mais cartas de 1 ponto recebe +3 pontos.\n");
    pause_and_clear();
}

void start_new_game() {
    initStones();
    create_deck();
    initPlayers();
    deal_initial_hands();
    current_player = rand_range(0, 1);
    round_first_player = current_player;
    printf("Começando novo jogo. Jogador inicial: %s\n", players[current_player].name);
    play_game();
}

void menu() {
    char op[8];
    while (1) {
        printf("=== SHIFING STONES (simples) ===\n");
        printf("Autores: Aluno Exemplo\n");
        printf("a) Regras\n");
        printf("b) Novo jogo\n");
        printf("c) Carregar jogo\n");
        printf("d) Sair\n");
        printf("Escolha: ");
        if (!fgets(op, sizeof(op), stdin)) return;
        if (op[0] == 'a') { show_rules(); }
        else if (op[0] == 'b') { start_new_game(); }
        else if (op[0] == 'c') {
            if (load_game()) {
                printf("Continuar jogo carregado? (s/n): ");
                char ans[8]; if (!fgets(ans, sizeof(ans), stdin)) break;
                if (ans[0] == 's' || ans[0] == 'S') { play_game(); }
            }
            else printf("Nenhum jogo salvo encontrado.\n");
        }
        else if (op[0] == 'd') { printf("Adeus!\n"); break; }
        else printf("Opcao invalida\n");
    }
}

/* ----------------- main ----------------- */
int main() {
    initRandom();
    menu();
    return 0;
}