#!/bin/bash

# ==============================================================================
# Experimento 2: Isolamento via Namespaces
# ==============================================================================

# Cores para a saída
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configurações
TOOL_SRC="experimentos/exp2_namespaces.c"
TOOL_BIN="bin/exp2_namespaces"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗"
echo -e "║           Experimento 2: Isolamento via Namespaces         ║"
echo -e "╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# --- Passo 1: Compilação ---
echo "Compilando a ferramenta de teste de namespace..."
make $TOOL_BIN > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${RED}Falha ao compilar a ferramenta. Verifique o Makefile.${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Compilação concluída.${NC}"
echo ""

# --- Passo 2: Execução dos Cenários ---

run_scenario() {
    local description="$1"
    shift
    local args="$@"

    echo -e "${BLUE}======================================================================${NC}"
    echo -e "${BLUE}Cenário: $description${NC}"
    echo -e "${BLUE}======================================================================${NC}"

    # Executa a ferramenta com sudo e captura a saída
    output=$(sudo ./$TOOL_BIN $args)

    # Extrai o tempo de criação
    creation_time=$(echo "$output" | awk -F: '/creation_time_ms/ {print $2}')

    # Imprime a saída da verificação
    echo "$output" | grep -v "creation_time_ms"

    printf "\n${GREEN}Tempo de criação e configuração do namespace: %.4f ms${NC}\n\n" "$creation_time"
    sleep 2
}

# Cenário 1: Sem isolamento (referência)
echo -e "${BLUE}======================================================================${NC}"
echo -e "${BLUE}Cenário: Linha de Base (Host - Sem Isolamento)${NC}"
echo -e "${BLUE}======================================================================${NC}"
echo "--- Visão de Processos do Host ---"
ps aux | head -n 10
echo "--- Visão de Rede do Host ---"
ip addr | head -n 10
echo ""

# Cenário 2: Isolamento de PID
run_scenario "Isolamento de Processos (PID Namespace)" --pid

# Cenário 3: Isolamento de Rede
run_scenario "Isolamento de Rede (Network Namespace)" --net

# Cenário 4: Isolamento de PID e Rede
run_scenario "Isolamento Combinado (PID + Network)" --pid --net

