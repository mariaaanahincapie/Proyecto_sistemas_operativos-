# Makefile del proyecto evaluador (C++17, Linux)
# Compila solo los programas cuyo archivo principal ya existe.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -pthread
LDLIBS   ?= -lrt -pthread
BIN      := bin

COMMON := $(wildcard src/shared/*.cpp src/queues/*.cpp src/concurrency/*.cpp)

# programa -> archivo principal
MAINS := evaluator:src/main/evaluator.cpp \
         evaluator-init:src/evaluator/init.cpp \
         evaluator-reg:src/registration/reg.cpp \
         evaluator-ctrl:src/main/ctrl.cpp \
         evaluator-rep:src/output/rep.cpp

TARGETS := $(foreach m,$(MAINS),$(if $(wildcard $(word 2,$(subst :, ,$(m)))),$(BIN)/$(word 1,$(subst :, ,$(m)))))

.PHONY: all clean install
all: $(TARGETS)
	@if [ -z "$(TARGETS)" ]; then echo "Aun no hay codigo fuente que compilar."; fi

$(BIN)/evaluator: src/main/evaluator.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)
$(BIN)/evaluator-init: src/evaluator/init.cpp $(wildcard src/evaluator/*.cpp) $(COMMON)
	$(CXX) $(CXXFLAGS) $(sort $^) -o $@ $(LDLIBS)
$(BIN)/evaluator-reg: src/registration/reg.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)
$(BIN)/evaluator-ctrl: src/main/ctrl.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)
$(BIN)/evaluator-rep: src/output/rep.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -f $(BIN)/evaluator $(BIN)/evaluator-*

# make install PREFIX=/ruta/destino
PREFIX ?= /usr/local
install: all
	install -d $(PREFIX)/bin
	install -m 755 $(BIN)/evaluator* $(PREFIX)/bin/
