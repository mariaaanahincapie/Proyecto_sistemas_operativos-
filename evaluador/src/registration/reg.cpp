// reg.cpp - Programa real de `evaluator reg` (bin/evaluator-reg).
//
//   evaluator reg [-n <string>] { <archivo> ... | - }
//
// Cada línea: <cola> {B|D|S} <cantidad 1..5>. Pueden correr varios reg a la vez.
//  - Interactivo ("-"): imprime el id de cada muestra aceptada.
//  - Archivo "x.txt": escribe los ids aceptados en "x.spl".
//  - Archivo inexistente: se ignora sin mensaje (guía §3.1.2).
//  - Cola llena: espera a que se libere una posición (guía §3.2.2).
#include "queues/ring.hpp"
#include "shared/cli.hpp"
#include "shared/platform.hpp"
#include "shared/shm.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace evaluator;

namespace {

enum class LineResult { Accepted, Skipped, Invalid, Stopped };

// Valida el formato "<integer> {B|D|S} <integer>" y los rangos.
bool parseLine(const std::string& line, int inputs, Sample& s, std::string& why) {
    std::istringstream in(line);
    std::string queueText, typeText, qtyText, extra;
    if (!(in >> queueText >> typeText >> qtyText) || (in >> extra)) {
        why = "se esperaba '<cola> <B|D|S> <cantidad>'"; return false;
    }
    int queue = 0, qty = 0;
    if (!cli::parseInt(queueText, 0, inputs - 1, queue)) {
        why = "cola inválida '" + queueText + "' (válidas 0.." + std::to_string(inputs - 1) + ")"; return false;
    }
    if (typeText.size() != 1 || typeIndex(typeText[0]) < 0) { why = "tipo inválido '" + typeText + "' (B, D o S)"; return false; }
    if (!cli::parseInt(qtyText, kMinQty, kMaxQty, qty)) { why = "cantidad inválida '" + qtyText + "' (1..5)"; return false; }
    s = Sample{};
    s.queue = queue;
    s.type = typeText[0];
    s.qty = qty;
    return true;
}

LineResult registerLine(SharedRegion& region, std::string line, const std::string& source, int lineNo, std::uint64_t& id) {
    if (!line.empty() && line.back() == '\r') line.pop_back();   // archivos con fin de línea de Windows
    const auto first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line[first] == '#') return LineResult::Skipped;

    auto* h = region.hdr();
    Sample s{};
    std::string why;
    if (!parseLine(line, h->cfg.inputs, s, why)) {
        std::cerr << "evaluator reg: " << source << ':' << lineNo << ": " << why << " -> línea ignorada\n";
        return LineResult::Invalid;
    }
    if (!region.systemAlive()) return LineResult::Stopped;

    // Productor: si la cola está llena, ringPush espera (semáforo empty).
    // El id se asigna DENTRO de la sección crítica de la cola (orden: cola -> mutexId).
    const bool ok = ringPush(region, region.inRing(s.queue), region.inSlots(s.queue), s,
        [&region] { return !region.systemAlive(); },
        [&](Sample& inserted) {
            {
                SemLock idLock(region.sem(), h->mutexId);
                inserted.id = h->nextId++;
                ++h->statRegistered;
            }
            inserted.enqueuedAt = platform::nowSeconds();
            id = inserted.id;
        });
    return ok ? LineResult::Accepted : LineResult::Stopped;
}

} // namespace

int main(int argc, char** argv) {
    std::string name;
    std::vector<std::string> sources;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-n") {
            if (i + 1 >= argc) { std::cerr << "evaluator reg: falta el valor de -n\n"; return kExitBadArgs; }
            name = argv[++i];
        } else {
            sources.push_back(arg);
        }
    }
    if (sources.empty()) {
        std::cerr << "uso: evaluator reg [-n <string>] { <archivo> ... | - }\n";
        return kExitBadArgs;
    }
    name = cli::resolveName(name);

    SharedRegion region;
    std::string error;
    if (!SharedRegion::validName(name) || !region.open(name, error)) {
        std::cerr << "evaluator reg: " << (error.empty() ? "nombre inválido" : error) << '\n';
        return kExitNotRunning;
    }
    if (!region.systemAlive()) { std::cerr << "evaluator reg: el sistema \"" << name << "\" no está activo\n"; return kExitNotRunning; }

    bool anyInvalid = false;
    for (const auto& source : sources) {
        std::istream* input = &std::cin;
        std::ifstream file;
        std::ofstream spl;
        const bool interactive = source == "-";
        if (!interactive) {
            file.open(source);
            if (!file) continue;   // archivo inexistente: se ignora en silencio
            std::filesystem::path out(source);
            out.replace_extension(".spl");
            if (out == std::filesystem::path(source)) out += ".spl";
            spl.open(out, std::ios::trunc);
            if (!spl) { std::cerr << "evaluator reg: no se pudo crear " << out.string() << '\n'; continue; }
            input = &file;
        }
        std::string line;
        int lineNo = 0;
        while (std::getline(*input, line)) {
            std::uint64_t id = 0;
            switch (registerLine(region, line, interactive ? "stdin" : source, ++lineNo, id)) {
                case LineResult::Accepted:
                    if (interactive) std::cout << id << std::endl;
                    else spl << id << std::endl;   // se vacía en cada id: se ve el avance
                    break;
                case LineResult::Invalid: anyInvalid = true; break;
                case LineResult::Skipped: break;
                case LineResult::Stopped:
                    std::cerr << "evaluator reg: el evaluador se detuvo; registro interrumpido\n";
                    return kExitNotRunning;
            }
        }
    }
    return anyInvalid ? kExitBadArgs : kExitOk;
}
