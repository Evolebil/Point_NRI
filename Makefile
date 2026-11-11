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

# модульные тесты на C++ (отдельные функции и классы): make unit
build/unit_tests: tests/unit_tests.cc $(DEPS) $(wildcard src/kampaniya/*)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -Isrc/kampaniya -o $@ $<
unit: build/unit_tests
	./build/unit_tests

# стресс-тесты устойчивости: make stress
build/stress_tests: tests/stress_tests.cc $(DEPS) $(wildcard src/kampaniya/*)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -g $(INC) -Isrc/kampaniya -o $@ $<
stress: build/stress_tests
	./build/stress_tests

# кампания (исследование и социалка): свои заголовки в src/kampaniya — их правка боевые программы не пересобирает
KDEPS := $(wildcard src/kampaniya/*)
build/kampaniya: src/apps/kampaniya.cc $(DEPS) $(KDEPS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -Isrc/kampaniya -o $@ $<
build/kampaniya_tests: tests/kampaniya_tests.cc $(DEPS) $(KDEPS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -Isrc/kampaniya -o $@ $<
test_kampaniya: build/kampaniya_tests
	./build/kampaniya_tests

$(APPS) audit_tests kampaniya kampaniya_tests: %: build/%

test: build/audit_tests
	./tests/run_tests.sh

clean:
	rm -rf build

.PHONY: all test unit stress clean test_kampaniya kampaniya kampaniya_tests $(APPS) audit_tests
