#include <stdlib.h>

#define CHAO 400.0f
#define GRAV 1800.0f
#define PULO -700.0f

typedef struct { float x, y, vy; int no_chao; } Jogador;
typedef struct { float x, w, h; int ativo; } Obstaculo;

static Jogador j = {100, CHAO, 0, 1};
static Obstaculo obs[8];
static float vel = 300, spawn_t = 0, pontos = 0;

void jogo_pular(void) {
    if (j.no_chao) { j.vy = PULO; j.no_chao = 0; }
}

// retorna 1 se bateu (game over)
int jogo_update(float dt) {
    j.vy += GRAV * dt;
    j.y += j.vy * dt;
    if (j.y >= CHAO) { j.y = CHAO; j.vy = 0; j.no_chao = 1; }

    vel += 5 * dt;            // dificuldade sobe
    pontos += vel * dt * 0.1f;

    spawn_t -= dt;
    if (spawn_t <= 0) {
        for (int i = 0; i < 8; i++) if (!obs[i].ativo) {
            obs[i] = (Obstaculo){1280, 40, 40 + rand() % 40, 1};
            break;
        }
        spawn_t = 0.8f + (rand() % 100) / 100.0f;
    }

    for (int i = 0; i < 8; i++) if (obs[i].ativo) {
        obs[i].x -= vel * dt;
        if (obs[i].x + obs[i].w < 0) obs[i].ativo = 0;
        // colisão AABB (jogador 40x40, y = base)
        if (j.x < obs[i].x + obs[i].w && j.x + 40 > obs[i].x &&
            j.y > CHAO - obs[i].h)
            return 1;
    }
    return 0;
}
