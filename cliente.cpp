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

// --- Variáveis Globais ---
std::mutex g_mutex;
bool g_minhaVez = false;
std::vector<Carta> g_minhaMao;
bool g_florOcorreu = false; // Flag para bloquear envido se houve flor
bool g_envidoOcorreu = false;
enum class EstadoResposta { 
    NENHUMA, 
    JOGADA_APENAS, 
    JOGADA_OU_TRUCO, 
    JOGADA_OU_TRUCO_OU_ENVIDO, // NOVO: Permite Envido
    RESPOSTA_TRUCO, 
    RESPOSTA_RETRUCO, 
    RESPOSTA_VALE4,
    RESPOSTA_ENVIDO,           // NOVO: Resposta a Envido
    RESPOSTA_REAL_ENVIDO       // NOVO: Resposta a Real Envido
};
EstadoResposta g_estadoEsperado = EstadoResposta::NENHUMA;

// --- Funções Auxiliares ---

void reimprimirPrompt() {
    std::cout << "\r" << std::string(80, ' ') << "\r"; // Limpa a linha
    
    if (g_estadoEsperado == EstadoResposta::JOGADA_OU_TRUCO_OU_ENVIDO) {
        std::cout << "Digite [numero], [truco] ou [envido]: ";
    } else if (g_estadoEsperado == EstadoResposta::JOGADA_OU_TRUCO) {
        std::cout << "Digite [numero] ou [truco]: ";
    } else if (g_estadoEsperado == EstadoResposta::JOGADA_APENAS) {
        std::cout << "Digite [numero da carta]: ";
    } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_TRUCO) {
        std::cout << "Truco! Responda: [aceito], [corro], [retruco]: ";
    } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_RETRUCO) {
        std::cout << "Retruco! Responda: [aceito], [corro], [vale 4]: ";
    } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_VALE4) {
        std::cout << "Vale 4! Responda: [aceito], [corro]: ";
    } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_ENVIDO) {
        std::cout << "Envido! Responda: [aceito], [corro], [real envido]: ";
    } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_REAL_ENVIDO) {
        std::cout << "Real Envido! Responda: [aceito], [corro]: ";
    } else {
        std::cout << "> "; 
    }
    fflush(stdout);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

// --- Thread de Recebimento ---

