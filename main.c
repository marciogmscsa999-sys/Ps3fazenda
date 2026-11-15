// ============================================================
//  MINI STARDEW VALLEY EM C++
//  Fazenda, plantio, colheita, loja, dias e pesca
// ============================================================

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <iomanip>
#include <limits>
#include <cstdlib>
#include <ctime>

using namespace std;

const int LARGURA = 8;
const int ALTURA = 6;
const int SLOTS_HOTBAR = 12;

struct Tile {
    bool arado = false;
    bool plantado = false;
    bool regado = false;
    string cultura = "";
    int diasRestantes = 0;
};

struct Cultura {
    string nome;
    string icone;
    int precoSemente;
    int precoVenda;
    int diasParaCrescer;
};

struct Peixe {
    string nome;
    string icone;
    int preco;
    int dificuldade;
};

class Jogo {
private:
    vector<vector<Tile>> mapa;

    int energia = 100;
    int dinheiro = 500;
    int dia = 1;
    string estacao = "Primavera";

    int nivelPesca = 0;
    int xpPesca = 0;
    bool temVara = true;

    vector<string> hotbar;
    map<string, int> inventario;

    const map<string, Cultura> culturas = {
        {"chirivia", {"Chirívia", "🥕", 20, 35, 4}},
        {"batata",   {"Batata",   "🥔", 50, 80, 6}},
        {"couve",    {"Couve",    "🥬", 100, 175, 9}}
    };

    const map<string, Peixe> peixes = {
        {"carpa",         {"Carpa",         "🐟", 30,  1}},
        {"perca",         {"Perca",         "🐠", 60,  2}},
        {"truta",         {"Truta",         "🐡", 100, 3}},
        {"lula",          {"Lula",          "🦑", 150, 4}},
        {"peixe_dourado", {"Peixe Dourado", "🌟", 500, 5}}
    };

public:
    Jogo() {
        mapa.assign(ALTURA, vector<Tile>(LARGURA));

        inventario["semente_chirivia"] = 5;

        hotbar.assign(SLOTS_HOTBAR, "");
        hotbar[0] = "enxada";
        hotbar[1] = "regador";
        hotbar[2] = "vara_de_pesca";
        hotbar[3] = "semente_chirivia";
    }

    // ========================================================
    //  VISUAL
    // ========================================================

    void linha() {
        cout << "  +";

        for (int i = 0; i < LARGURA * 5; i++) {
            cout << "-";
        }

        cout << "+\n";
    }

    void mostrarMapa() {
        cout << "\n";
        cout << "  ================== SUA FAZENDA ==================\n";
        cout << "  Estacao: " << estacao << " | Dia: " << dia << "/28\n";
        cout << "\n";

        linha();

        for (int y = 0; y < ALTURA; y++) {
            cout << "  |";

            for (int x = 0; x < LARGURA; x++) {
                const Tile& t = mapa[y][x];
                string celula = "  .  ";

                if (t.arado && !t.plantado) {
                    celula = "  ~~ ";
                } else if (t.plantado && t.diasRestantes == 0) {
                    celula = "  🌾 ";
                } else if (t.plantado && t.regado) {
                    celula = "  💧 ";
                } else if (t.plantado) {
                    celula = "  🌱 ";
                }

                cout << celula << "|";
            }

            cout << "\n";
            linha();
        }

        cout << "\n  Legenda:\n";
        cout << "  .  = grama\n";
        cout << "  ~~ = solo arado\n";
        cout << "  🌱 = semente plantada\n";
        cout << "  💧 = plantacao regada\n";
        cout << "  🌾 = pronta para colher\n";
    }

    void mostrarHUD() {
        cout << "\n";
        cout << "  ==============================================\n";
        cout << "  DIA " << setw(2) << dia << "/28"
             << " | " << left << setw(10) << estacao
             << " | OURO: " << dinheiro << "g\n";

        cout << "  ENERGIA [";

        int barras = energia / 10;

        for (int i = 0; i < 10; i++) {
            cout << (i < barras ? "#" : ".");
        }

        cout << "] " << energia << "/100\n";

        cout << "  PESCA: Nivel " << nivelPesca
             << " | XP: " << xpPesca << "\n";
        cout << "  ==============================================\n";

        mostrarHotbar();
    }

    void mostrarHotbar() {
        cout << "\n  HOTBAR\n";
        cout << "  +";

        for (int i = 0; i < SLOTS_HOTBAR; i++) {
            cout << "------+";
        }

        cout << "\n  |";

        for (int i = 0; i < SLOTS_HOTBAR; i++) {
            string icone = iconeDoItem(hotbar[i]);

            if (icone.empty()) {
                icone = " . ";
            }

            cout << " " << icone << " |";
        }

        cout << "\n  +";

        for (int i = 0; i < SLOTS_HOTBAR; i++) {
            cout << "------+";
        }

        cout << "\n  |";

        for (int i = 0; i < SLOTS_HOTBAR; i++) {
            string numero;

            if (i < 9) {
                numero = "  " + to_string(i + 1) + "  ";
            } else {
                numero = " " + to_string(i + 1) + "  ";
            }

            cout << numero << "|";
        }

        cout << "\n";
    }

