#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "regras.h"
#include <map>
#include "cartas.h"
#include "baralho.h"
#include <sstream>    // Para construir strings
#include <vector>     // Para std::vector
#define PORT 8080
#define NUM_ROOMS 2
#define CLIENTS_PER_ROOM 2
#define BUFFER_SIZE 1024

// Estrutura para retransmitir mensagens entre dois clientes
struct RelayArgs {
    int from_sock;
    int to_sock;
};

// Estrutura para gerenciar uma sala
struct Room {
    int id;
    int client_sockets[CLIENTS_PER_ROOM];
    int client_count;
    pthread_mutex_t mutex;
    pthread_cond_t cond_full; // Sinaliza quando a sala está cheia
};

// Array global de salas
Room rooms[NUM_ROOMS];

/**
 * @brief Thread para retransmitir mensagens de um socket para outro.
 * Esta é a "lógica do jogo" da sala.
 */
void* relay_messages(void* arg) {
    RelayArgs* args = (RelayArgs*)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = read(args->from_sock, buffer, BUFFER_SIZE)) > 0) {
        // Envia os dados lidos para o outro cliente
        send(args->to_sock, buffer, bytes_read, 0);
    }

    // Se read() retornar 0 ou -1, o cliente desconectou
    std::cout << "Cliente (socket " << args->from_sock << ") desconectou. Fechando relay." << std::endl;
    
    // Fecha o socket de leitura
    close(args->from_sock);
    // Envia um sinal de "fim de escrita" para o outro socket
    // Isso fará com que o read() da *outra* thread de relay retorne 0
    shutdown(args->to_sock, SHUT_WR); 

    return NULL;
}
/**
 * @brief Converte um vetor de Cartas em uma string para envio.
 * Formato: "VAL1,NAIPE1;VAL2,NAIPE2;..."
 */
std::string serializarMao(const std::vector<Carta>& mao) {
    std::stringstream ss;
    for (size_t i = 0; i < mao.size(); ++i) {
        // Converte o enum para int
        ss << (int)mao[i].valor << "," << (int)mao[i].naipe;
        if (i < mao.size() - 1) {
            ss << ";"; // Separador de cartas
        }
    }
    return ss.str();
}
// Função para ler uma linha (terminada em \n) do socket
bool lerDoSocket(int sock, std::string& msg) {
    char buffer[1];
    msg = "";
    while (read(sock, buffer, 1) > 0) {
        if (buffer[0] == '\n') {
            return true;
        }
        msg += buffer[0];
    }
    return false; // Cliente desconectou
}
/**
 * @brief Thread principal para cada sala.
 * (Implementa a lógica do jogo)
 */
