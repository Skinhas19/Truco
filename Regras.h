#pragma once

#include "cartas.h" 

int getForca(const Carta& c) {
    
    // --- 1. Checagem das Manilhas (casos especiais) ---

    // Manilha 1: 1 de Espadas
    if (c.valor == Valor::Um && c.naipe == Naipe::Espadas) {
        return 13;
    }
    // Manilha 2: 1 de Paus
    if (c.valor == Valor::Um && c.naipe == Naipe::Paus) {
        return 12;
    }
    // Manilha 3: 7 de Espadas
    if (c.valor == Valor::Sete && c.naipe == Naipe::Espadas) {
        return 11;
    }
    // Manilha 4: 7 de Ouros
    if (c.valor == Valor::Sete && c.naipe == Naipe::Ouros) {
        return 10;
    }

    // --- 2. Checagem das Cartas Comuns ---

    switch (c.valor) {
        case Valor::Tres:
            return 9;
        case Valor::Dois:
            return 8;
        case Valor::Um:
            return 7;
        case Valor::Doze:
            return 6;
        case Valor::Onze:
            return 5;
        case Valor::Dez:
            return 4;
        case Valor::Sete:
            return 3; 
        case Valor::Seis:
            return 2;
        case Valor::Cinco:
            return 1;
        case Valor::Quatro:
            return 0;
    }
    return -1; 
}

/*Compara duas cartas para ver qual é mais forte.
return 1 se carta1 > carta2
return -1 se carta1 < carta2
return 0 se empatarem */

int compararCartas(const Carta& carta1, const Carta& carta2) {
    int forca1 = getForca(carta1);
    int forca2 = getForca(carta2);

    if (forca1 > forca2) {
        return 1;
    }
    if (forca1 < forca2) {
        return -1;
    }
    return 0; // Empate
}
// ... (Código anterior de regras.h)

// Retorna o valor "facial" da carta para o Envido
// (Ex: 3 vale 3, 7 vale 7, Valete/Dama/Rei valem 0, Ás vale 1)
int getValorEnvido(const Carta& c) {
    switch (c.valor) {
        case Valor::Quatro: return 4;
        case Valor::Cinco: return 5;
        case Valor::Seis: return 6;
        case Valor::Sete: return 7;
        case Valor::Dez: return 0; // Figuras valem 0
        case Valor::Onze: return 0;
        case Valor::Doze: return 0;
        case Valor::Um: return 1;
        case Valor::Dois: return 2;
        case Valor::Tres: return 3;
    }
    return 0;
}

// Calcula os pontos de Envido de uma mão (3 cartas)
int calcularEnvido(const std::vector<Carta>& mao) {
    int maxPontos = 0;

    // 1. Testa pares de cartas com mesmo naipe
    for (size_t i = 0; i < mao.size(); i++) {
        for (size_t j = i + 1; j < mao.size(); j++) {
            if (mao[i].naipe == mao[j].naipe) {
                // Regra: 20 + carta1 + carta2
                int pontos = 20 + getValorEnvido(mao[i]) + getValorEnvido(mao[j]);
                if (pontos > maxPontos) maxPontos = pontos;
            }
        }
    }

    // 2. Se não tiver cartas do mesmo naipe, vale a carta mais alta sozinha
    if (maxPontos == 0) {
        for (const auto& c : mao) {
            int v = getValorEnvido(c);
            if (v > maxPontos) maxPontos = v;
        }
    }

    return maxPontos;
}
