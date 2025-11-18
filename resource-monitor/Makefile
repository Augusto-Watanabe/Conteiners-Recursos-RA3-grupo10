# ==============================================================================
# Resource Monitor - Makefile
# ==============================================================================

# Compilador e flags
CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -Werror -std=c11 -pedantic
CXXFLAGS = -Wall -Wextra -Werror -std=c++23 -pedantic
INCLUDES = -Iinclude
LDFLAGS = 
LIBS = -lm

# Diretórios
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
TEST_DIR = tests
DOC_DIR = docs

# Arquivos fonte
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Testes
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_BINS = $(TEST_SOURCES:$(TEST_DIR)/%.c=$(BIN_DIR)/%)
EXPERIMENT_DIR = experimentos

# Executável principal
TARGET = $(BIN_DIR)/resource-monitor

# Headers
HEADERS = $(wildcard $(INC_DIR)/*.h)

# ==============================================================================
# Regras principais
# ==============================================================================

# Regra padrão
.PHONY: all
all: directories $(TARGET)

# Criar diretórios necessários
.PHONY: directories
directories:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BIN_DIR)

# Compilar executável principal
$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(LIBS)
	@echo "Build successful!"

# Compilar arquivos .c em .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ==============================================================================
# Testes
# ==============================================================================

.PHONY: tests
tests: directories $(TEST_BINS)
	@echo "All tests compiled successfully!"

# Define os objetos da biblioteca (todos os .o de src/ exceto main.o)
LIB_OBJECTS = $(filter-out $(OBJ_DIR)/main.o, $(OBJECTS))

# Compilar cada teste individualmente
$(BIN_DIR)/test_%: $(OBJ_DIR)/test_%.o $(LIB_OBJECTS)
	@echo "Linking test $@..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS)

$(OBJ_DIR)/test_%.o: $(TEST_DIR)/test_%.c

# Compilar arquivos de teste
$(OBJ_DIR)/test_%.o: $(TEST_DIR)/test_%.c $(HEADERS)
	@echo "Compiling test $<..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Executar todos os testes
.PHONY: run-tests
run-tests: tests
	@echo "Running tests..."
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		sudo $$test || exit 1; \
	done
	@echo "All tests passed!"

.PHONY: integration-test
integration-test: all
	@echo "Running integration tests..."
	@chmod +x $(TEST_DIR)/integration_test.sh
	sudo $(TEST_DIR)/integration_test.sh

# ==============================================================================
# Experimentos
# ==============================================================================

.PHONY: experiment-overhead
experiment-overhead: $(BIN_DIR)/cpu_workload all
	@chmod +x $(EXPERIMENT_DIR)/exp1_overhead.sh
	@./$(EXPERIMENT_DIR)/exp1_overhead.sh

# Regra para compilar o workload do experimento
$(BIN_DIR)/cpu_workload: $(EXPERIMENT_DIR)/cpu_workload.c
	@echo "Compiling CPU workload..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

.PHONY: experiment-namespaces
experiment-namespaces: $(BIN_DIR)/exp2_namespaces all
	@chmod +x $(EXPERIMENT_DIR)/exp2_namespaces.sh
	@./$(EXPERIMENT_DIR)/exp2_namespaces.sh

# Regra para compilar a ferramenta do experimento 2
$(BIN_DIR)/exp2_namespaces: $(EXPERIMENT_DIR)/exp2_namespaces.c
	@echo "Compiling Namespace experiment tool..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

.PHONY: experiment-throttling
experiment-throttling: $(BIN_DIR)/cpu_workload all
	@chmod +x $(EXPERIMENT_DIR)/exp3_throttling.sh
	@./$(EXPERIMENT_DIR)/exp3_throttling.sh

.PHONY: experiment-memory
experiment-memory: $(BIN_DIR)/mem_workload all
	@chmod +x $(EXPERIMENT_DIR)/exp4_memory_limit.sh
	@./$(EXPERIMENT_DIR)/exp4_memory_limit.sh

# Regra para compilar o workload do experimento 4
$(BIN_DIR)/mem_workload: $(EXPERIMENT_DIR)/mem_workload.c
	@echo "Compiling Memory workload..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

.PHONY: experiment-io
experiment-io: $(BIN_DIR)/io_workload all
	@chmod +x $(EXPERIMENT_DIR)/exp5_io_limit.sh
	@./$(EXPERIMENT_DIR)/exp5_io_limit.sh

# Regra para compilar o workload do experimento 5
$(BIN_DIR)/io_workload: $(EXPERIMENT_DIR)/io_workload.c
	@echo "Compiling I/O workload..."
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)




# ==============================================================================
# Limpeza
# ==============================================================================

.PHONY: clean
clean:
	@echo "Cleaning build files..."
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Clean complete!"

.PHONY: clean-all
clean-all: clean
	@echo "Removing all generated files..."
	rm -f core vgcore.* *.log
	find . -name "*~" -delete
	find . -name "*.csv" -delete
	find . -name "*.json" -delete
	@echo "Deep clean complete!"

# ==============================================================================
# Verificação e validação
# ==============================================================================

.PHONY: check
check:
	@echo "Checking for warnings..."
	$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only $(SOURCES)
	@echo "No warnings found!"

.PHONY: valgrind
valgrind: $(TARGET)
	@echo "Running valgrind memory check..."
	valgrind --leak-check=full \
	         --show-leak-kinds=all \
	         --track-origins=yes \
	         --verbose \
	         --log-file=valgrind.log \
	         $(TARGET) --help
	@echo "Valgrind report saved to valgrind.log"

.PHONY: valgrind-tests
valgrind-tests: tests
	@echo "Running valgrind on tests..."
	@for test in $(TEST_BINS); do \
		echo "Checking $$test with valgrind..."; \
		valgrind --leak-check=full --error-exitcode=1 $$test || exit 1; \
	done

# ==============================================================================
# Instalação e desinstalação
# ==============================================================================

PREFIX ?= /usr/local
INSTALL_BIN = $(PREFIX)/bin

.PHONY: install
install: $(TARGET)
	@echo "Installing $(TARGET) to $(INSTALL_BIN)..."
	install -d $(INSTALL_BIN)
	install -m 755 $(TARGET) $(INSTALL_BIN)/
	@echo "Installation complete!"

.PHONY: uninstall
uninstall:
	@echo "Removing $(INSTALL_BIN)/resource-monitor..."
	rm -f $(INSTALL_BIN)/resource-monitor
	@echo "Uninstallation complete!"

# ==============================================================================
# Utilitários
# ==============================================================================

.PHONY: run
run: $(TARGET)
	@echo "Running $(TARGET)..."
	sudo $(TARGET)

.PHONY: debug
debug: CFLAGS += -g -O0 -DDEBUG
debug: clean all
	@echo "Debug build complete!"

.PHONY: release
release: CFLAGS += -O3 -DNDEBUG
release: clean all
	@echo "Release build complete!"

.PHONY: info
info:
	@echo "=== Build Information ==="
	@echo "CC:        $(CC)"
	@echo "CFLAGS:    $(CFLAGS)"
	@echo "INCLUDES:  $(INCLUDES)"
	@echo "SOURCES:   $(SOURCES)"
	@echo "OBJECTS:   $(OBJECTS)"
	@echo "TARGET:    $(TARGET)"
	@echo "========================="

.PHONY: help
help:
	@echo "Resource Monitor - Available Make Targets"
	@echo "=========================================="
	@echo "  all          : Build the main program (default)"
	@echo "  tests        : Build all test programs"
	@echo "  experiment-io: Run the I/O limit precision experiment"
	@echo "  experiment-memory: Run the memory limit enforcement experiment"
	@echo "  experiment-throttling: Run the CPU throttling precision experiment"
	@echo "  experiment-namespaces: Run the namespace isolation experiment"
	@echo "  experiment-overhead: Run the monitoring overhead experiment"
	@echo "  integration-test: Run integration test script"
	@echo "  run-tests    : Build and run all tests"
	@echo "  run          : Build and run the main program"
	@echo "  clean        : Remove build artifacts"
	@echo "  clean-all    : Deep clean (includes logs, CSVs, etc.)"
	@echo "  check        : Check for compilation warnings"
	@echo "  valgrind     : Run valgrind on main program"
	@echo "  valgrind-tests: Run valgrind on all tests"
	@echo "  debug        : Build with debug symbols"
	@echo "  release      : Build optimized release version"
	@echo "  install      : Install to system (requires sudo)"
	@echo "  uninstall    : Remove from system"
	@echo "  info         : Show build configuration"
	@echo "  help         : Show this help message"

# ==============================================================================
# Dependências automáticas (opcional, mas muito útil)
# ==============================================================================

-include $(OBJECTS:.o=.d)
-include $(TEST_OBJECTS:.o=.d)

$(OBJ_DIR)/%.d: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT '$(OBJ_DIR)/$*.o' $< > $@

$(OBJ_DIR)/%.d: $(TEST_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT '$(OBJ_DIR)/$*.o' $< > $@