void* handle_room(void* arg) {
    Room* room = (Room*)arg;
    int sock1 = -1, sock2 = -1;

    // Loop principal da sala (espera jogadores, joga uma "mão", reseta)
    while (true) {
        // --- 1. Espera a sala encher ---
        pthread_mutex_lock(&room->mutex);
        while (room->client_count < CLIENTS_PER_ROOM) {
            pthread_cond_wait(&room->cond_full, &room->mutex);
        }
        
        sock1 = room->client_sockets[0];
        sock2 = room->client_sockets[1];
        
        pthread_mutex_unlock(&room->mutex); 

        // --- 2. Preparação do Jogo ---
        Baralho baralho;
        baralho.embaralhar();

        // Guarda as mãos no servidor (a fonte da verdade)
        std::map<int, std::vector<Carta>> maos;
        maos[sock1] = baralho.distribuir(3);
        maos[sock2] = baralho.distribuir(3);

        // Envia as mãos para os clientes
        std::string maoStr1 = "HAND:" + serializarMao(maos[sock1]) + "\n";
        std::string maoStr2 = "HAND:" + serializarMao(maos[sock2]) + "\n";
        send(sock1, maoStr1.c_str(), maoStr1.length(), 0);
        send(sock2, maoStr2.c_str(), maoStr2.length(), 0);

        int pontos_j1 = 0;
        int pontos_j2 = 0;
        int quem_comeca_sock = sock1; // Jogador 1 sempre começa a primeira jogada

        // --- 3. Loop das 3 Jogadas ("Tricks") ---
        for (int jogada = 1; jogada <= 3; jogada++) {
            
            // Define quem joga primeiro e quem joga segundo nesta jogada
            int primeiro_sock = quem_comeca_sock;
            int segundo_sock = (primeiro_sock == sock1) ? sock2 : sock1;

            Carta carta_primeiro, carta_segundo;
            std::string msg_do_cliente;
            int index_carta;

            // --- Pega a jogada do primeiro jogador ---
            send(primeiro_sock, "SUA_VEZ\n", 8, 0);
            if (!lerDoSocket(primeiro_sock, msg_do_cliente) || msg_do_cliente.rfind("JOGAR:", 0) != 0) {
                break; // Cliente desconectou ou mandou msg inválida
            }
            index_carta = std::stoi(msg_do_cliente.substr(6));
            
            // Validação (pega a carta e remove da mão no servidor)
            if (index_carta < 0 || index_carta >= maos[primeiro_sock].size()) index_carta = 0; // Joga a 0 se inválido
            carta_primeiro = maos[primeiro_sock][index_carta];
            maos[primeiro_sock].erase(maos[primeiro_sock].begin() + index_carta);

            // Informa os clientes
            std::string msg_jogada_p1 = "VOCE_JOGOU:" + std::to_string((int)carta_primeiro.valor) + "," + std::to_string((int)carta_primeiro.naipe) + "\n";
            std::string msg_oponente_p1 = "OPONENTE_JOGOU:" + std::to_string((int)carta_primeiro.valor) + "," + std::to_string((int)carta_primeiro.naipe) + "\n";
            send(primeiro_sock, msg_jogada_p1.c_str(), msg_jogada_p1.length(), 0);
            send(segundo_sock, msg_oponente_p1.c_str(), msg_oponente_p1.length(), 0);

            // --- Pega a jogada do segundo jogador ---
            send(segundo_sock, "SUA_VEZ\n", 8, 0);
            if (!lerDoSocket(segundo_sock, msg_do_cliente) || msg_do_cliente.rfind("JOGAR:", 0) != 0) {
                break; // Cliente desconectou
            }
            index_carta = std::stoi(msg_do_cliente.substr(6));
            
            if (index_carta < 0 || index_carta >= maos[segundo_sock].size()) index_carta = 0;
            carta_segundo = maos[segundo_sock][index_carta];
            maos[segundo_sock].erase(maos[segundo_sock].begin() + index_carta);
            
            std::string msg_jogada_p2 = "VOCE_JOGOU:" + std::to_string((int)carta_segundo.valor) + "," + std::to_string((int)carta_segundo.naipe) + "\n";

std::string msg_oponente_p2 = "OPONENTE_JOGOU:" + std::to_string((int)carta_segundo.valor) + "," + std::to_string((int)carta_segundo.naipe) + "\n";
send(segundo_sock, msg_jogada_p2.c_str(), msg_jogada_p2.length(), 0);
            send(primeiro_sock, msg_oponente_p2.c_str(), msg_oponente_p2.length(), 0);

            // --- 4. Comparação e Pontuação da Jogada ---
            int resultado = compararCartas(carta_primeiro, carta_segundo);
            int vencedor_jogada_sock = -1;

            if (resultado == 1) { // Primeiro jogador ganhou a jogada
                vencedor_jogada_sock = primeiro_sock;
                send(primeiro_sock, "INFO_JOGADA:Voce ganhou a jogada!\n", 32, 0);
                send(segundo_sock, "INFO_JOGADA:Voce perdeu a jogada.\n", 31, 0);
            } else if (resultado == -1) { // Segundo jogador ganhou a jogada
                vencedor_jogada_sock = segundo_sock;
                send(segundo_sock, "INFO_JOGADA:Voce ganhou a jogada!\n", 32, 0);
                send(primeiro_sock, "INFO_JOGADA:Voce perdeu a jogada.\n", 31, 0);
            } else { // Empate
                vencedor_jogada_sock = -1; // Ninguém ganhou
                send(primeiro_sock, "INFO_JOGADA:Empatou (cangou)!\n", 29, 0);
                send(segundo_sock, "INFO_JOGADA:Empatou (cangou)!\n", 29, 0);
            }

            // Atualiza placar da "mão" e define quem começa a próxima
            if (vencedor_jogada_sock == sock1) {
                pontos_j1++;
                quem_comeca_sock = sock1;
            } else if (vencedor_jogada_sock == sock2) {
                pontos_j2++;
                quem_comeca_sock = sock2;
            } else {
                // Se empatou, quem começou a jogada anterior, começa de novo
                quem_comeca_sock = primeiro_sock;
            }

            // Envia o placar
            std::string placar_p1 = "PLACAR:" + std::to_string(pontos_j1) + "," + std::to_string(pontos_j2) + "\n";
            std::string placar_p2 = "PLACAR:" + std::to_string(pontos_j2) + "," + std::to_string(pontos_j1) + "\n";
            send(sock1, placar_p1.c_str(), placar_p1.length(), 0);
            send(sock2, placar_p2.c_str(), placar_p2.length(), 0);

            // --- 5. Verifica se a "Mão" acabou ---
            if (pontos_j1 == 2 || (jogada == 3 && pontos_j1 > pontos_j2)) {
                send(sock1, "FIM_MAO:Voce ganhou a mao!\n", 27, 0);
                send(sock2, "FIM_MAO:Voce perdeu a mao.\n", 27, 0);
                break; // Sai do loop das 3 jogadas
            }
            if (pontos_j2 == 2 || (jogada == 3 && pontos_j2 > pontos_j1)) {
                send(sock2, "FIM_MAO:Voce ganhou a mao!\n", 27, 0);
                send(sock1, "FIM_MAO:Voce perdeu a mao.\n", 27, 0);
                break; // Sai do loop das 3 jogadas
            }
            if (jogada == 3 && pontos_j1 == pontos_j2) { // Ex: 1-1 e empatou a 3a
                send(sock1, "FIM_MAO:Empatou a mao!\n", 24, 0);
                send(sock2, "FIM_MAO:Empatou a mao!\n", 24, 0);
                break; // Sai do loop das 3 jogadas
            }
        } // Fim do loop das 3 jogadas

        // --- 6. Limpeza da Sala ---
        // (Simula uma pausa antes de resetar para a próxima mão)
        sleep(5); // Espera 5 segundos

        // O loop 'while(true)' recomeça, distribuindo novas cartas
        // A lógica de desconexão já foi tratada pelos 'break'
    } // Fim do loop 'while(true)' da sala

    // Se saiu do loop (raro), fecha os sockets
    if(sock1 != -1) close(sock1);
    if(sock2 != -1) close(sock2);
    
    // Reseta a sala se algo quebrou
    pthread_mutex_lock(&room->mutex);
    room->client_count = 0;
    room->client_sockets[0] = -1;
    room->client_sockets[1] = -1;
    pthread_mutex_unlock(&room->mutex);

    return NULL;
}
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
