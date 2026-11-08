# Сборка всех программ в build/: make (всё), make test (тесты), make arena_play и т. д.
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -pthread
INC := -Isrc/engine -Isrc/ai
# движок — один блок: любая правка любого заголовка пересобирает всё (программы включают движок целиком)
DEPS := $(wildcard src/engine/*) $(wildcard src/ai/*)
APPS := arena arena_play net_fighter net_builder tiers

all: $(addprefix build/,$(APPS))

build/%: src/apps/%.cc $(DEPS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -o $@ $<
build/audit_tests: tests/audit_tests.cc $(wildcard tests/audit/*.inc) $(DEPS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -o $@ $<

$(APPS) audit_tests: %: build/%

test: build/audit_tests
	./tests/run_tests.sh

clean:
	rm -rf build

.PHONY: all test clean $(APPS) audit_tests
