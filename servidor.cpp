#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sstream>
#include <vector>
#include <map>
#include "cartas.h"
#include "baralho.h"
#include "regras.h"

#define PORT 8080
#define NUM_ROOMS 2
#define CLIENTS_PER_ROOM 2
#define BUFFER_SIZE 1024
#define PONTOS_VITORIA 13 // Limite de pontos do jogo

// Estruturas (Room, RelayArgs) - permanecem iguais
struct Room {
    int id;
    int client_sockets[CLIENTS_PER_ROOM];
    int client_count;
    pthread_mutex_t mutex;
    pthread_cond_t cond_full;
};
Room rooms[NUM_ROOMS];

// Funções auxiliares (serializarMao, lerDoSocket) - permanecem iguais
std::string serializarMao(const std::vector<Carta>& mao) {
    std::stringstream ss;
    for (size_t i = 0; i < mao.size(); ++i) {
        ss << (int)mao[i].valor << "," << (int)mao[i].naipe;
        if (i < mao.size() - 1) { ss << ";"; }
    }
    return ss.str();
}

bool lerDoSocket(int sock, std::string& msg) {
    char buffer[1];
    msg = "";
    while (read(sock, buffer, 1) > 0) {
        if (buffer[0] == '\n') { return true; }
        msg += buffer[0];
    }
    return false; // Cliente desconectou
}

// Função auxiliar para enviar mensagens para ambos os jogadores
void enviarParaAmbos(int sock1, int sock2, const std::string& msg) {
    send(sock1, msg.c_str(), msg.length(), 0);
    send(sock2, msg.c_str(), msg.length(), 0);
}

/**
 * @brief Lógica de Aposta (Truco)
 * Esta é uma função bloqueante que gere o pedido de truco.
 * @return 0 se a mão continuar, 1 se o J1 ganhou a mão, 2 se o J2 ganhou a mão.
 */
/**
 * @brief Lógica de Aposta (Truco)
 * @return 0 se a mão continuar, 1 se o J1 ganhou a mão, 2 se o J2 ganhou a mão.
 */
/**
 * @brief Verifica se uma mão tem 3 cartas do mesmo naipe.
 */
bool temFlor(const std::vector<Carta>& mao) {
    if (mao.size() != 3) return false; // Segurança
    
    // Verifica se o naipe da carta 0 é igual ao da 1 E ao da 2
    return (mao[0].naipe == mao[1].naipe && mao[1].naipe == mao[2].naipe);
}
int gerirPedidoTruco(int sock_pediu, int sock_responde, int& valor_mao, int nivel, int sock1) {
    std::string pedido;
    std::string resposta_valida[3];
    int pontos_se_correr;

    if (nivel == 1) { // TRUCO
        pedido = "PEDIDO_TRUCO\n";
        resposta_valida[0] = "ACEITO";
        resposta_valida[1] = "CORRO";
        resposta_valida[2] = "RETRUCO";
        pontos_se_correr = 1;
        valor_mao = 2; // Valor se for aceite
    } else if (nivel == 2) { // RETRUCO
        pedido = "PEDIDO_RETRUCO\n";
        resposta_valida[0] = "ACEITO";
        resposta_valida[1] = "CORRO";
        resposta_valida[2] = "VALE_QUATRO";
        pontos_se_correr = 2;
        valor_mao = 3; // Valor se for aceite
    } else { // VALE QUATRO
        pedido = "PEDIDO_VALE_QUATRO\n";
        resposta_valida[0] = "ACEITO";
        resposta_valida[1] = "CORRO";
        resposta_valida[2] = ""; 
        pontos_se_correr = 3;
        valor_mao = 4; // Valor se for aceite
    }

    send(sock_responde, pedido.c_str(), pedido.length(), 0);
    
    std::string msg_resposta;
    if (!lerDoSocket(sock_responde, msg_resposta)) {
        valor_mao = pontos_se_correr; // <<< CORREÇÃO 1
        return (sock_pediu == sock1 ? 1 : 2); // Desconexão = correu
    }

    if (msg_resposta == resposta_valida[0]) { // ACEITO
        std::string info = "INFO:Aposta aceite! A mão vale " + std::to_string(valor_mao) + " pontos.\n";
        enviarParaAmbos(sock_pediu, sock_responde, info);
        return 0; // Mão continua
    }
    else if (msg_resposta == resposta_valida[1]) { // CORRO
        std::string info_venceu = "INFO:Oponente correu! Você ganha " + std::to_string(pontos_se_correr) + " pontos.\n";
        std::string info_perdeu = "INFO:Você correu! Você perde " + std::to_string(pontos_se_correr) + " pontos.\n";
        send(sock_pediu, info_venceu.c_str(), info_venceu.length(), 0);
        send(sock_responde, info_perdeu.c_str(), info_perdeu.length(), 0);

        valor_mao = pontos_se_correr; // <<< CORREÇÃO 1
        return (sock_pediu == sock1 ? 1 : 2); // J1 ou J2 ganha
    }
    else if (msg_resposta == resposta_valida[2] && !resposta_valida[2].empty()) { // AUMENTOU
        // Inverte os papéis e chama a função recursivamente
        return gerirPedidoTruco(sock_responde, sock_pediu, valor_mao, nivel + 1, sock1);
    }
    else {
        // Resposta inválida
        send(sock_responde, "INFO:Resposta inválida. Considerado 'CORRO'.\n", 43, 0);
        valor_mao = pontos_se_correr; // <<< CORREÇÃO 1
        return (sock_pediu == sock1 ? 1 : 2); // J1 ou J2 ganha
    }
}

