#pragma once

#include "cartas.h" // Precisamos saber o que é uma Carta

/**
 * @brief Retorna um valor numérico para a "força" de uma carta.
 * Quanto maior o número, mais forte é a carta, seguindo
 * a hierarquia específica do seu jogo.
 *
 * Hierarquia Manilhas (mais fortes):
 * 1. Ás de Espadas (Força 13)
 * 2. Ás de Paus (Força 12)
 * 3. 7 de Espadas (Força 11)
 * 4. 7 de Ouros (Força 10)
 *
 * Hierarquia Comum (em ordem de força):
 * 5. 3s (qualquer naipe) (Força 9)
 * 6. 2s (qualquer naipe) (Força 8)
 * 7. Ás (Ouros, Copas) (Força 7)
 * 8. Reis (qualquer naipe) (Força 6)
 * 9. Valetes (qualquer naipe) (Força 5)
 * 10. Damas (qualquer naipe) (Força 4)
 * 11. 7s (Paus, Copas) (Força 3)
 * 12. 6s (qualquer naipe) (Força 2)
 * 13. 5s (qualquer naipe) (Força 1)
 * 14. 4s (qualquer naipe) (Força 0)
 */
int getForca(const Carta& c) {
    
    // --- 1. Checagem das Manilhas (casos especiais) ---
    // (A hierarquia exata que você pediu)

    // Manilha 1: Ás de Espadas
    if (c.valor == Valor::Um && c.naipe == Naipe::Espadas) {
        return 13;
    }
    // Manilha 2: Ás de Paus
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
    // (A ordem padrão do Truco que você mencionou)

    switch (c.valor) {
        case Valor::Tres:
            return 9;
        case Valor::Dois:
            return 8;
        case Valor::Um:
            // Os Ás das manilhas (Espadas, Paus) já foram tratados.
            // Sobram Ouros e Copas.
            return 7;
        case Valor::Doze:
            return 6;
        case Valor::Onze:
            return 5;
        case Valor::Dez:
            return 4;
        case Valor::Sete:
            // Os 7s das manilhas (Espadas, Ouros) já foram tratados.
            // Sobram Paus e Copas.
            return 3; 
        case Valor::Seis:
            return 2;
        case Valor::Cinco:
            return 1;
        case Valor::Quatro:
            return 0;
    }

    // (Nunca deve chegar aqui)
    return -1; 
}

/**
 * @brief Compara duas cartas para ver qual é mais forte.
 * * @return 1 se carta1 > carta2
 * @return -1 se carta1 < carta2
 * @return 0 se empatarem (ex: dois Reis, ou duas cartas de força 9)
 */
int compararCartas(const Carta& carta1, const Carta& carta2) {
    int forca1 = getForca(carta1);
    int forca2 = getForca(carta2);

    if (forca1 > forca2) {
        return 1;
    }
    if (forca1 < forca2) {
        return -1;
    }
    
    // As cartas têm a mesma força (ex: dois 3s, ou um 3 e um Ás de Ouros)
    // Espera, 3 é 9 e Ás de Ouros é 7. Minha lógica está correta.
    // O empate só ocorre se getForca() retornar o MESMO número.
    return 0; // Empate
}
