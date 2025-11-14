#pragma once

#include "cartas.h" // Inclui a nossa definição de Carta, Naipe e Valor
#include <vector>     // Para usar o std::vector (nosso "monte" de cartas)
#include <algorithm>  // Para usar std::shuffle (embaralhar)
#include <random>     // Para o std::shuffle
#include <chrono>     // Para uma "semente" (seed) de aleatoriedade melhor

class Baralho {
private:
    std::vector<Carta> cartas;

    // Função interna para (re)criar o baralho
    void inicializar() {
        // Limpa o vetor caso esteja a ser resetado
        cartas.clear();

        // Itera sobre todos os naipes
        for (int n = 0; n <= (int)Naipe::Paus; ++n) {
            
            // Itera sobre todos os valores
            for (int v = 0; v <= (int)Valor::Tres; ++v) {
                
                // Converte os inteiros de volta para os enums
                Naipe naipeAtual = (Naipe)n;
                Valor valorAtual = (Valor)v;

                // Adiciona a carta ao vetor
                cartas.push_back(Carta(valorAtual, naipeAtual));
            }
        }
    }

public:
    // Construtor: Chamado quando o Baralho é criado
    Baralho() {
        inicializar();
    }

    // Embaralha o vetor de cartas
    void embaralhar() {
        // 1. Cria uma "semente" aleatória baseada no relógio atual
        //    Isso garante que cada embaralhada seja diferente
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        
        // 2. Cria um gerador de números aleatórios
        std::default_random_engine rng(seed);

        // 3. Usa a função std::shuffle para misturar o vetor 'cartas'
        std::shuffle(cartas.begin(), cartas.end(), rng);
    }

    // Distribui um número de cartas (ex: 3 para o Truco)
    // Retorna um vetor de Cartas (a "mão" do jogador)
    std::vector<Carta> distribuir(int quantidade) {
        std::vector<Carta> mao;

        // Tira cartas do "topo" do baralho (o fim do vetor)
        for (int i = 0; i < quantidade; ++i) {
            if (cartas.empty()) {
                // Isso não deve acontecer num jogo normal de truco,
                // mas é bom ter uma verificação
                break; 
            }
            // Pega a última carta do vetor
            mao.push_back(cartas.back());
            // Remove a última carta do vetor
            cartas.pop_back();
        }
        return mao;
    }

    // Retorna o baralho ao estado original (40 cartas)
    // Útil para começar uma nova "mão"
    void resetar() {
        inicializar();
    }
};
