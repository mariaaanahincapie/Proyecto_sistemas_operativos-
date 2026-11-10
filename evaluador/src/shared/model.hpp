// model.hpp - Tipos y constantes comunes a todos los programas del evaluador.
//
// Todo lo que se guarda en la memoria compartida debe ser de tipos simples
// (enteros, char, arreglos fijos): nada de std::string ni std::vector, porque
// cada proceso mapea la región en una dirección distinta y los punteros no
// tendrían sentido en otro proceso.
#pragma once

#include <cstdint>

namespace evaluator {

// ---------------------------------------------------------------------------
// Tipos de muestra: 0 = B (sangre), 1 = D (detritos), 2 = S (piel)
// ---------------------------------------------------------------------------
constexpr int kTypeCount = 3;

inline int typeIndex(char type) {
    switch (type) {
        case 'B': return 0;
        case 'D': return 1;
        case 'S': return 2;
        default:  return -1;
    }
}

inline char typeCode(int index) {
    static constexpr char codes[] = {'B', 'D', 'S'};
    return (index >= 0 && index < kTypeCount) ? codes[index] : '?';
}

inline const char* typeName(int index) {
    static constexpr const char* names[] = {"sangre", "detritos", "piel"};
    return (index >= 0 && index < kTypeCount) ? names[index] : "?";
}

// Tiempo de procesamiento (segundos) por tipo, guía §3.2.7.
constexpr int kProcMin[kTypeCount] = {1, 5, 8};
constexpr int kProcMax[kTypeCount] = {7, 20, 25};

// ---------------------------------------------------------------------------
// Valores por omisión (guía §3.1.1) y supuestos del diseño (doc/06).
// ---------------------------------------------------------------------------
constexpr int kDefaultInputs        = 5;    // -i
constexpr int kDefaultInputCap      = 6;    // -ie
constexpr int kDefaultOutputCap     = 10;   // -oe
constexpr int kDefaultInternalCap   = 6;    // -q
constexpr int kDefaultReactive      = 100;  // -b -d -s
constexpr int kMaxReactive          = 100;  // "valor máximo 100"
constexpr int kDefaultRobotCap      = 50;   // -ee (supuesto D-01)
constexpr int kDefaultMaxWait       = 120;  // EVALUATOR_MAX_WAIT (supuesto D-10)
constexpr int kRobotTripSeconds     = 3;    // ida al almacén y vuelta: 3 s cada una (D-04)
constexpr int kRobotLoadSeconds     = 1;    // tiempo de carga en el analizador
constexpr int kWatchdogPeriod       = 5;    // el vigilante revisa cada 5 s
constexpr int kMinQty = 1, kMaxQty  = 5;    // cantidad de muestra 1..5

// Límites de validación (evitan regiones absurdamente grandes).
constexpr int kMaxInputs   = 64;
constexpr int kMaxCapacity = 1024;

constexpr const char* kDefaultName = "evaluator";

// Códigos de salida comunes (doc/02 §3).
constexpr int kExitOk         = 0;
constexpr int kExitBadArgs    = 1;
constexpr int kExitNotRunning = 2;
constexpr int kExitInternal   = 3;

// ---------------------------------------------------------------------------
// Muestra. Viaja por todas las colas; al final lleva el resultado.
// ---------------------------------------------------------------------------
struct Sample {
    std::uint64_t id;          // identificador único (>= 1)
    std::int64_t  enqueuedAt;  // instante en que reg la aceptó (epoch, s)
    std::int32_t  queue;       // cola de entrada de origen (0..i-1)
    std::int32_t  qty;         // cantidad de muestra (1..5) = reactivo que gasta
    std::int32_t  procTime;    // segundos de procesamiento (0 si no se procesó)
    char          type;        // 'B', 'D', 'S'
    char          result;      // 'P', 'N', '?' (0 mientras no tiene resultado)
    char          pad[2];
};

} // namespace evaluator
