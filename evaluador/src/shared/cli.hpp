// cli.hpp - Utilidades de línea de comandos comunes a los programas.
#pragma once

#include "shared/model.hpp"

#include <cstdlib>
#include <string>

namespace evaluator::cli {

// Convierte `text` a entero en [min, max]. false si no es un entero válido o está fuera de rango.
inline bool parseInt(const std::string& text, int min, int max, int& out) {
    if (text.empty()) return false;
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (*end != '\0' || value < min || value > max) return false;
    out = static_cast<int>(value);
    return true;
}

// Nombre del segmento: el de la opción (-n / -s) si se dio; si no, EVALUATOR_SHM; si no, "evaluator".
inline std::string resolveName(const std::string& fromOption) {
    if (!fromOption.empty()) return fromOption;
    if (const char* env = std::getenv("EVALUATOR_SHM"); env && *env) return env;
    return kDefaultName;
}

} // namespace evaluator::cli
