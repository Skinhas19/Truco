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
#include "cartas.h"
#include "regras.h"
#include <mutex>
#include <algorithm> 

#define PORT 8080
#define BUFFER_SIZE 1024

std::mutex g_mutex;
bool g_minhaVez = false;
std::vector<Carta> g_minhaMao;

// <<< MUDANÇA AQUI: Novos estados para o prompt
enum class EstadoResposta { 
    NENHUMA, 
    JOGADA_APENAS,      // Só pode jogar carta
    JOGADA_OU_TRUCO,    // Pode jogar carta OU pedir truco
    RESPOSTA_TRUCO, 
    RESPOSTA_RETRUCO, 
    RESPOSTA_VALE4 
};
EstadoResposta g_estadoEsperado = EstadoResposta::NENHUMA;

// <<< MUDANÇA AQUI: O prompt agora depende dos novos estados
void reimprimirPrompt() {
    std::cout << "\r" << std::string(80, ' ') << "\r"; 
    
    if (g_estadoEsperado == EstadoResposta::JOGADA_OU_TRUCO) {
        std::cout << "Digite [numero da carta] ou [truco]: ";
    } else if (g_estadoEsperado == EstadoResposta::JOGADA_APENAS) {
        std::cout << "Digite [numero da carta]: "; // <<< A CORREÇÃO
    } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_TRUCO) {
        std::cout << "Responda com [aceito], [corro] ou [retruco]: ";
    } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_RETRUCO) {
        std::cout << "Responda com [aceito], [corro] ou [vale 4]: ";
    } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_VALE4) {
        std::cout << "Responda com [aceito] ou [corro]: ";
    } else {
        std::cout << "> "; 
    }
    fflush(stdout);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

