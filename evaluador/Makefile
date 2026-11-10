# Makefile del proyecto evaluador (C++17).
#   Linux (g++/clang++)            : make
#   Windows con MSYS2 UCRT64/MinGW : make        (detecta Windows solo; genera .exe)
#   Compilación cruzada desde Linux: make OS_WIN=1 CXX=x86_64-w64-mingw32-g++-posix
#
#   make            compila todo en bin/
#   make test       pruebas automáticas (Linux: tests/run_tests.sh, MSYS2: tests/run_tests_windows.sh)
#   make install PREFIX=/ruta    instala en otro directorio
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Isrc
BIN      := bin
OBJ      := build/obj

# En MSYS2 / Windows la variable de ambiente OS vale Windows_NT.
ifeq ($(OS),Windows_NT)
OS_WIN   := 1
endif

ifdef OS_WIN
EXE      := .exe
LDLIBS   := -static -pthread
CPPFLAGS += -DNOMINMAX -DWIN32_LEAN_AND_MEAN
else
EXE      :=
LDLIBS   := -pthread -lrt
endif

# Código común a todos los programas
COMMON_SRC := src/shared/platform.cpp src/shared/shm.cpp \
              src/concurrency/semaphore.cpp src/queues/ring.cpp
COMMON_OBJ := $(COMMON_SRC:src/%.cpp=$(OBJ)/%.o)

PROGRAMS := $(BIN)/evaluator$(EXE) $(BIN)/evaluator-init$(EXE) $(BIN)/evaluator-reg$(EXE) \
            $(BIN)/evaluator-ctrl$(EXE) $(BIN)/evaluator-rep$(EXE)

.PHONY: all clean install test
all: $(PROGRAMS)

$(OBJ)/%.o: src/%.cpp $(wildcard src/*/*.hpp)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Envoltorio (wrapper) y programas reales
$(BIN)/evaluator$(EXE): $(OBJ)/main/evaluator.o $(COMMON_OBJ)
	@mkdir -p $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)
$(BIN)/evaluator-init$(EXE): $(OBJ)/evaluator/init.o $(OBJ)/evaluator/evaluator.o $(COMMON_OBJ)
	@mkdir -p $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)
$(BIN)/evaluator-reg$(EXE): $(OBJ)/registration/reg.o $(COMMON_OBJ)
	@mkdir -p $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)
$(BIN)/evaluator-ctrl$(EXE): $(OBJ)/main/ctrl.o $(COMMON_OBJ)
	@mkdir -p $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)
$(BIN)/evaluator-rep$(EXE): $(OBJ)/output/rep.o $(COMMON_OBJ)
	@mkdir -p $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

ifdef OS_WIN
TEST_SCRIPT := tests/run_tests_windows.sh
else
TEST_SCRIPT := tests/run_tests.sh
endif

test: all
	bash $(TEST_SCRIPT)

clean:
	rm -rf build $(BIN)/evaluator $(BIN)/evaluator-* $(BIN)/*.exe

PREFIX ?= /usr/local
install: all
	install -d $(PREFIX)/bin
	install -m 755 $(PROGRAMS) $(PREFIX)/bin/
