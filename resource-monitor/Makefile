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
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ==============================================================================
# Testes
# ==============================================================================

.PHONY: tests
tests: directories $(TEST_BINS)
	@echo "All tests compiled successfully!"

# Compilar cada teste individualmente
$(BIN_DIR)/test_%: $(OBJ_DIR)/test_%.o $(filter-out $(OBJ_DIR)/main.o, $(OBJECTS))
	@echo "Linking test $@..."
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS)

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

SOURCES = $(wildcard $(SRC_DIR)/*.c)