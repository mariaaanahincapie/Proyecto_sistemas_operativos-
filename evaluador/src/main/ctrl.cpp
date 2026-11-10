// ctrl.cpp - Programa real de `evaluator ctrl` (bin/evaluator-ctrl).
//
//   evaluator ctrl [-s <string>]
//
// Modo interactivo hasta fin de archivo (Ctrl+D en Linux, Ctrl+Z+Enter en Windows):
//   list [processing | waiting | reported | reactive | all]
//   update {B|D|S} <entero positivo>
#include "output/format.hpp"
#include "queues/ring.hpp"
#include "shared/cli.hpp"
#include "shared/platform.hpp"
#include "shared/shm.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace evaluator;

namespace {

// Processing: [id i k q p] — lo que cada analizador procesa ahora.
void listProcessing(SharedRegion& region) {
    auto* h = region.hdr();
    std::vector<Sample> items;
    {
        SemLock board(region.sem(), h->mutexBoard);
        for (int t = 0; t < kTypeCount; ++t) if (h->busy[t]) items.push_back(h->processing[t]);
    }
    std::cout << "Processing:\n";
    for (const auto& s : items) std::cout << format::processing(s) << '\n';
}

// Waiting: [id i k q] — colas de entrada + "en mano" de los hilos de entrada + colas internas.
// Se toman TODOS los candados de entrada (en orden 0..i-1) y luego el del tablero:
// así la foto es consistente y una muestra no aparece dos veces ni desaparece.
void listWaiting(SharedRegion& region) {
    auto* h = region.hdr();
    auto& sem = region.sem();
    std::vector<Sample> items;
    for (int k = 0; k < h->cfg.inputs; ++k) sem.wait(region.inRing(k).mutex);
    sem.wait(h->mutexBoard);
    for (int k = 0; k < h->cfg.inputs; ++k)
        for (const auto& s : ringContents(region.inRing(k), region.inSlots(k))) items.push_back(s);
    for (int k = 0; k < h->cfg.inputs; ++k) if (region.holding(k).busy) items.push_back(region.holding(k).sample);
    for (int t = 0; t < kTypeCount; ++t)
        for (int i = 0; i < h->internalCount[t]; ++i) items.push_back(region.internalMirror(t)[i]);
    sem.post(h->mutexBoard);
    for (int k = h->cfg.inputs - 1; k >= 0; --k) sem.post(region.inRing(k).mutex);

    std::sort(items.begin(), items.end(), [](const Sample& a, const Sample& b) { return a.id < b.id; });
    std::cout << "Waiting:\n";
    for (const auto& s : items) std::cout << format::waiting(s) << '\n';
}

// Reported: [id i k r] — resultados en la cola de salida que aún no tomó ningún rep (no se retiran).
void listReported(SharedRegion& region) {
    std::vector<Sample> items;
    {
        SemLock lock(region.sem(), region.outRing().mutex);
        items = ringContents(region.outRing(), region.outSlots());
    }
    std::cout << "Reported:\n";
    for (const auto& s : items) std::cout << format::reported(s) << '\n';
}

// Reactive: nivel de cada reactivo y estado del robot.
void listReactive(SharedRegion& region) {
    auto* h = region.hdr();
    int level[kTypeCount], pending[kTypeCount], robotState, robotType, robotLoad;
    {
        SemLock lock(region.sem(), h->mutexReactive);
        for (int t = 0; t < kTypeCount; ++t) { level[t] = h->reactive[t]; pending[t] = h->reqPending[t]; }
        robotState = h->robotState; robotType = h->robotType; robotLoad = h->robotLoad;
    }
    std::cout << "Reactive:\n";
    for (int t = 0; t < kTypeCount; ++t)
        std::cout << typeCode(t) << ' ' << level[t] << (pending[t] ? "  (recarga solicitada)" : "") << '\n';
    static const char* states[] = {"libre", "yendo al almacén", "regresando", "cargando"};
    std::cout << "Robot: " << states[std::clamp(robotState, 0, 3)];
    if (robotType >= 0) std::cout << " (" << typeCode(robotType) << ", " << robotLoad << " unidades)";
    std::cout << '\n';
}

void update(SharedRegion& region, int t, int amount) {
    auto* h = region.hdr();
    int before, after;
    {
        SemLock lock(region.sem(), h->mutexReactive);
        before = h->reactive[t];
        h->reactive[t] = std::min(kMaxReactive, before + amount);   // tope: valor máximo 100
        after = h->reactive[t];
    }
    region.sem().post(h->recharged[t]);   // si el analizador esperaba reactivo, que vuelva a revisar
    std::cout << "Reactive " << typeCode(t) << ": " << before << " -> " << after;
    if (before + amount > kMaxReactive) std::cout << " (limitado al máximo " << kMaxReactive << ")";
    std::cout << '\n';
}

} // namespace

int main(int argc, char** argv) {
    std::string name;
    for (int i = 1; i < argc; ++i) {
        const std::string opt = argv[i];
        if (opt == "-s" && i + 1 < argc) name = argv[++i];
        else { std::cerr << "uso: evaluator ctrl [-s <string>]\n"; return kExitBadArgs; }
    }
    name = cli::resolveName(name);

    SharedRegion region;
    std::string error;
    if (!SharedRegion::validName(name) || !region.open(name, error)) {
        std::cerr << "evaluator ctrl: " << (error.empty() ? "nombre inválido" : error) << '\n';
        return kExitNotRunning;
    }

    const bool prompt = platform::stdinIsTerminal();
    std::string line;
    for (;;) {
        if (prompt) std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;   // fin de archivo: termina ctrl
        if (!region.systemAlive()) { std::cerr << "error: el sistema \"" << name << "\" se detuvo\n"; return kExitNotRunning; }
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::istringstream in(line);
        std::string command, arg, value, extra;
        in >> command;
        if (command.empty()) continue;
        if (command == "list") {
            in >> arg;
            if (in >> extra) { std::cerr << "error: uso list [processing|waiting|reported|reactive|all]\n"; continue; }
            if (arg == "processing") listProcessing(region);
            else if (arg == "waiting") listWaiting(region);
            else if (arg == "reported") listReported(region);
            else if (arg == "reactive") listReactive(region);
            else if (arg == "all" || arg.empty()) {
                listProcessing(region); listWaiting(region); listReported(region); listReactive(region);
            } else std::cerr << "error: uso list [processing|waiting|reported|reactive|all]\n";
        } else if (command == "update") {
            int amount = 0;
            if (!(in >> arg >> value) || (in >> extra) || arg.size() != 1 || typeIndex(arg[0]) < 0 ||
                !cli::parseInt(value, 1, 1000000, amount)) {
                std::cerr << "error: valor inválido (uso: update {B|D|S} <entero positivo>)\n";
                continue;
            }
            update(region, typeIndex(arg[0]), amount);
        } else {
            std::cerr << "error: comando desconocido '" << command << "' (use list o update)\n";
        }
        std::cout << std::flush;
    }
    if (prompt) std::cout << '\n';
    return kExitOk;
}
