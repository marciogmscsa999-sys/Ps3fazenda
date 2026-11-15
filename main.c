// ============================================================
//  MINI STARDEW VALLEY PARA PS3
//  Versao em C puro, compativel com PSL1GHT
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LARGURA 8
#define ALTURA 6
#define MAX_ITENS 40
#define MAX_HOTBAR 12

typedef struct {
    int arado;
    int plantado;
    int regado;
    char cultura[32];
    int diasRestantes;
} Tile;

typedef struct {
    char nome[32];
    int precoSemente;
    int precoVenda;
    int diasParaCrescer;
} Cultura;

typedef struct {
    char nome[32];
    int preco;
    int dificuldade;
} Peixe;

typedef struct {
    char nome[64];
    int quantidade;
} Item;

Tile mapa[ALTURA][LARGURA];

int energia = 100;
int dinheiro = 500;
int dia = 1;
int estacao = 0; // 0 Primavera, 1 Verao, 2 Outono, 3 Inverno

int nivelPesca = 0;
int xpPesca = 0;
int temVara = 1;

Item inventario[MAX_ITENS];
int totalItens = 0;

char hotbar[MAX_HOTBAR][64];

const char *nomesEstacao[4] = {
    "Primavera",
    "Verao",
    "Outono",
    "Inverno"
};

Cultura culturas[3] = {
    {"chirivia", 20, 35, 4},
    {"batata", 50, 80, 6},
    {"couve", 100, 175, 9}
};

Peixe peixes[5] = {
    {"carpa", 30, 1},
    {"perca", 60, 2},
    {"truta", 100, 3},
    {"lula", 150, 4},
    {"peixe_dourado", 500, 5}
};

// ============================================================
//  INVENTARIO
// ============================================================

int buscarItem(const char *nome) {
    int i;

    for (i = 0; i < totalItens; i++) {
        if (strcmp(inventario[i].nome, nome) == 0) {
            return i;
        }
    }

    return -1;
}

void adicionarItem(const char *nome, int quantidade) {
    int indice = buscarItem(nome);

    if (indice >= 0) {
        inventario[indice].quantidade += quantidade;
        return;
    }

    if (totalItens >= MAX_ITENS) {
        printf("Inventario cheio!\n");
        return;
    }

    strcpy(inventario[totalItens].nome, nome);
    inventario[totalItens].quantidade = quantidade;
    totalItens++;
}

int removerItem(const char *nome, int quantidade) {
    int indice = buscarItem(nome);

    if (indice < 0 || inventario[indice].quantidade < quantidade) {
        return 0;
    }

    inventario[indice].quantidade -= quantidade;

    if (inventario[indice].quantidade <= 0) {
        int i;

        for (i = indice; i < totalItens - 1; i++) {
            inventario[i] = inventario[i + 1];
        }

        totalItens--;
    }

    return 1;
}

void mostrarInventario() {
    int i;

    printf("\n==============================================\n");
    printf("                 INVENTARIO\n");
    printf("==============================================\n");
    printf("Ouro: %dg | Pesca: Nivel %d | XP: %d\n",
           dinheiro, nivelPesca, xpPesca);
    printf("----------------------------------------------\n");

    if (totalItens == 0) {
        printf("Seu inventario esta vazio.\n");
    }

    for (i = 0; i < totalItens; i++) {
        if (inventario[i].quantidade > 0) {
            printf("%-28s x%d\n",
                   inventario[i].nome,
                   inventario[i].quantidade);
        }
    }

    printf("==============================================\n");
}

void mostrarHotbar() {
    int i;

    printf("\nHOTBAR\n");

    for (i = 0; i < MAX_HOTBAR; i++) {
        printf("[%2d] %-18s\n", i + 1, hotbar[i]);
    }
}

// ============================================================
//  INTERFACE
// ============================================================