    string iconeDoItem(const string& item) {
        if (item == "enxada") return "X";
        if (item == "regador") return "W";
        if (item == "vara_de_pesca") return "F";
        if (item == "machado") return "A";
        if (item == "picareta") return "P";

        if (item.rfind("semente_", 0) == 0) {
            string semente = item.substr(8);

            if (culturas.count(semente)) {
                return culturas.at(semente).icone;
            }
        }

        if (item.rfind("peixe_", 0) == 0) {
            for (const auto& p : peixes) {
                if (item.find("peixe_" + p.first + "_") == 0) {
                    return p.second.icone;
                }
            }
        }

        for (const auto& c : culturas) {
            if (item == c.first) {
                return c.second.icone;
            }
        }

        return "";
    }

    void mostrarInventario() {
        cout << "\n";
        cout << "  ==============================================\n";
        cout << "                 INVENTARIO\n";
        cout << "  ==============================================\n";
        cout << "  Ouro: " << dinheiro << "g"
             << " | Pesca: Nivel " << nivelPesca
             << " | XP: " << xpPesca << "\n";
        cout << "  ----------------------------------------------\n";

        bool vazio = true;

        for (const auto& item : inventario) {
            if (item.second <= 0) continue;

            string icone = iconeDoItem(item.first);

            if (icone.empty()) {
                icone = "[?]";
            }

            cout << "  " << icone << "  "
                 << left << setw(28) << item.first
                 << "x" << item.second << "\n";

            vazio = false;
        }

        if (vazio) {
            cout << "  Seu inventario esta vazio.\n";
        }

        cout << "  ==============================================\n";
    }

    bool podeAgir() {
        if (energia <= 0) {
            cout << "\n  Voce esta sem energia. Va dormir.\n";
            return false;
        }

        return true;
    }

    void gastarEnergia(int quantidade) {
        energia -= quantidade;

        if (energia < 0) {
            energia = 0;
        }
    }

    int lerInteiro(const string& mensagem) {
        int valor;

        cout << "  " << mensagem;

        while (!(cin >> valor)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "  Entrada invalida. Tente novamente: ";
        }

        return valor;
    }

    // ========================================================
    //  FAZENDA
    // ========================================================

    void arar(int y, int x) {
        if (!podeAgir()) return;

        if (y < 0 || y >= ALTURA || x < 0 || x >= LARGURA) {
            cout << "  Posicao invalida.\n";
            return;
        }

        Tile& t = mapa[y][x];

        if (t.arado) {
            cout << "  Este tile ja esta arado.\n";
            return;
        }

        t.arado = true;
        gastarEnergia(2);

        cout << "\n  Voce arou o solo em (" << x << ", " << y << ").\n";
    }

    void plantar(int y, int x, string semente) {
        if (!podeAgir()) return;

        string chave = "semente_" + semente;

        if (inventario[chave] <= 0) {
            cout << "  Voce nao tem sementes de " << semente << ".\n";
            return;
        }

        if (y < 0 || y >= ALTURA || x < 0 || x >= LARGURA) {
            cout << "  Posicao invalida.\n";
            return;
        }

        Tile& t = mapa[y][x];

        if (!t.arado || t.plantado) {
            cout << "  Escolha um solo arado e vazio.\n";
            return;
        }

        t.plantado = true;
        t.cultura = semente;
        t.diasRestantes = culturas.at(semente).diasParaCrescer;
        t.regado = false;

        inventario[chave]--;
        gastarEnergia(1);

        cout << "\n  Voce plantou " << semente << ".\n";
    }

    void regar(int y, int x) {
        if (!podeAgir()) return;

        if (y < 0 || y >= ALTURA || x < 0 || x >= LARGURA) {
            cout << "  Posicao invalida.\n";
            return;
        }

        Tile& t = mapa[y][x];

        if (!t.plantado) {
            cout << "  Nao ha plantacao aqui.\n";
            return;
        }

        if (t.regado) {
            cout << "  Esta plantacao ja foi regada hoje.\n";
            return;
        }

        t.regado = true;
        gastarEnergia(1);

        cout << "\n  Voce regou a plantacao.\n";
    }

