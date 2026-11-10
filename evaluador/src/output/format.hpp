// format.hpp - Formatos de salida de la guía (§3.1.3 y §3.2.6).
#pragma once

#include "shared/model.hpp"

#include <sstream>
#include <string>

namespace evaluator::format {

// list processing:  [id i k q p]
inline std::string processing(const Sample& s) {
    std::ostringstream out;
    out << '[' << s.id << ' ' << s.queue << ' ' << s.type << ' ' << s.qty << ' ' << s.procTime << ']';
    return out.str();
}

// list waiting:  [id i k q]
inline std::string waiting(const Sample& s) {
    std::ostringstream out;
    out << '[' << s.id << ' ' << s.queue << ' ' << s.type << ' ' << s.qty << ']';
    return out.str();
}

// list reported:  [id i k r]  con r en {positive, negative, -}  (§3.1.3)
inline std::string reported(const Sample& s) {
    const char* r = s.result == 'P' ? "positive" : (s.result == 'N' ? "negative" : "-");
    std::ostringstream out;
    out << '[' << s.id << ' ' << s.queue << ' ' << s.type << ' ' << r << ']';
    return out.str();
}

// rep:  [id i k r]  con r en {P, N, ?}  (§3.2.6)
inline std::string report(const Sample& s) {
    std::ostringstream out;
    out << '[' << s.id << ' ' << s.queue << ' ' << s.type << ' ' << (s.result ? s.result : '?') << ']';
    return out.str();
}

} // namespace evaluator::format