void mostrarHUD() {
    int i;
    int barras = energia / 10;

    printf("\n==============================================\n");
    printf("DIA %d/28 | %s | OURO: %dg\n",
           dia, nomesEstacao[estacao], dinheiro);

    printf("ENERGIA [");

    for (i = 0; i < 10; i++) {
        printf(i < barras ? "#" : ".");
    }

    printf("] %d/100\n", energia);

    printf("PESCA: Nivel %d | XP: %d\n", nivelPesca, xpPesca);
    printf("==============================================\n");

    mostrarHotbar();
}

void mostrarMapa() {
    int y, x;

    printf("\n================== SUA FAZENDA ==================\n");
    printf("Estacao: %s | Dia: %d/28\n\n",
           nomesEstacao[estacao], dia);

    printf("   ");

    for (x = 0; x < LARGURA; x++) {
        printf("%4d", x);
    }

    printf("\n");

    for (y = 0; y < ALTURA; y++) {
        printf("%3d ", y);

        for (x = 0; x < LARGURA; x++) {
            Tile *t = &mapa[y][x];

            if (t->plantado && t->diasRestantes == 0) {
                printf("  C  ");
            } else if (t->plantado && t->regado) {
                printf("  w  ");
            } else if (t->plantado) {
                printf("  s  ");
            } else if (t->arado) {
                printf("  ~~ ");
            } else {
                printf("  .  ");
            }
        }

        printf("\n");
    }

    printf("\nLegenda:\n");
    printf(".  = grama\n");
    printf("~~ = solo arado\n");
    printf("s  = semente plantada\n");
    printf("w  = plantacao regada\n");
    printf("C  = pronta para colher\n");
}

int podeAgir() {
    if (energia <= 0) {
        printf("\nVoce esta sem energia. Va dormir.\n");
        return 0;
    }

    return 1;
}

void gastarEnergia(int quantidade) {
    energia -= quantidade;

    if (energia < 0) {
        energia = 0;
    }
}

int lerInteiro(const char *mensagem) {
    int valor;

    printf("%s", mensagem);

    while (scanf("%d", &valor) != 1) {
        while (getchar() != '\n');
        printf("Entrada invalida. Tente novamente: ");
    }

    return valor;
}

void lerTexto(const char *mensagem, char *destino, int tamanho) {
    printf("%s", mensagem);
    scanf("%63s", destino);

    if (tamanho < 64) {
        destino[tamanho - 1] = '\0';
    }
}

// ============================================================
//  FAZENDA
// ============================================================

void arar(int y, int x) {
    if (!podeAgir()) return;

    if (y < 0 || y >= ALTURA || x < 0 || x >= LARGURA) {
        printf("Posicao invalida.\n");
        return;
    }

    if (mapa[y][x].arado) {
        printf("Este tile ja esta arado.\n");
        return;
    }

    mapa[y][x].arado = 1;
    gastarEnergia(2);

    printf("\nVoce arou o solo em (%d, %d).\n", x, y);
}

void plantar(int y, int x, const char *semente) {
    char chave[64];
    int i;

    if (!podeAgir()) return;

    snprintf(chave, sizeof(chave), "semente_%s", semente);

    if (buscarItem(chave) < 0) {
        printf("Voce nao tem sementes de %s.\n", semente);
        return;
    }

    if (y < 0 || y >= ALTURA || x < 0 || x >= LARGURA) {
        printf("Posicao invalida.\n");
        return;
    }

    if (!mapa[y][x].arado || mapa[y][x].plantado) {
        printf("Escolha um solo arado e vazio.\n");
        return;
    }

    for (i = 0; i < 3; i++) {
        if (strcmp(culturas[i].nome, semente) == 0) {
            strcpy(mapa[y][x].cultura, semente);
            mapa[y][x].plantado = 1;
            mapa[y][x].diasRestantes = culturas[i].diasParaCrescer;
            mapa[y][x].regado = 0;

            removerItem(chave, 1);
            gastarEnergia(1);

            printf("\nVoce plantou %s.\n", semente);
            return;
        }
    }

    printf("Semente invalida.\n");
}

