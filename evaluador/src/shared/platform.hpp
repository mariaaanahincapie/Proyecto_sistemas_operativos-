// platform.hpp - Funciones que dependen del sistema operativo (Linux / Windows).
// El resto del código no incluye <windows.h> ni <unistd.h> directamente.
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace evaluator::platform {

std::int64_t currentPid();
bool processAlive(std::int64_t pid);      // ¿sigue vivo el proceso pid?
std::int64_t nowSeconds();                // reloj de pared (epoch, s)
bool stdinIsTerminal();                   // para mostrar el prompt "> " solo si hay usuario
std::string executableDir();              // carpeta del ejecutable actual

// Instala el manejador de Ctrl+C / SIGTERM: solo pone `flag` en true.
void installStopHandler(std::atomic<bool>* flag);

// Lee una variable de ambiente entera positiva; devuelve `fallback` si no existe o es inválida.
int envPositiveInt(const char* name, int fallback);

// Factor de aceleración de la simulación (EVALUATOR_SPEED, por omisión 1).
// Solo divide los tiempos reales de espera; los tiempos reportados no cambian.
int speedFactor();

} // namespace evaluator::platform