/**
 * @brief Thread principal para cada sala.
 * (Máquina de estados completa do jogo)
 */
/**
 * @brief Thread principal para cada sala.
 * (Máquina de estados completa do jogo)
 */
/**
 * @brief Thread principal para cada sala.
 * (Implementa a lógica do jogo e o PLACAR TOTAL)
 */
/**
 * @brief Thread principal para cada sala.
 * (Implementa a lógica da FLOR que não termina a mão)
 */
void* handle_room(void* arg) {
    Room* room = (Room*)arg;
    int sock1 = -1, sock2 = -1;

    int pontos_jogo_j1 = 0;
    int pontos_jogo_j2 = 0;
    int proximo_a_comecar_mao_sock = -1;

    while (true) {
        pthread_mutex_lock(&room->mutex);
        while (room->client_count < CLIENTS_PER_ROOM) {
            pthread_cond_wait(&room->cond_full, &room->mutex);
        }
        
        sock1 = room->client_sockets[0];
        sock2 = room->client_sockets[1];
        
        pthread_mutex_unlock(&room->mutex); 

        // --- 2. Preparação da Mão ---
        Baralho baralho;
        baralho.embaralhar();

        std::map<int, std::vector<Carta>> maos;
        maos[sock1] = baralho.distribuir(3);
        maos[sock2] = baralho.distribuir(3);

        std::string maoStr1 = "HAND:" + serializarMao(maos[sock1]) + "\n";
        std::string maoStr2 = "HAND:" + serializarMao(maos[sock2]) + "\n";
        send(sock1, maoStr1.c_str(), maoStr1.length(), 0);
        send(sock2, maoStr2.c_str(), maoStr2.length(), 0);

        int valor_mao = 1; // Pontos da mão (best-of-three)
        int ultimo_pediu_truco = 0; 
        int rodadas_j1 = 0; 
        int rodadas_j2 = 0;

        int quem_comeca_rodada;
        if (proximo_a_comecar_mao_sock == -1) { quem_comeca_rodada = sock1; }
        else { quem_comeca_rodada = proximo_a_comecar_mao_sock; }
        proximo_a_comecar_mao_sock = quem_comeca_rodada; 
        
        int vencedor_mao = 0; 

        // --- NOVO: Verificação de FLOR (Pré-Rodada) ---
        bool j1_tem_flor = temFlor(maos[sock1]);
        bool j2_tem_flor = temFlor(maos[sock2]);

        if (j1_tem_flor && !j2_tem_flor) {
            pontos_jogo_j1 += 3;
            send(sock1, "INFO:Você tem Flor! Ganha 3 pontos.\n", 37, 0);
            send(sock2, "INFO:Oponente tem Flor! Ele ganha 3 pontos.\n", 45, 0);
        } else if (j2_tem_flor && !j1_tem_flor) {
            pontos_jogo_j2 += 3;
            send(sock2, "INFO:Você tem Flor! Ganha 3 pontos.\n", 37, 0);
            send(sock1, "INFO:Oponente tem Flor! Ele ganha 3 pontos.\n", 45, 0);
        } else if (j1_tem_flor && j2_tem_flor) {
            enviarParaAmbos(sock1, sock2, "INFO:Flor contra Flor! Ninguém pontua.\n");
        }
        // (Se nenhum tem flor, nada acontece)
        // --- Fim da Verificação de FLOR ---


        // --- NOVO: Envia o placar atualizado (com pontos da Flor) ANTES da mão ---
        std::string placar_j1_pre = "PLACAR_JOGO:" + std::to_string(pontos_jogo_j1) + "," + std::to_string(pontos_jogo_j2) + "\n";
        std::string placar_j2_pre = "PLACAR_JOGO:" + std::to_string(pontos_jogo_j2) + "," + std::to_string(pontos_jogo_j1) + "\n";
        send(sock1, placar_j1_pre.c_str(), placar_j1_pre.length(), 0);
        send(sock2, placar_j2_pre.c_str(), placar_j2_pre.length(), 0);


        // --- 3. Loop das 3 Rodadas ("Tricks") ---
        // (Este loop agora corre sempre, mesmo que tenha havido Flor)
        for (int rodada = 1; rodada <= 3; rodada++) {
            
            int primeiro_sock = quem_comeca_rodada;
            int segundo_sock = (primeiro_sock == sock1) ? sock2 : sock1;
            Carta carta_primeiro, carta_segundo;
            bool turno_primeiro_terminado = false;
            bool turno_segundo_terminado = false;
            
            // --- Turno do primeiro jogador ---
            while (!turno_primeiro_terminado) {
                send(primeiro_sock, "SUA_VEZ\n", 8, 0);
                std::string msg_cliente;
                if (!lerDoSocket(primeiro_sock, msg_cliente)) { vencedor_mao = (primeiro_sock == sock1 ? 2 : 1); break; }

                if (msg_cliente.rfind("JOGAR:", 0) == 0) {
                    int index = std::stoi(msg_cliente.substr(6));
                    if (index < 0 || index >= maos[primeiro_sock].size()) index = 0;
                    carta_primeiro = maos[primeiro_sock][index];
                    maos[primeiro_sock].erase(maos[primeiro_sock].begin() + index);
                    std::string msg_jogou = "VOCE_JOGOU:" + std::to_string((int)carta_primeiro.valor) + "," + std::to_string((int)carta_primeiro.naipe) + "\n";
                    std::string msg_oponente = "OPONENTE_JOGOU:" + std::to_string((int)carta_primeiro.valor) + "," + std::to_string((int)carta_primeiro.naipe) + "\n";
                    send(primeiro_sock, msg_jogou.c_str(), msg_jogou.length(), 0);
                    send(segundo_sock, msg_oponente.c_str(), msg_oponente.length(), 0);
                    turno_primeiro_terminado = true;
                
                } else if (msg_cliente == "TRUCO" && valor_mao == 1 && (ultimo_pediu_truco == 2 || ultimo_pediu_truco == 0)) {
                    ultimo_pediu_truco = (primeiro_sock == sock1 ? 1 : 2);
                    vencedor_mao = gerirPedidoTruco(primeiro_sock, segundo_sock, valor_mao, 1, sock1);
                    if (vencedor_mao != 0) break;
                
                } else { send(primeiro_sock, "INFO:Comando inválido.\n", 22, 0); }
            }
            if (vencedor_mao != 0) break; 

            // --- Turno do segundo jogador ---
            while (!turno_segundo_terminado) {
                send(segundo_sock, "SUA_VEZ\n", 8, 0);
                std::string msg_cliente;
                if (!lerDoSocket(segundo_sock, msg_cliente)) { vencedor_mao = (segundo_sock == sock1 ? 2 : 1); break; }

                if (msg_cliente.rfind("JOGAR:", 0) == 0) {
                    int index = std::stoi(msg_cliente.substr(6));
                    if (index < 0 || index >= maos[segundo_sock].size()) index = 0;
                    carta_segundo = maos[segundo_sock][index];
                    maos[segundo_sock].erase(maos[segundo_sock].begin() + index);
                    std::string msg_jogou = "VOCE_JOGOU:" + std::to_string((int)carta_segundo.valor) + "," + std::to_string((int)carta_segundo.naipe) + "\n";
                    std::string msg_oponente = "OPONENTE_JOGOU:" + std::to_string((int)carta_segundo.valor) + "," + std::to_string((int)carta_segundo.naipe) + "\n";
                    send(segundo_sock, msg_jogou.c_str(), msg_jogou.length(), 0);
                    send(primeiro_sock, msg_oponente.c_str(), msg_oponente.length(), 0);
                    turno_segundo_terminado = true;
                
                } else if (msg_cliente == "TRUCO" && valor_mao == 1 && (ultimo_pediu_truco == 1 || ultimo_pediu_truco == 0)) {
                    ultimo_pediu_truco = (segundo_sock == sock1 ? 1 : 2);
                    vencedor_mao = gerirPedidoTruco(segundo_sock, primeiro_sock, valor_mao, 1, sock1);
                    if (vencedor_mao != 0) break;
                
                } else { send(segundo_sock, "INFO:Comando inválido.\n", 22, 0); }
            }
            if (vencedor_mao != 0) break; 

            // --- 4. Comparação da Rodada ---
            int resultado = compararCartas(carta_primeiro, carta_segundo);

            if (resultado == 1) { 
                if (primeiro_sock == sock1) rodadas_j1++; else rodadas_j2++; 
                quem_comeca_rodada = primeiro_sock;
                send(primeiro_sock, "INFO_JOGADA:Voce ganhou a rodada!\n", 32, 0);
                send(segundo_sock, "INFO_JOGADA:Voce perdeu a rodada.\n", 31, 0);
            } else if (resultado == -1) { 
                if (segundo_sock == sock1) rodadas_j1++; else rodadas_j2++; 
                quem_comeca_rodada = segundo_sock;
                send(segundo_sock, "INFO_JOGADA:Voce ganhou a rodada!\n", 32, 0);
                send(primeiro_sock, "INFO_JOGADA:Voce perdeu a rodada.\n", 31, 0);
            } else { 
                quem_comeca_rodada = primeiro_sock;
                send(primeiro_sock, "INFO_JOGADA:Empatou (cangou)!\n", 29, 0);
                send(segundo_sock, "INFO_JOGADA:Empatou (cangou)!\n", 29, 0);
            }

            std::string placar_m1 = "PLACAR:" + std::to_string(rodadas_j1) + "," + std::to_string(rodadas_j2) + "\n";
            std::string placar_m2 = "PLACAR:" + std::to_string(rodadas_j2) + "," + std::to_string(rodadas_j1) + "\n";
            send(sock1, placar_m1.c_str(), placar_m1.length(), 0);
            send(sock2, placar_m2.c_str(), placar_m2.length(), 0);

            // --- 5. Verifica se a "Mão" acabou ---
            if (rodadas_j1 == 2 || (rodada == 3 && rodadas_j1 > rodadas_j2)) {
                vencedor_mao = 1; break;
            }
            if (rodadas_j2 == 2 || (rodada == 3 && rodadas_j2 > rodadas_j1)) {
                vencedor_mao = 2; break;
            }
            if (rodada == 3 && rodadas_j1 == rodadas_j2) {
                vencedor_mao = 0; break;
            }
        } // Fim do loop das 3 rodadas

        // --- 6. Atualiza Placar Total do JOGO ---
        // (Agora, esta secção SÓ adiciona os pontos da MÃO)
        if (vencedor_mao == 1) {
            pontos_jogo_j1 += valor_mao;
            proximo_a_comecar_mao_sock = sock1; 
            enviarParaAmbos(sock1, sock2, "FIM_MAO:Jogador 1 ganhou a mao!\n");
        } else if (vencedor_mao == 2) {
            pontos_jogo_j2 += valor_mao;
            proximo_a_comecar_mao_sock = sock2; 
            enviarParaAmbos(sock1, sock2, "FIM_MAO:Jogador 2 ganhou a mao!\n");
        } else if (vencedor_mao == 0) {
            enviarParaAmbos(sock1, sock2, "FIM_MAO:Empatou a mao! Ninguem pontua.\n");
        }
        
        std::string placar_j1_pos = "PLACAR_JOGO:" + std::to_string(pontos_jogo_j1) + "," + std::to_string(pontos_jogo_j2) + "\n";
        std::string placar_j2_pos = "PLACAR_JOGO:" + std::to_string(pontos_jogo_j2) + "," + std::to_string(pontos_jogo_j1) + "\n";
        send(sock1, placar_j1_pos.c_str(), placar_j1_pos.length(), 0);
        send(sock2, placar_j2_pos.c_str(), placar_j2_pos.length(), 0);

        // --- 7. Verifica Fim de JOGO ---
        if (pontos_jogo_j1 >= PONTOS_VITORIA || pontos_jogo_j2 >= PONTOS_VITORIA) {
            std::string msg_vitoria_j1 = "FIM_JOGO:Jogador 1 venceu o jogo!\n";
            std::string msg_vitoria_j2 = "FIM_JOGO:Jogador 2 venceu o jogo!\n";
            
            if (pontos_jogo_j1 > pontos_jogo_j2) {
                enviarParaAmbos(sock1, sock2, msg_vitoria_j1);
            } else {
                enviarParaAmbos(sock1, sock2, msg_vitoria_j2);
            }
            break; 
        }

        sleep(5); 
    } 

    // --- 8. Limpeza Final ---
    close(sock1);
    close(sock2);
    
    pthread_mutex_lock(&room->mutex);
    room->client_count = 0;
    room->client_sockets[0] = -1;
    room->client_sockets[1] = -1;
    pthread_mutex_unlock(&room->mutex);

    return NULL;
}