void regar(int y, int x) {
    if (!podeAgir()) return;

    if (y < 0 || y >= ALTURA || x < 0 || x >= LARGURA) {
        printf("Posicao invalida.\n");
        return;
    }

    if (!mapa[y][x].plantado) {
        printf("Nao ha plantacao aqui.\n");
        return;
    }

    if (mapa[y][x].regado) {
        printf("Esta plantacao ja foi regada hoje.\n");
        return;
    }

    mapa[y][x].regado = 1;
    gastarEnergia(1);

    printf("\nVoce regou a plantacao.\n");
}

void colher(int y, int x) {
    if (!podeAgir()) return;

    if (y < 0 || y >= ALTURA || x < 0 || x >= LARGURA) {
        printf("Posicao invalida.\n");
        return;
    }

    if (!mapa[y][x].plantado || mapa[y][x].diasRestantes > 0) {
        printf("A plantacao ainda nao esta pronta.\n");
        return;
    }

    adicionarItem(mapa[y][x].cultura, 1);
    gastarEnergia(1);

    printf("\nVoce colheu 1 %s!\n", mapa[y][x].cultura);

    mapa[y][x].arado = 0;
    mapa[y][x].plantado = 0;
    mapa[y][x].regado = 0;
    mapa[y][x].diasRestantes = 0;
    strcpy(mapa[y][x].cultura, "");
}

void avancarDia() {
    int y, x;

    dia++;

    for (y = 0; y < ALTURA; y++) {
        for (x = 0; x < LARGURA; x++) {
            Tile *t = &mapa[y][x];

            if (t->plantado && t->regado && t->diasRestantes > 0) {
                t->diasRestantes--;
            }

            t->regado = 0;
        }
    }

    if (dia > 28) {
        dia = 1;
        estacao = (estacao + 1) % 4;

        printf("\nUma nova estacao comecou: %s!\n",
               nomesEstacao[estacao]);
    }

    energia = 100;

    printf("\nVoce dormiu. Energia restaurada.\n");
}

// ============================================================
//  LOJA E VENDAS
// ============================================================

void comprar(const char *semente, int quantidade) {
    char chave[64];
    int i;

    snprintf(chave, sizeof(chave), "semente_%s", semente);

    for (i = 0; i < 3; i++) {
        if (strcmp(culturas[i].nome, semente) == 0) {
            int preco = culturas[i].precoSemente * quantidade;

            if (quantidade <= 0) {
                printf("Quantidade invalida.\n");
                return;
            }

            if (dinheiro < preco) {
                printf("Ouro insuficiente.\n");
                return;
            }

            dinheiro -= preco;
            adicionarItem(chave, quantidade);

            printf("\nComprou %d sementes de %s por %dg.\n",
                   quantidade, semente, preco);

            return;
        }
    }

    printf("Semente invalida.\n");
}

int precoDoPeixe(const Peixe *peixe, const char *qualidade) {
    if (strcmp(qualidade, "iridio") == 0) {
        return peixe->preco * 2;
    }

    if (strcmp(qualidade, "ouro") == 0) {
        return (int)(peixe->preco * 1.5);
    }

    if (strcmp(qualidade, "prata") == 0) {
        return (int)(peixe->preco * 1.25);
    }

    return peixe->preco;
}

void vender(const char *item) {
    int i;

    if (buscarItem(item) < 0) {
        printf("Voce nao possui %s.\n", item);
        return;
    }

    for (i = 0; i < 3; i++) {
        if (strcmp(culturas[i].nome, item) == 0) {
            removerItem(item, 1);
            dinheiro += culturas[i].precoVenda;

            printf("\nVendeu 1 %s por %dg.\n",
                   item, culturas[i].precoVenda);

            return;
        }
    }

    for (i = 0; i < 5; i++) {
        char prefixo[96];
        char qualidade[32];

        snprintf(prefixo, sizeof(prefixo),
                 "peixe_%s_", peixes[i].nome);

        if (strncmp(item, prefixo, strlen(prefixo)) == 0) {
            strcpy(qualidade, item + strlen(prefixo));

            int preco = precoDoPeixe(&peixes[i], qualidade);

            removerItem(item, 1);
            dinheiro += preco;

            printf("\nVendeu 1 %s por %dg.\n", item, preco);

            return;
        }
    }

    printf("Este item nao pode ser vendido.\n");
}

