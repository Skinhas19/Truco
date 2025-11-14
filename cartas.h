#pragma once // Evita que o arquivo seja incluído múltiplas vezes

#include <string>
#include <map>

// --- 1. Definição dos Naipes ---
// Usamos 'enum class' para um escopo mais seguro
enum class Naipe {
    Ouros,
    Espadas,
    Copas,
    Paus
};

// --- 2. Definição dos Valores (Faces) ---
// Ordenados pela força base do Truco (antes das Manilhas)
// 4 (fraco) -> 3 (forte)
enum class Valor {
    Quatro,
    Cinco,
    Seis,
    Sete,   // Os 7 "comuns"
    Dez,   // Q (Rainha)
    Onze, // J (Guerreiro)
    Doze,    // K
    Um,     // A
    Dois,
    Tres
    // As Manilhas (Zap, 7 Copas, Espadilha, 7 Ouros) terão
    // um ranking especial, mas ainda são definidas por um Naipe e um Valor.
};

// --- 3. A Estrutura da Carta ---
struct Carta {
    Valor valor;
    Naipe naipe;

    // Construtor padrão (opcional, mas bom para inicializar)
    Carta(Valor v = Valor::Quatro, Naipe n = Naipe::Ouros) : valor(v), naipe(n) {}
};

// --- 4. Funções Auxiliares (para imprimir as cartas) ---

// Mapas para converter enums em texto
// (Colocamos em um 'namespace' para organizar)
namespace Tradutor {

    // std::map é como um dicionário
    static std::map<Naipe, std::string> nomesNaipe = {
        {Naipe::Ouros, "Ouros"},
        {Naipe::Espadas, "Espadas"},
        {Naipe::Copas, "Copas"},
        {Naipe::Paus, "Paus"}
    };

    static std::map<Valor, std::string> nomesValor = {
        {Valor::Quatro, "4"},
        {Valor::Cinco, "5"},
        {Valor::Seis, "6"},
        {Valor::Sete, "7"},
        {Valor::Dez, "10"},
        {Valor::Onze, "11"},
        {Valor::Doze, "12"},
        {Valor::Um, "1"},
        {Valor::Dois, "2"},
        {Valor::Tres, "3"}
    };

    // Função que retorna o nome completo da carta
    static std::string toString(const Carta& c) {
        // Exemplo: "7" + " de " + "Espadas"
        return nomesValor[c.valor] + " de " + nomesNaipe[c.naipe];
    }
} // fim do namespace Tradutor
