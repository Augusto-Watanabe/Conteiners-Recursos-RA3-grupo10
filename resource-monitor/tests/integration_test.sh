#!/bin/bash

# ==============================================================================
# Resource Monitor - Integration Test Suite
# ==============================================================================
#
# Este script executa o programa 'resource-monitor' com vários argumentos
# para testar a integração entre o parsing de argumentos, o modo de
# monitoramento e o modo de execução com cgroups.
#
# Uso:
#   1. Certifique-se de que o projeto foi compilado com 'make'.
#   2. Execute este script a partir do diretório raiz do projeto:
#      sudo ./tests/integration_test.sh
#
# ==============================================================================

# Cores para a saída
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Variáveis
TARGET_BIN="./bin/resource-monitor"
TEST_FAIL=0
TEST_COUNT=0
LOG_FILE="/tmp/integration_test.log"
CGROUP_CPU_NAME="integration_test_cpu"
CGROUP_MEM_NAME="integration_test_mem"
CGROUP_V1_CPU_PATH="/sys/fs/cgroup/cpu/${CGROUP_CPU_NAME}"
CGROUP_V2_PATH="/sys/fs/cgroup/${CGROUP_CPU_NAME}"

# Função para executar um teste e verificar o resultado
run_test() {
    ((TEST_COUNT++))
    local description="$1"
    local command="$2"
    local check_text="$3"

    printf "Test %d: %-50s... " "$TEST_COUNT" "$description"

    # Executa o comando e redireciona a saída para o log
    eval "$command" > "$LOG_FILE" 2>&1
    local exit_code=$?

    if [[ $exit_code -ne 0 ]]; then
        printf "${RED}FAILED (Exit Code: %d)${NC}\n" "$exit_code"
        cat "$LOG_FILE"
        ((TEST_FAIL++))
        return
    fi

    if ! grep -q "$check_text" "$LOG_FILE"; then
        printf "${RED}FAILED (Check Text Not Found: '%s')${NC}\n" "$check_text"
        cat "$LOG_FILE"
        ((TEST_FAIL++))
        return
    fi

    printf "${GREEN}PASSED${NC}\n"
}

# --- Início dos Testes ---

echo "╔════════════════════════════════════════════════════════════╗"
echo "║         Resource Monitor - Integration Test Suite          ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

if [ ! -f "$TARGET_BIN" ]; then
    echo -e "${RED}Error: Executable '$TARGET_BIN' not found. Please run 'make' first.${NC}"
    exit 1
fi

if [ "$EUID" -ne 0 ]; then
    echo -e "${YELLOW}Warning: Cgroup tests require root. Some tests will be skipped.${NC}"
    echo "Run with: sudo $0"
fi

# Testes Básicos
run_test "Show help message" "$TARGET_BIN --help" "Usage (Monitoring Mode):"
run_test "Show version" "$TARGET_BIN --version" "Resource Monitor v1.0"

# Testes de Modo de Monitoramento
run_test "Monitor 'self' for 1 sample" "$TARGET_BIN -c 1 self" "Monitoring Summary"
run_test "Show namespace info for 'self'" "$TARGET_BIN -N self" "Namespaces for PID"

# Testes de Modo de Execução (requer root)
if [ "$EUID" -eq 0 ]; then
    # Teste de Limite de CPU
    run_test "Execution mode with CPU limit" \
             "sudo $TARGET_BIN --cgroup-name $CGROUP_CPU_NAME --cpu-limit 0.5 -- sleep 1" \
             "CPU limit set to 0.50 cores"

    # Teste de Limite de Memória
    run_test "Execution mode with Memory limit" \
             "sudo $TARGET_BIN --cgroup-name $CGROUP_MEM_NAME --mem-limit 128 -- sleep 1" \
             "Memory limit set to 128 MB"

    # Verifica se os cgroups foram limpos
    if [ -d "$CGROUP_V1_CPU_PATH" ] || [ -d "$CGROUP_V2_PATH" ]; then
        printf "${RED}Error: Cgroup cleanup failed. Directory still exists.${NC}\n"
        ((TEST_FAIL++))
    else
        printf "Test %d: %-50s... ${GREEN}PASSED${NC}\n" "$((++TEST_COUNT))" "Cgroup cleanup verification"
    fi
fi

echo ""
if [ $TEST_FAIL -eq 0 ]; then
    echo -e "${GREEN}✓ All $TEST_COUNT tests passed successfully!${NC}"
    exit 0
else
    echo -e "${RED}✗ $TEST_FAIL out of $TEST_COUNT tests failed.${NC}"
    exit 1
fi