// ============================================================
//  PESCA
// ============================================================

int sortearPeixe() {
    int sorte = rand() % 100;

    if (sorte < 10) return 4;
    if (sorte < 30) return 3;
    if (sorte < 55) return 2;
    if (sorte < 80) return 1;

    return 0;
}

int definirQualidade(char *qualidade) {
    int sorte = rand() % 100;
    int bonus = nivelPesca * 3;

    if (sorte + bonus >= 95) {
        strcpy(qualidade, "iridio");
    } else if (sorte + bonus >= 75) {
        strcpy(qualidade, "ouro");
    } else if (sorte + bonus >= 45) {
        strcpy(qualidade, "prata");
    } else {
        strcpy(qualidade, "normal");
    }

    return 0;
}

int miniJogoPesca(const Peixe *peixe) {
    int barra = 20 + nivelPesca * 2;
    int posicaoBarra = 40;
    int posicaoPeixe = 50;
    int progresso = 50;
    int dificuldade = peixe->dificuldade;

    char comando;

    printf("\n==============================================\n");
    printf("PESCA - %s\n", peixe->nome);
    printf("==============================================\n");
    printf("Digite 's' para subir a barra.\n");
    printf("Digite 'n' para soltar a barra.\n\n");

    while (progresso > 0 && progresso < 100) {
        int i;

        printf("[");

        for (i = 0; i < 100; i += 5) {
            int dentroDaBarra =
                i >= posicaoBarra &&
                i <= posicaoBarra + barra;

            int eOPeixe =
                i >= posicaoPeixe &&
                i <= posicaoPeixe + 5;

            if (dentroDaBarra && eOPeixe) {
                printf("#");
            } else if (dentroDaBarra) {
                printf("=");
            } else if (eOPeixe) {
                printf("F");
            } else {
                printf(" ");
            }
        }

        printf("]\n");
        printf("Progresso: ");

        for (i = 0; i < 20; i++) {
            printf(i < progresso / 5 ? "#" : ".");
        }

        printf(" %d%%\n", progresso);
        printf("Comando (s/n): ");

        scanf(" %c", &comando);

        if (comando == 's' || comando == 'S') {
            posicaoBarra += 8;
        } else {
            posicaoBarra -= 6;
        }

        if (posicaoBarra < 0) {
            posicaoBarra = 0;
        }

        if (posicaoBarra + barra > 100) {
            posicaoBarra = 100 - barra;
        }

        int movimento =
            (rand() % (dificuldade * 6 + 4)) - dificuldade * 3;

        posicaoPeixe += movimento;

        if (posicaoPeixe < 0) posicaoPeixe = 0;
        if (posicaoPeixe > 95) posicaoPeixe = 95;

        int capturando =
            posicaoPeixe + 5 >= posicaoBarra &&
            posicaoPeixe <= posicaoBarra + barra;

        if (capturando) {
            progresso += 8;
        } else {
            progresso -= dificuldade * 2;
        }

        if (progresso > 100) progresso = 100;
        if (progresso < 0) progresso = 0;
    }

    if (progresso >= 100) {
        printf("\nCaptura perfeita!\n");
        return 1;
    }

    printf("\nO peixe escapou!\n");
    return 0;
}

