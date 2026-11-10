// init.cpp - Programa real de `evaluator init` (bin/evaluator-init).
//
//   evaluator init [-i <int>] [-ie <int>] [-oe <int>] [-n <string>] [-b <int>]
//                  [-d <int>] [-ee <int>] [-s <int>] [-q <int>]
//
// Crea la memoria compartida (si no existe ya un sistema vivo con ese nombre),
// arranca los hilos del evaluador y se queda ocupando la consola hasta Ctrl+C.
#include "evaluator/evaluator.hpp"
#include "shared/cli.hpp"
#include "shared/platform.hpp"
#include "shared/shm.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace evaluator;

namespace {

void usage() {
    std::cerr << "uso: evaluator init [-i <int>] [-ie <int>] [-oe <int>] [-n <string>] [-b <int>] [-d <int>]\n"
                 "                    [-ee <int>] [-s <int>] [-q <int>]\n"
                 "  -i  colas de entrada (5)          -ie posiciones por cola de entrada (6)\n"
                 "  -oe posiciones de la salida (10)  -n  nombre de la memoria compartida (evaluator)\n"
                 "  -b  reactivo de sangre 0..100 (100)  -d reactivo de detritos (100)  -s reactivo de piel (100)\n"
                 "  -q  tamaño de las colas internas (6)  -ee capacidad del robot 1..100 (50)\n";
}

bool parseArgs(int argc, char** argv, Config& cfg, std::string& name) {
    for (int i = 1; i < argc; ++i) {
        const std::string opt = argv[i];
        if (i + 1 >= argc) { std::cerr << "evaluator init: falta el valor de " << opt << '\n'; return false; }
        const std::string value = argv[++i];
        if (opt == "-n") { name = value; continue; }

        int* target = nullptr;
        int min = 1, max = kMaxCapacity;
        if      (opt == "-i")  { target = &cfg.inputs;      max = kMaxInputs; }
        else if (opt == "-ie") { target = &cfg.inputCap; }
        else if (opt == "-oe") { target = &cfg.outputCap; }
        else if (opt == "-q")  { target = &cfg.internalCap; }
        else if (opt == "-b")  { target = &cfg.reactive[0]; min = 0; max = kMaxReactive; }
        else if (opt == "-d")  { target = &cfg.reactive[1]; min = 0; max = kMaxReactive; }
        else if (opt == "-s")  { target = &cfg.reactive[2]; min = 0; max = kMaxReactive; }
        else if (opt == "-ee") { target = &cfg.robotCap;    max = kMaxReactive; }
        else { std::cerr << "evaluator init: opción desconocida " << opt << '\n'; return false; }

        if (!cli::parseInt(value, min, max, *target)) {
            std::cerr << "evaluator init: valor inválido para " << opt << " (" << value << "); debe ser un entero en "
                      << min << ".." << max << '\n';
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Config cfg;
    std::string name;
    if (!parseArgs(argc, argv, cfg, name)) { usage(); return kExitBadArgs; }
    name = cli::resolveName(name);
    if (!SharedRegion::validName(name)) {
        std::cerr << "evaluator init: nombre inválido \"" << name << "\" (use letras, dígitos, _ o -; máx. 40)\n";
        return kExitBadArgs;
    }
    cfg.maxWait = platform::envPositiveInt("EVALUATOR_MAX_WAIT", kDefaultMaxWait);

    SharedRegion region;
    bool alreadyRunning = false;
    std::string error;
    if (!region.create(name, cfg, alreadyRunning, error)) {
        std::cerr << "evaluator init: " << error << '\n';
        return alreadyRunning ? kExitBadArgs : kExitInternal;
    }

    std::atomic<bool> stopRequested{false};
    platform::installStopHandler(&stopRequested);

    std::cout << "Sistema \"" << name << "\" listo (pid " << platform::currentPid() << ", "
              << region.hdr()->totalSize << " bytes de memoria compartida)\n"
              << "  entradas=" << cfg.inputs << " x " << cfg.inputCap << "   salida=" << cfg.outputCap
              << "   colas internas=" << cfg.internalCap << "   reactivo B/D/S=" << cfg.reactive[0] << '/'
              << cfg.reactive[1] << '/' << cfg.reactive[2] << "   robot=" << cfg.robotCap
              << "   espera máx=" << cfg.maxWait << " s";
    if (platform::speedFactor() > 1) std::cout << "   velocidad x" << platform::speedFactor();
    std::cout << "\n  Hilos: " << cfg.inputs << " de entrada, 3 analizadores, 1 robot, 1 vigilante."
              << " Ctrl+C para detener.\n" << std::flush;

    Evaluator evaluator(region);
    evaluator.start();

    // El hilo principal solo espera la señal de apagado (ocupa la consola, §3.2.4).
    while (!stopRequested.load()) std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Apagado ordenado: avisar a los demás procesos, detener hilos, borrar el segmento.
    region.hdr()->shuttingDown.store(1);
    evaluator.log("init", "apagando...");
    evaluator.stop();
    evaluator.join();
    const auto* h = region.hdr();
    std::cout << "Resumen: registradas=" << h->statRegistered << " procesadas=" << h->statProcessed
              << " vencidas=" << h->statExpired << '\n';
    region.unlink();
    region.close();
    std::cout << "Sistema \"" << name << "\" detenido; memoria compartida liberada." << std::endl;
    return kExitOk;
}