// --- Funções main() e assign_client_to_room() ---
// (Estas funções permanecem exatamente iguais ao código original)
// ... (copie e cole o 'main' e 'assign_client_to_room' da sua versão anterior aqui) ...
/**
 * @brief Atribui um novo cliente a uma sala disponível.
 */
void assign_client_to_room(int client_socket) {
    for (int i = 0; i < NUM_ROOMS; i++) {
        pthread_mutex_lock(&rooms[i].mutex);

        if (rooms[i].client_count < CLIENTS_PER_ROOM) {
            int slot = rooms[i].client_count;
            rooms[i].client_sockets[slot] = client_socket;
            rooms[i].client_count++;

            std::cout << "Cliente (socket " << client_socket << ") atribuído à sala " << i << " no slot " << slot << std::endl;

            // Prepara mensagem de espera
            char msg[100];
            int players_needed = CLIENTS_PER_ROOM - rooms[i].client_count;
            sprintf(msg, "Você está na sala %d. Esperando por mais %d jogador(es)...\n", i, players_needed);
            send(client_socket, msg, strlen(msg), 0);

            // Se a sala acabou de encher, sinaliza a thread da sala
            if (rooms[i].client_count == CLIENTS_PER_ROOM) {
                pthread_cond_signal(&rooms[i].cond_full);
            }
            
            pthread_mutex_unlock(&rooms[i].mutex);
            return; // Cliente foi atribuído
        }

        pthread_mutex_unlock(&rooms[i].mutex);
    }

    // Se o loop terminar, todas as salas estão cheias
    std::cout << "Servidor cheio. Rejeitando cliente (socket " << client_socket << ")" << std::endl;
    const char* full_msg = "Servidor cheio. Tente novamente mais tarde.\n";
    send(client_socket, full_msg, strlen(full_msg), 0);
    close(client_socket);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    pthread_t room_threads[NUM_ROOMS];

    // 1. Inicializa as salas
    for (int i = 0; i < NUM_ROOMS; i++) {
        rooms[i].id = i;
        rooms[i].client_count = 0;
        pthread_mutex_init(&rooms[i].mutex, NULL);
        pthread_cond_init(&rooms[i].cond_full, NULL);

        // 2. Cria as threads persistentes das salas
        if (pthread_create(&room_threads[i], NULL, handle_room, (void*)&rooms[i]) != 0) {
            perror("pthread_create (room)");
            exit(EXIT_FAILURE);
        }
    }

    // 3. Configura o socket do servidor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Permite reuso do endereço e porta
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) { // 5 é o backlog (fila de conexões pendentes)
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "Servidor ouvindo na porta " << PORT << std::endl;
    std::cout << "Esperando conexões..." << std::endl;

    // 4. Loop principal de aceitação de clientes
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue; // Continua para a próxima tentativa
        }

        std::cout << "Nova conexão aceita! (socket " << new_socket << ")" << std::endl;
        
        // 5. Atribui o cliente a uma sala
        assign_client_to_room(new_socket);
    }

    // (Opcional) Limpeza ao final - este código nunca será atingido em um loop infinito
    for (int i = 0; i < NUM_ROOMS; i++) {
        pthread_join(room_threads[i], NULL);
        pthread_mutex_destroy(&rooms[i].mutex);
        pthread_cond_destroy(&rooms[i].cond_full);
    }
    close(server_fd);

    return 0;
}