    void colher(int y, int x) {
        if (!podeAgir()) return;

        if (y < 0 || y >= ALTURA || x < 0 || x >= LARGURA) {
            cout << "  Posicao invalida.\n";
            return;
        }

        Tile& t = mapa[y][x];

        if (!t.plantado || t.diasRestantes > 0) {
            cout << "  A plantacao ainda nao esta pronta.\n";
            return;
        }

        string produto = t.cultura;

        inventario[produto]++;
        gastarEnergia(1);

        t = Tile();

        cout << "\n  Voce colheu 1 "
             << culturas.at(produto).nome << "!\n";
    }

    void avancarDia() {
        dia++;

        for (auto& linhaAtual : mapa) {
            for (auto& t : linhaAtual) {
                if (t.plantado && t.regado && t.diasRestantes > 0) {
                    t.diasRestantes--;
                }

                t.regado = false;
            }
        }

        if (dia > 28) {
            dia = 1;

            if (estacao == "Primavera") estacao = "Verao";
            else if (estacao == "Verao") estacao = "Outono";
            else if (estacao == "Outono") estacao = "Inverno";
            else estacao = "Primavera";

            cout << "\n  Uma nova estacao comecou: "
                 << estacao << "!\n";
        }

        energia = 100;

        cout << "\n  Voce dormiu. Energia restaurada.\n";
    }

    // ========================================================
    //  LOJA E VENDAS
    // ========================================================

    void comprar(string semente, int quantidade) {
        string chave = "semente_" + semente;

        if (!culturas.count(semente)) {
            cout << "  Semente invalida.\n";
            return;
        }

        if (quantidade <= 0) {
            cout << "  Quantidade invalida.\n";
            return;
        }

        int preco = culturas.at(semente).precoSemente * quantidade;

        if (dinheiro < preco) {
            cout << "  Ouro insuficiente.\n";
            return;
        }

        dinheiro -= preco;
        inventario[chave] += quantidade;

        cout << "\n  Comprou " << quantidade << " sementes de "
             << semente << " por " << preco << "g.\n";
    }

    void vender(string item) {
        if (inventario[item] <= 0) {
            cout << "  Voce nao possui " << item << ".\n";
            return;
        }

        int preco = 0;

        for (const auto& c : culturas) {
            if (c.first == item) {
                preco = c.second.precoVenda;
                break;
            }
        }

        if (preco == 0) {
            for (const auto& p : peixes) {
                string prefixo = "peixe_" + p.first + "_";

                if (item.rfind(prefixo, 0) == 0) {
                    string qualidade = item.substr(prefixo.size());
                    preco = precoComQualidade(p.second, qualidade);
                    break;
                }
            }
        }

        if (preco == 0) {
            cout << "  Este item nao pode ser vendido.\n";
            return;
        }

        inventario[item]--;
        dinheiro += preco;

        cout << "\n  Vendeu 1 " << item
             << " por " << preco << "g.\n";
    }

    // ========================================================
    //  PESCA
    // ========================================================

    string sortearPeixe() {
        int sorte = rand() % 100;

        if (sorte < 10) return "peixe_dourado";
        if (sorte < 30) return "lula";
        if (sorte < 55) return "truta";
        if (sorte < 80) return "perca";

        return "carpa";
    }

    string definirQualidade() {
        int sorte = rand() % 100;
        int bonus = nivelPesca * 3;

        if (sorte + bonus >= 95) return "iridio";
        if (sorte + bonus >= 75) return "ouro";
        if (sorte + bonus >= 45) return "prata";

        return "normal";
    }

    int precoComQualidade(const Peixe& peixe, const string& qualidade) {
        if (qualidade == "iridio") return peixe.preco * 2;
        if (qualidade == "ouro") return static_cast<int>(peixe.preco * 1.5);
        if (qualidade == "prata") return static_cast<int>(peixe.preco * 1.25);

        return peixe.preco;
    }

    bool miniJogoPesca(const Peixe& peixe) {
        int barra = 20 + nivelPesca * 2;
        int posicaoBarra = 40;
        int posicaoPeixe = 50;
        int progresso = 50;
        int dificuldade = peixe.dificuldade;

        cout << "\n  ==============================================\n";
        cout << "  PESCA - " << peixe.icone << " " << peixe.nome << "\n";
        cout << "  ==============================================\n";
        cout << "  Digite 's' para subir a barra.\n";
        cout << "  Digite 'n' para soltar a barra.\n\n";

        while (progresso > 0 && progresso < 100) {
            cout << "  [";

            for (int i = 0; i < 100; i += 5) {
                bool dentroDaBarra =
                    i >= posicaoBarra &&
                    i <= posicaoBarra + barra;

                bool eOPeixe =
                    i >= posicaoPeixe &&
                    i <= posicaoPeixe + 5;

                if (dentroDaBarra && eOPeixe) cout << "#";
                else if (dentroDaBarra) cout << "=";
                else if (eOPeixe) cout << "F";
                else cout << " ";
            }

            cout << "]\n";
            cout << "  Progresso: ";

            int blocos = progresso / 5;

            for (int i = 0; i < 20; i++) {
                cout << (i < blocos ? "#" : ".");
            }

            cout << " " << progresso << "%\n";
            cout << "  Comando (s/n): ";

            char comando;
            cin >> comando;

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

            bool capturando =
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
            cout << "\n  Captura perfeita!\n";
            return true;
        }

        cout << "\n  O peixe escapou!\n";
        return false;
    }