void* receive_messages(void* arg) {
    int sock = *(int*)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;
    std::string msgBuffer = "";
    bool aposta_aceita_nesta_mao = false;

    while ((bytes_read = read(sock, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[bytes_read] = '\0';
        msgBuffer += buffer; 

        size_t pos;
        while ((pos = msgBuffer.find('\n')) != std::string::npos) {
            std::string mensagem = msgBuffer.substr(0, pos);
            msgBuffer.erase(0, pos + 1); 

            std::lock_guard<std::mutex> lock(g_mutex); 
            std::cout << "\r" << std::string(80, ' ') << "\r"; // Limpa a linha visualmente

            try {
                if (mensagem.rfind("HAND:", 0) == 0) {
                    aposta_aceita_nesta_mao = false; 
                    g_florOcorreu = false; // Reseta flag de flor
                    g_envidoOcorreu = false;
                    std::cout << "\n=== SUA MÃO ===" << std::endl;
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
                    if (g_estadoEsperado == EstadoResposta::NENHUMA) {
                        // Regra: 3 cartas (1ª rodada), sem aposta e SEM FLOR -> Pode Envido
                        if (g_minhaMao.size() == 3 && !aposta_aceita_nesta_mao && !g_florOcorreu && !g_envidoOcorreu) {
                            g_estadoEsperado = EstadoResposta::JOGADA_OU_TRUCO_OU_ENVIDO;
                        } else if (aposta_aceita_nesta_mao) {
                             g_estadoEsperado = EstadoResposta::JOGADA_APENAS;
                        } else {
                             g_estadoEsperado = EstadoResposta::JOGADA_OU_TRUCO;
                        }
                    }
                    std::cout << "\n--- SUA VEZ ---" << std::endl;

                } else if (mensagem.rfind("PEDIDO_ENVIDO", 0) == 0) {
                    g_minhaVez = true; g_estadoEsperado = EstadoResposta::RESPOSTA_ENVIDO;
                    g_envidoOcorreu = true;
                    std::cout << "\n!!! OPONENTE PEDIU ENVIDO !!!" << std::endl;
                } else if (mensagem.rfind("PEDIDO_REAL_ENVIDO", 0) == 0) {
                    g_minhaVez = true; g_estadoEsperado = EstadoResposta::RESPOSTA_REAL_ENVIDO;
                    g_envidoOcorreu = true;
                    std::cout << "\n!!! OPONENTE PEDIU REAL ENVIDO !!!" << std::endl;
                } else if (mensagem.rfind("PEDIDO_TRUCO", 0) == 0) {
                    g_minhaVez = true; g_estadoEsperado = EstadoResposta::RESPOSTA_TRUCO;
                    std::cout << "\n!!! OPONENTE PEDIU TRUCO !!!" << std::endl;
                } else if (mensagem.rfind("PEDIDO_RETRUCO", 0) == 0) {
                    g_minhaVez = true; g_estadoEsperado = EstadoResposta::RESPOSTA_RETRUCO;
                    std::cout << "\n!!! OPONENTE PEDIU RETRUCO !!!" << std::endl;
                } else if (mensagem.rfind("PEDIDO_VALE_QUATRO", 0) == 0) {
                    g_minhaVez = true; g_estadoEsperado = EstadoResposta::RESPOSTA_VALE4;
                    std::cout << "\n!!! OPONENTE PEDIU VALE QUATRO !!!" << std::endl;

                } else if (mensagem.rfind("VOCE_JOGOU:", 0) == 0) {
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
                    std::string data = mensagem.substr(15); 
                    size_t pos = data.find(',');
                    int val = std::stoi(data.substr(0, pos));
                    int nai = std::stoi(data.substr(pos + 1));
                    std::cout << "Oponente jogou: " << Tradutor::toString(Carta((Valor)val, (Naipe)nai)) << std::endl;

                } else if (mensagem.rfind("PLACAR:", 0) == 0) {
                    std::string data = mensagem.substr(7); std::stringstream ss_data(data);
                    std::string s; int meu, op;
                    std::getline(ss_data, s, ','); meu = std::stoi(s);
                    std::getline(ss_data, s, ','); op = std::stoi(s);
                    std::cout << "Placar da Mão: Você " << meu << " x " << op << " Oponente" << std::endl;
                
                } else if (mensagem.rfind("PLACAR_JOGO:", 0) == 0) {
                    std::string data = mensagem.substr(12); std::stringstream ss_data(data);
                    std::string s; int meu, op;
                    std::getline(ss_data, s, ','); meu = std::stoi(s);
                    std::getline(ss_data, s, ','); op = std::stoi(s);
                    std::cout << "\n=================================\n  PLACAR DO JOGO: Voce " << meu << " x " << op << " Oponente\n=================================\n" << std::endl;
                
                } else if (mensagem.rfind("FIM_MAO:", 0) == 0) {
                    g_minhaVez = false;
                    g_estadoEsperado = EstadoResposta::NENHUMA;
                    std::cout << "\n*** " << mensagem.substr(8) << " ***" << std::endl;
                    std::cout << "Aguardando próxima mão...\n" << std::endl;
                
                } else if (mensagem.rfind("FIM_JOGO:", 0) == 0) {
                    std::cout << "\n*********************************\n*** " << mensagem.substr(9) << " ***\n*********************************" << std::endl;
                    close(sock);
                    exit(0); 
                
                } else if (mensagem.rfind("INFO:Aposta aceite!", 0) == 0) { 
                    aposta_aceita_nesta_mao = true;
                    std::cout << "INFO: " << mensagem.substr(5) << std::endl;
                } else {
                    // Se servidor avisa de Flor, bloqueamos envido localmente
                    if (mensagem.find("Flor") != std::string::npos) g_florOcorreu = true;
                    if (mensagem.find("Envido") != std::string::npos||mensagem.find("correu") != std::string::npos) g_envidoOcorreu = true;
                    std::cout << "INFO: " << mensagem.substr(5) << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "[ERRO DE PARSING]: " << e.what() << " em " << mensagem << std::endl;
            }

            reimprimirPrompt(); 
        }
    }
    if (bytes_read == 0) std::cout << "\nServidor desconectou." << std::endl;
    close(sock); exit(0); return NULL;
}

int main() {
    int sock = 0; struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;
    memset(&serv_addr, 0, sizeof(serv_addr)); serv_addr.sin_family = AF_INET; serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) return -1;
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) return -1;
    std::cout << "Conectado ao servidor!" << std::endl;
    pthread_t recv_thread; if (pthread_create(&recv_thread, NULL, receive_messages, (void*)&sock) != 0) return -1;
    { std::lock_guard<std::mutex> lock(g_mutex); reimprimirPrompt(); }

    std::string line;
    while (std::getline(std::cin, line)) { 
        std::string comando = toLower(line); 
        std::lock_guard<std::mutex> lock(g_mutex); 
        if (!g_minhaVez) { std::cout << "\rAguarde sua vez..." << std::endl; reimprimirPrompt(); continue; }

        std::string msg_enviar = "";
        // --- LÓGICA DE ENVIO ---
        if (g_estadoEsperado == EstadoResposta::JOGADA_OU_TRUCO_OU_ENVIDO) {
            if (comando == "truco") msg_enviar = "TRUCO\n";
            else if (comando == "envido") msg_enviar = "ENVIDO\n";
            else try { int c=std::stoi(comando); if(c>=1 && c<=g_minhaMao.size()) msg_enviar="JOGAR:"+std::to_string(c-1)+"\n"; } catch(...){}
        } else if (g_estadoEsperado == EstadoResposta::JOGADA_OU_TRUCO) {
            if (comando == "truco") msg_enviar = "TRUCO\n";
            else try { int c=std::stoi(comando); if(c>=1 && c<=g_minhaMao.size()) msg_enviar="JOGAR:"+std::to_string(c-1)+"\n"; } catch(...){}
        } else if (g_estadoEsperado == EstadoResposta::JOGADA_APENAS) {
            try { int c=std::stoi(comando); if(c>=1 && c<=g_minhaMao.size()) msg_enviar="JOGAR:"+std::to_string(c-1)+"\n"; } catch(...){}
        } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_TRUCO) {
            if (comando == "aceito") msg_enviar = "ACEITO\n"; else if (comando == "corro") msg_enviar = "CORRO\n"; else if (comando == "retruco") msg_enviar = "RETRUCO\n";
        } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_RETRUCO) {
            if (comando == "aceito") msg_enviar = "ACEITO\n"; else if (comando == "corro") msg_enviar = "CORRO\n"; else if (comando == "vale 4" || comando == "valequatro") msg_enviar = "VALE_QUATRO\n";
        } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_VALE4) {
            if (comando == "aceito") msg_enviar = "ACEITO\n"; else if (comando == "corro") msg_enviar = "CORRO\n";
        } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_ENVIDO) {
            if (comando == "aceito") msg_enviar = "ACEITO\n"; else if (comando == "corro") msg_enviar = "CORRO\n"; else if (comando == "real envido") msg_enviar = "REAL ENVIDO\n";
        } else if (g_estadoEsperado == EstadoResposta::RESPOSTA_REAL_ENVIDO) {
            if (comando == "aceito") msg_enviar = "ACEITO\n"; else if (comando == "corro") msg_enviar = "CORRO\n";
        }

        if (!msg_enviar.empty()) {
            send(sock, msg_enviar.c_str(), msg_enviar.length(), 0);
            g_minhaVez = false; g_estadoEsperado = EstadoResposta::NENHUMA; 
        } else { std::cout << "\rComando inválido." << std::endl; reimprimirPrompt(); }
    }
    close(sock); return 0;
}