void pescar() {
    char qualidade[32];
    char chave[96];

    if (!temVara) {
        printf("Voce precisa de uma vara de pesca.\n");
        return;
    }

    if (!podeAgir()) return;

    gastarEnergia(4);

    printf("\nVoce lancou a linha na agua...\n");

    if (rand() % 100 < 35) {
        printf("Nada mordeu a isca desta vez.\n");
        return;
    }

    int indice = sortearPeixe();
    Peixe *peixe = &peixes[indice];

    printf("Um %s mordeu a isca!\n", peixe->nome);

    if (!miniJogoPesca(peixe)) {
        return;
    }

    definirQualidade(qualidade);

    snprintf(chave, sizeof(chave),
             "peixe_%s_%s", peixe->nome, qualidade);

    adicionarItem(chave, 1);

    xpPesca += peixe->dificuldade * 10;

    while (xpPesca >= (nivelPesca + 1) * 100) {
        xpPesca -= (nivelPesca + 1) * 100;
        nivelPesca++;

        printf("\nSeu nivel de pesca subiu para %d!\n", nivelPesca);
    }

    printf("\nVoce capturou um %s de qualidade %s!\n",
           peixe->nome, qualidade);

    printf("Valor de venda: %dg.\n",
           precoDoPeixe(peixe, qualidade));
}

// ============================================================
//  MENU
// ============================================================

void mostrarMenu() {
    printf("\n==============================================\n");
    printf("              FAZENDA DO VALE\n");
    printf("==============================================\n");
    printf("1  - Ver mapa          6  - Inventario\n");
    printf("2  - Arar solo         7  - Loja\n");
    printf("3  - Plantar           8  - Vender\n");
    printf("4  - Regar             9  - Dormir\n");
    printf("5  - Colher           10  - Pescar\n");
    printf("0  - Sair\n");
    printf("==============================================\n");
    printf("Escolha: ");
}

// ============================================================
//  MAIN
// ============================================================

int main(void) {
    int opcao;

    srand((unsigned int)time(NULL));

    strcpy(hotbar[0], "enxada");
    strcpy(hotbar[1], "regador");
    strcpy(hotbar[2], "vara_de_pesca");
    strcpy(hotbar[3], "semente_chirivia");

    adicionarItem("semente_chirivia", 5);

    printf("\n==============================================\n");
    printf("     BEM-VINDO A FAZENDA DO VALE!\n");
    printf("==============================================\n");
    printf("Voce comeca com 5 sementes de chirivia e 500g.\n");

    while (1) {
        mostrarHUD();
        mostrarMenu();

        if (scanf("%d", &opcao) != 1) {
            break;
        }

        if (opcao == 0) {
            printf("\nAte a proxima colheita!\n");
            break;
        }

        switch (opcao) {
            case 1:
                mostrarMapa();
                break;

            case 2: {
                int x = lerInteiro("X: ");
                int y = lerInteiro("Y: ");

                arar(y, x);
                break;
            }

            case 3: {
                char semente[32];
                int x, y;

                mostrarInventario();
                lerTexto("Semente (chirivia, batata, couve): ",
                         semente, sizeof(semente));

                x = lerInteiro("X: ");
                y = lerInteiro("Y: ");

                plantar(y, x, semente);
                break;
            }

            case 4: {
                int x = lerInteiro("X: ");
                int y = lerInteiro("Y: ");

                regar(y, x);
                break;
            }

            case 5: {
                int x = lerInteiro("X: ");
                int y = lerInteiro("Y: ");

                colher(y, x);
                break;
            }

            case 6:
                mostrarInventario();
                break;

            case 7: {
                char semente[32];
                int quantidade;

                lerTexto("Semente (chirivia, batata, couve): ",
                         semente, sizeof(semente));

                quantidade = lerInteiro("Quantidade: ");

                comprar(semente, quantidade);
                break;
            }

            case 8: {
                char item[64];

                mostrarInventario();
                lerTexto("Item para vender: ", item, sizeof(item));

                vender(item);
                break;
            }

            case 9:
                avancarDia();
                break;

            case 10:
                pescar();
                break;

            default:
                printf("Opcao invalida.\n");
        }
    }

    return 0;
}