void* receive_messages(void* arg) {
    int sock = *(int*)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;
    std::string msgBuffer = "";

    // <<< MUDANÇA AQUI: Controla se a aposta já foi aceite nesta mão
    bool aposta_aceita_nesta_mao = false;

    while ((bytes_read = read(sock, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[bytes_read] = '\0';
        msgBuffer += buffer; 

        size_t pos;
        while ((pos = msgBuffer.find('\n')) != std::string::npos) {
            std::string mensagem = msgBuffer.substr(0, pos);
            msgBuffer.erase(0, pos + 1); 

            std::lock_guard<std::mutex> lock(g_mutex); 
            std::cout << "\r" << std::string(80, ' ') << "\r"; 

            try {
                if (mensagem.rfind("HAND:", 0) == 0) {
                    aposta_aceita_nesta_mao = false; // <<< MUDANÇA: Reseta aposta
                    std::cout << "\n=== SUA MÃO ===" << std::endl;
                    // ... (resto do código de HAND:) ...
                    g_minhaMao.clear();
                    std::string maoData = mensagem.substr(5); 
                    std::stringstream ss_mao(maoData);
                    std::string segmentoCarta;
                    int i = 1;
                    while (std::getline(ss_mao, segmentoCarta, ';')) {
                        std::stringstream ss_carta(segmentoCarta); std::string s;
                        int val, nai;
                        std::getline(ss_carta, s, ','); val = std::stoi(s);
                        std::getline(ss_carta, s, ','); nai = std::stoi(s);
                        Carta c((Valor)val, (Naipe)nai);
                        g_minhaMao.push_back(c);
                        std::cout << i++ << ". " << Tradutor::toString(c) << std::endl;
                    }
                    std::cout << "================" << std::endl;
                
                } else if (mensagem.rfind("SUA_VEZ", 0) == 0) {
                    g_minhaVez = true;
                    // <<< MUDANÇA AQUI: Decide qual estado de JOGADA usar
                    if (g_estadoEsperado == EstadoResposta::NENHUMA) {
                        if (aposta_aceita_nesta_mao) {
                            g_estadoEsperado = EstadoResposta::JOGADA_APENAS;
                        } else {
                            g_estadoEsperado = EstadoResposta::JOGADA_OU_TRUCO;
                        }
                    }
                    std::cout << "\n--- SUA VEZ ---" << std::endl;
                
                } else if (mensagem.rfind("PEDIDO_TRUCO", 0) == 0) {
                    g_minhaVez = true;
                    g_estadoEsperado = EstadoResposta::RESPOSTA_TRUCO;
                    std::cout << "\n!!! OPONENTE PEDIU TRUCO !!!" << std::endl;
                
                } else if (mensagem.rfind("PEDIDO_RETRUCO", 0) == 0) {
                    g_minhaVez = true;
                    g_estadoEsperado = EstadoResposta::RESPOSTA_RETRUCO;
                    std::cout << "\n!!! OPONENTE PEDIU RETRUCO !!!" << std::endl;
                
                } else if (mensagem.rfind("PEDIDO_VALE_QUATRO", 0) == 0) {
                    g_minhaVez = true;
                    g_estadoEsperado = EstadoResposta::RESPOSTA_VALE4;
                    std::cout << "\n!!! OPONENTE PEDIU VALE QUATRO !!!" << std::endl;
                
                } else if (mensagem.rfind("VOCE_JOGOU:", 0) == 0) {
                    // ... (código de VOCE_JOGOU:) ...
                    std::string data = mensagem.substr(11); std::stringstream ss_data(data);
                    std::string s; int val, nai;
                    std::getline(ss_data, s, ','); val = std::stoi(s);
                    std::getline(ss_data, s, ','); nai = std::stoi(s);
                    Carta c_jogada((Valor)val, (Naipe)nai);
                    std::cout << "Você jogou: " << Tradutor::toString(c_jogada) << std::endl;
                    for (auto it = g_minhaMao.begin(); it != g_minhaMao.end(); ++it) {
                        if (it->valor == c_jogada.valor && it->naipe == c_jogada.naipe) {
                            g_minhaMao.erase(it); break;
                        }
                    }
                
                } else if (mensagem.rfind("OPONENTE_JOGOU:", 0) == 0) {
                    // (Mantendo o seu bloco de código)
                    std::string data = mensagem.substr(15); 
                    size_t pos = data.find(',');
                    int val = std::stoi(data.substr(0, pos));
                    int nai = std::stoi(data.substr(pos + 1));
                    std::cout << "Oponente jogou: " << Tradutor::toString(Carta((Valor)val, (Naipe)nai)) << std::endl;

                } else if (mensagem.rfind("PLACAR:", 0) == 0) {
                    // ... (código de PLACAR:) ...
                    std::string data = mensagem.substr(7); std::stringstream ss_data(data);
                    std::string s; int meu, op;
                    std::getline(ss_data, s, ','); meu = std::stoi(s);
                    std::getline(ss_data, s, ','); op = std::stoi(s);
                    std::cout << "Placar da Mão: Você " << meu << " x " << op << " Oponente" << std::endl;
                
                } else if (mensagem.rfind("PLACAR_JOGO:", 0) == 0) {
                    // ... (código de PLACAR_JOGO:) ...
                    std::string data = mensagem.substr(12); std::stringstream ss_data(data);
                    std::string s; int meu, op;
                    std::getline(ss_data, s, ','); meu = std::stoi(s);
                    std::getline(ss_data, s, ','); op = std::stoi(s);
                    std::cout << "\n=================================" << std::endl;
                    std::cout << "  PLACAR DO JOGO: Voce " << meu << " x " << op << " Oponente" << std::endl;
                    std::cout << "=================================\n" << std::endl;
                
                } else if (mensagem.rfind("FIM_MAO:", 0) == 0) {
                    g_minhaVez = false;
                    g_estadoEsperado = EstadoResposta::NENHUMA;
                    std::cout << "\n*** " << mensagem.substr(8) << " ***" << std::endl;
                    std::cout << "Aguardando próxima mão...\n" << std::endl;
                
                } else if (mensagem.rfind("FIM_JOGO:", 0) == 0) {
                    // ... (código de FIM_JOGO:) ...
                    std::cout << "\n*********************************" << std::endl;
                    std::cout << "*** " << mensagem.substr(9) << " ***" << std::endl;
                    std::cout << "*********************************" << std::endl;
                    close(sock);
                    exit(0); 
                
                } else if (mensagem.rfind("INFO:Aposta aceite!", 0) == 0) { // <<< MUDANÇA
                    aposta_aceita_nesta_mao = true;
                    std::cout << "INFO: " << mensagem.substr(5) << std::endl;
                
                } else {
                    std::cout << "INFO: " << mensagem.substr(5) << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "[ERRO DE PARSING]: " << e.what() << " em " << mensagem << std::endl;
            }

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

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    // ... (código de socket(), memset(), inet_pton(), connect()) ...
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { std::cout << "Socket error" << std::endl; return -1; }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) { std::cout << "Addr error" << std::endl; return -1; }
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) { std::cout << "Connect error" << std::endl; return -1; }
    std::cout << "Conectado ao servidor!" << std::endl;

    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, (void*)&sock) != 0) { perror("pthread_create"); return -1; }

    { std::lock_guard<std::mutex> lock(g_mutex); reimprimirPrompt(); }

    std::string line;
    while (std::getline(std::cin, line)) { 
        std::string comando = toLower(line); 
        std::lock_guard<std::mutex> lock(g_mutex); 
        
        if (!g_minhaVez) {
            std::cout << "\rAguarde sua vez..." << std::endl;
            reimprimirPrompt();
            continue;
        }

        std::string msg_enviar = "";

        // <<< MUDANÇA AQUI: Lógica de envio atualizada
        if (g_estadoEsperado == EstadoResposta::JOGADA_OU_TRUCO) {
            if (comando == "truco") {
                msg_enviar = "TRUCO\n";
            } else {
                try {
                    int card_num = std::stoi(comando); 
                    if (card_num >= 1 && card_num <= g_minhaMao.size()) {
                        msg_enviar = "JOGAR:" + std::to_string(card_num - 1) + "\n";
                    }
                } catch (...) { /* ignora */ }
            }
        
        } else if (g_estadoEsperado == EstadoResposta::JOGADA_APENAS) {
            // Só aceita jogar carta
            try {
                int card_num = std::stoi(comando); 
                if (card_num >= 1 && card_num <= g_minhaMao.size()) {
                    msg_enviar = "JOGAR:" + std::to_string(card_num - 1) + "\n";
                }
            } catch (...) { /* ignora */ }
            // Se digitar "truco" aqui, msg_enviar fica vazia e cai no "Comando inválido"

        } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_TRUCO) {
            if (comando == "aceito") msg_enviar = "ACEITO\n";
            else if (comando == "corro") msg_enviar = "CORRO\n";
            else if (comando == "retruco") msg_enviar = "RETRUCO\n";
        
        } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_RETRUCO) {
            if (comando == "aceito") msg_enviar = "ACEITO\n";
            else if (comando == "corro") msg_enviar = "CORRO\n";
            else if (comando == "vale 4" || comando == "valequatro" || comando == "vale4") msg_enviar = "VALE_QUATRO\n";
        
        } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_VALE4) {
            if (comando == "aceito") msg_enviar = "ACEITO\n";
            else if (comando == "corro") msg_enviar = "CORRO\n";
        }
        // --- FIM DA LÓGICA DE ESTADO ---

        if (!msg_enviar.empty()) {
            send(sock, msg_enviar.c_str(), msg_enviar.length(), 0);
            g_minhaVez = false;
            g_estadoEsperado = EstadoResposta::NENHUMA; 
        } else {
            std::cout << "\rComando inválido." << std::endl;
            reimprimirPrompt();
        }
    }

    close(sock);
    std::cout << "Desconectado." << std::endl;
    return 0;
}