    void pescar() {
        if (!temVara) {
            cout << "  Voce precisa de uma vara de pesca.\n";
            return;
        }

        if (!podeAgir()) return;

        gastarEnergia(4);

        cout << "\n  Voce lancou a linha na agua...\n";

        int chanceDeMordida = rand() % 100;

        if (chanceDeMordida < 35) {
            cout << "  Nada mordeu a isca desta vez.\n";
            return;
        }

        string nomePeixe = sortearPeixe();
        const Peixe& peixe = peixes.at(nomePeixe);

        cout << "  Um " << peixe.nome << " mordeu a isca!\n";

        if (!miniJogoPesca(peixe)) {
            return;
        }

        string qualidade = definirQualidade();
        int precoFinal = precoComQualidade(peixe, qualidade);

        string chave = "peixe_" + nomePeixe + "_" + qualidade;

        inventario[chave]++;
        xpPesca += peixe.dificuldade * 10;

        while (xpPesca >= (nivelPesca + 1) * 100) {
            xpPesca -= (nivelPesca + 1) * 100;
            nivelPesca++;

            cout << "\n  Seu nivel de pesca subiu para "
                 << nivelPesca << "!\n";
        }

        cout << "\n  Voce capturou um " << peixe.nome
             << " de qualidade " << qualidade << "!\n";

        cout << "  Valor de venda: " << precoFinal << "g.\n";
    }
};

// ============================================================
//  MENU
// ============================================================

void mostrarMenu() {
    cout << "\n  ==============================================\n";
    cout << "                 FAZENDA DO VALE\n";
    cout << "  ==============================================\n";
    cout << "  1  - Ver mapa          6  - Inventario\n";
    cout << "  2  - Arar solo         7  - Loja\n";
    cout << "  3  - Plantar           8  - Vender\n";
    cout << "  4  - Regar             9  - Dormir\n";
    cout << "  5  - Colher           10  - Pescar\n";
    cout << "  0  - Sair\n";
    cout << "  ==============================================\n";
    cout << "  Escolha: ";
}

// ============================================================
//  MAIN
// ============================================================

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));

    Jogo jogo;
    int opcao;

    cout << "\n";
    cout << "  ==============================================\n";
    cout << "       BEM-VINDO A FAZENDA DO VALE!\n";
    cout << "  ==============================================\n";
    cout << "  Voce comeca com 5 sementes de chirivia e 500g.\n";
    cout << "  Arar, plante, regue, colha e pesque para ganhar ouro.\n";

    while (true) {
        jogo.mostrarHUD();
        mostrarMenu();

        if (!(cin >> opcao)) break;

        if (opcao == 0) {
            cout << "\n  Ate a proxima colheita!\n";
            break;
        }

        switch (opcao) {
            case 1:
                jogo.mostrarMapa();
                break;

            case 2: {
                int x = jogo.lerInteiro("X: ");
                int y = jogo.lerInteiro("Y: ");
                jogo.arar(y, x);
                break;
            }

            case 3: {
                jogo.mostrarInventario();

                string semente;
                cout << "  Semente (chirivia, batata, couve): ";
                cin >> semente;

                int x = jogo.lerInteiro("X: ");
                int y = jogo.lerInteiro("Y: ");

                jogo.plantar(y, x, semente);
                break;
            }

            case 4: {
                int x = jogo.lerInteiro("X: ");
                int y = jogo.lerInteiro("Y: ");
                jogo.regar(y, x);
                break;
            }

            case 5: {
                int x = jogo.lerInteiro("X: ");
                int y = jogo.lerInteiro("Y: ");
                jogo.colher(y, x);
                break;
            }

            case 6:
                jogo.mostrarInventario();
                break;

            case 7: {
                string semente;
                cout << "  Semente (chirivia, batata, couve): ";
                cin >> semente;

                int quantidade = jogo.lerInteiro("Quantidade: ");
                jogo.comprar(semente, quantidade);
                break;
            }

            case 8: {
                jogo.mostrarInventario();

                string item;
                cout << "  Item para vender: ";
                cin >> item;

                jogo.vender(item);
                break;
            }

            case 9:
                jogo.avancarDia();
                break;

            case 10:
                jogo.pescar();
                break;

            default:
                cout << "  Opcao invalida.\n";
        }
    }

    return 0;
}
