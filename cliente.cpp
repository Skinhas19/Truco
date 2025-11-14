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
#include "cartas.h" // As suas definições de cartas
#include "regras.h" // A sua lógica de 'getForca'
#include <mutex>    // Para std::mutex

// --- DEFINIÇÕES QUE FALTAVAM ---
#define PORT 8080
#define BUFFER_SIZE 1024
// -------------------------------

// Variáveis globais para controlo de estado
std::mutex g_mutex;
bool g_minhaVez = false;
std::vector<Carta> g_minhaMao;

/**
 * @brief Função de utilidade para reimprimir o prompt.
 * Deve ser chamada DEPOIS de ter travado o 'g_mutex'.
 */
void reimprimirPrompt() {
    if (g_minhaVez) {
        std::cout << "Digite o número da carta (1 a " << g_minhaMao.size() << "): ";
    } else {
        std::cout << "> "; // Um prompt simples se não for nossa vez
    }
    fflush(stdout); // Força a impressão
}

/**
 * @brief Thread para receber e interpretar mensagens do servidor.
 */
/**
 * @brief Thread para receber e interpretar mensagens do servidor.
 * (Versão corrigida com parser C++ e limpeza de linha)
 */
void* receive_messages(void* arg) {
    int sock = *(int*)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;
    std::string msgBuffer = "";

    while ((bytes_read = read(sock, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[bytes_read] = '\0';
        msgBuffer += buffer; 

        size_t pos;
        while ((pos = msgBuffer.find('\n')) != std::string::npos) {
            std::string mensagem = msgBuffer.substr(0, pos);
            msgBuffer.erase(0, pos + 1); 

            std::lock_guard<std::mutex> lock(g_mutex); 

            std::cout << "\r" << std::string(80, ' ') << "\r";

            // ----- Lógica de Interpretação (Corrigida) -----

            try { // Adiciona um try-catch para evitar que o cliente quebre
                if (mensagem.rfind("HAND:", 0) == 0) {
                    std::cout << "\n=== SUA MÃO ===" << std::endl;
                    g_minhaMao.clear();
                    std::string maoData = mensagem.substr(5); 
                    std::stringstream ss_mao(maoData);
                    std::string segmentoCarta;
                    int i = 1;
                    while (std::getline(ss_mao, segmentoCarta, ';')) { // "VAL,NAI"
                        std::stringstream ss_carta(segmentoCarta);
                        std::string segmentoNum;
                        int val, nai;

                        std::getline(ss_carta, segmentoNum, ',');
                        val = std::stoi(segmentoNum);
                        std::getline(ss_carta, segmentoNum, ',');
                        nai = std::stoi(segmentoNum);
                        
                        Carta c((Valor)val, (Naipe)nai);
                        g_minhaMao.push_back(c);
                        std::cout << i++ << ". " << Tradutor::toString(c) << std::endl;
                    }
                    std::cout << "================" << std::endl;
                
                } else if (mensagem.rfind("SUA_VEZ", 0) == 0) {
                    g_minhaVez = true;
                    std::cout << "\n--- SUA VEZ ---" << std::endl;
                
                } else if (mensagem.rfind("VOCE_JOGOU:", 0) == 0) {
                    std::string data = mensagem.substr(11); // "VAL,NAI"
                    std::stringstream ss_data(data);
                    std::string segmento;
                    int val, nai;

                    std::getline(ss_data, segmento, ','); val = std::stoi(segmento);
                    std::getline(ss_data, segmento, ','); nai = std::stoi(segmento);
                    
                    Carta c_jogada((Valor)val, (Naipe)nai);
                    std::cout << "Você jogou: " << Tradutor::toString(c_jogada) << std::endl;
                    for (auto it = g_minhaMao.begin(); it != g_minhaMao.end(); ++it) {
                        if (it->valor == c_jogada.valor && it->naipe == c_jogada.naipe) {
                            g_minhaMao.erase(it);
                            break;
                        }
                    }
                
                } else if (mensagem.rfind("OPONENTE_JOGOU:", 0) == 0) {
                    std::string data = mensagem.substr(16); // "VAL,NAI"
                    std::stringstream ss_data(data);
                    std::string segmento;
                    int val, nai;

                    std::getline(ss_data, segmento, ','); val = std::stoi(segmento);
                    std::getline(ss_data, segmento, ','); nai = std::stoi(segmento);

                    std::cout << "Oponente jogou: " << Tradutor::toString(Carta((Valor)val, (Naipe)nai)) << std::endl;
                
                } else if (mensagem.rfind("INFO_JOGADA:", 0) == 0) {
                    std::cout << ">> " << mensagem.substr(12) << std::endl;
                
                } else if (mensagem.rfind("PLACAR:", 0) == 0) {
                    std::string data = mensagem.substr(7); // "P1,P2"
                    std::stringstream ss_data(data);
                    std::string segmento;
                    int meu_placar, op_placar;

                    std::getline(ss_data, segmento, ','); meu_placar = std::stoi(segmento);
                    std::getline(ss_data, segmento, ','); op_placar = std::stoi(segmento);

                    std::cout << "Placar da Mão: Você " << meu_placar << " x " << op_placar << " Oponente" << std::endl;
                
                } else if (mensagem.rfind("FIM_MAO:", 0) == 0) {
                    g_minhaVez = false;
                    std::cout << "\n*** " << mensagem.substr(8) << " ***" << std::endl;
                    std::cout << "Aguardando próxima mão...\n" << std::endl;
                
                } else {
                    std::cout << mensagem << std::endl;
                }
            } catch (const std::exception& e) {
                // Se algo der errado ao ler os números, imprime um erro
                std::cout << "[ERRO DE PARSING]: " << e.what() << " em " << mensagem << std::endl;
            }

            // Reimprime o prompt
            reimprimirPrompt();
        }
    }
    if (bytes_read == 0) std::cout << "\nServidor desconectou." << std::endl;
    else perror("read");
    std::cout << "Pressione Enter para sair..." << std::endl;
    close(sock);
    exit(0); 
    return NULL;
}
/**
 * @brief Thread principal (envio).
 */
int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cout << "Erro ao criar socket" << std::endl; return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cout << "Endereço inválido" << std::endl; return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "Conexão falhou" << std::endl; return -1;
    }

    std::cout << "Conectado ao servidor!" << std::endl;

    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, (void*)&sock) != 0) {
        perror("pthread_create (recv)"); return -1;
    }

    // Imprime o prompt inicial
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        reimprimirPrompt();
    }

    std::string line;
    while (std::getline(std::cin, line)) { // Lê do stdin
        
        std::lock_guard<std::mutex> lock(g_mutex); 
        
        if (g_minhaVez) {
            try {
                int card_num = std::stoi(line); 
                if (card_num >= 1 && card_num <= g_minhaMao.size()) {
                    int card_index = card_num - 1;
                    
                    std::string msg = "JOGAR:" + std::to_string(card_index) + "\n";
                    send(sock, msg.c_str(), msg.length(), 0);
                    
                    g_minhaVez = false; // Já joguei

                } else {
                    std::cout << "Número inválido. Digite de 1 a " << g_minhaMao.size() << ": ";
                    fflush(stdout);
                }
            } catch (...) {
                std::cout << "Comando inválido. Digite um número: ";
                fflush(stdout);
            }
        } else if (line == "quit") {
            break;
        } else {
            // O usuário apertou Enter quando não era sua vez
            std::cout << "\rAguarde sua vez..." << std::endl;
            reimprimirPrompt(); // Reimprime o prompt (ex: "> ")
        }
    }

    close(sock);
    std::cout << "Desconectado." << std::endl;
    return 0;
}
