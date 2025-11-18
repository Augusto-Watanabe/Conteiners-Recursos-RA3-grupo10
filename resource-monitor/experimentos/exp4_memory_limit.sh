#!/bin/bash

# ==============================================================================
# Experimento 4: Comportamento sob Limite de Memória
# ==============================================================================

# Cores para a saída
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Configurações
WORKLOAD_BIN="bin/mem_workload"
PROFILER_BIN="bin/resource-monitor"
MEM_LIMIT_MB=100

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗"
echo -e "║        Experimento 4: Comportamento sob Limite de Memória      ║"
echo -e "╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# --- Passo 1: Compilação ---
echo "Compilando as ferramentas necessárias..."
make $WORKLOAD_BIN $PROFILER_BIN > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${RED}Falha na compilação. Verifique o Makefile.${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Compilação concluída.${NC}"
echo ""

# --- Passo 2: Execução do Experimento ---
echo "Executando workload em cgroup com limite de ${MEM_LIMIT_MB}MB..."

# Executa o profiler em modo de execução e captura toda a saída
# A saída do workload (stderr) é redirecionada para a do profiler (stdout)
output=$(sudo $PROFILER_BIN --mem-limit "$MEM_LIMIT_MB" -- ./$WORKLOAD_BIN 2>&1)
exit_code=$?

echo "$output"
echo ""

# --- Passo 3: Análise e Relatório ---
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗"
echo -e "║                 Resultados do Experimento                  ║"
echo -e "╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# 1. Analisar o comportamento do processo
printf "%-35s: " "Comportamento do Processo"
if [ $exit_code -eq 137 ]; then # 128 + 9 (SIGKILL)
    echo -e "${YELLOW}Processo terminado pelo OOM Killer (Exit Code 137)${NC}"
elif [ $exit_code -eq 0 ]; then
    echo -e "${GREEN}Processo terminou normalmente (malloc falhou)${NC}"
else
    echo -e "${RED}Processo terminou com erro inesperado (Exit Code: $exit_code)${NC}"
fi

# 2. Analisar a memória máxima alocada pelo workload
max_allocated=$(echo "$output" | awk -F: '/MAX_ALLOCATED_MB/ {print $2}')
if [ -n "$max_allocated" ]; then
    printf "%-35s: %s MB\n" "Máximo alocado (reportado pelo app)" "$max_allocated"
fi

# 3. Analisar o pico de memória reportado pelo cgroup
peak_mem=$(echo "$output" | awk '/Peak:/ {print $2, $3}')
if [ -n "$peak_mem" ]; then
    printf "%-35s: %s\n" "Pico de memória (reportado pelo cgroup)" "$peak_mem"
fi

# 4. Analisar o contador de falhas de alocação do cgroup
fail_count=$(echo "$output" | awk '/memory.failcnt/ {print $2}')
if [ -n "$fail_count" ]; then
    printf "%-35s: %s\n" "Contador de falhas (memory.failcnt)" "$fail_count"
fi

echo ""
echo "Conclusão: O experimento demonstra como o cgroup de memória age ao ser pressionado."
echo "O processo pode ser terminado pelo OOM Killer ou falhar ao alocar memória, dependendo"
echo "da configuração do sistema e da velocidade de alocação."
echo ""
