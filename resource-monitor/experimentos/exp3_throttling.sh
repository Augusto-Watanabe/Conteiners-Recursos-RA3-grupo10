#!/bin/bash

# ==============================================================================
# Experimento 3: Precisão do Throttling de CPU
# ==============================================================================

# Cores para a saída
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configurações
WORKLOAD_BIN="bin/cpu_workload"
PROFILER_BIN="bin/resource-monitor"
ITERATIONS=500000000 # Número de iterações para a carga de trabalho
CPU_LIMITS=(0.25 0.5 1.0 2.0) # Limites de CPU a serem testados (em cores)

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗"
echo -e "║           Experimento 3: Precisão do Throttling de CPU       ║"
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

# --- Passo 2: Cenário A (Baseline) ---
echo "Executando Cenário A: Workload sem limite (Baseline)..."
baseline_output=$(./$WORKLOAD_BIN $ITERATIONS)
baseline_time=$(echo "$baseline_output" | awk -F'[,=]' '/WORKLOAD_RESULT/ {print $4}')
baseline_throughput=$(echo "scale=2; $ITERATIONS / $baseline_time" | bc)
echo -e "${GREEN}✓ Baseline concluída. Tempo: ${baseline_time}s, Throughput: ${baseline_throughput} iter/s${NC}"
echo ""

# --- Passo 3: Documentação dos Resultados ---
echo -e "${BLUE}╔═════════════════════════════════════════════════════════════════════════════════════╗"
echo -e "║                               Resultados do Experimento                                 ║"
echo -e "╚═════════════════════════════════════════════════════════════════════════════════════╝${NC}"
echo ""
printf "%-15s | %-15s | %-15s | %-15s | %-25s\n" "Limite (Cores)" "Uso Real (%)" "Desvio (%)" "Tempo (s)" "Throughput (iter/s)"
echo "-----------------------------------------------------------------------------------------------"
printf "%-15s | %-15s | %-15s | %-15.2f | %-25.0f\n" "Ilimitado" "100.00" "N/A" "$baseline_time" "$baseline_throughput"

# --- Passo 4: Cenário B (Com Limites de CPU) ---
for limit in "${CPU_LIMITS[@]}"; do
    echo ""
    echo "Executando Cenário B: Workload com limite de ${limit} cores..."

    # Executa o profiler em modo de execução e captura toda a saída
    output=$(sudo $PROFILER_BIN --cpu-limit "$limit" -- ./$WORKLOAD_BIN $ITERATIONS 2>&1)

    # 1. Extrair métricas do workload
    workload_time=$(echo "$output" | awk -F'[,=]' '/WORKLOAD_RESULT/ {print $4}')
    if [ -z "$workload_time" ] || (( $(echo "$workload_time <= 0" | bc -l) )); then
        echo -e "${RED}  Falha ao extrair o tempo de execução do workload.${NC}"
        continue
    fi
    throughput=$(echo "scale=2; $ITERATIONS / $workload_time" | bc)

    # 2. Extrair métricas do cgroup (uso total de CPU em segundos)
    # A saída do profiler é "Usage: X.XX seconds"
    cpu_usage_sec=$(echo "$output" | awk '/CPU Metrics:/, /Usage:/ {if ($1 == "Usage:") print $2}')
    if [ -z "$cpu_usage_sec" ]; then
        echo -e "${RED}  Falha ao extrair o uso de CPU do profiler.${NC}"
        continue
    fi

    # 3. Calcular métricas do experimento
    # Uso real de CPU = (total de segundos de CPU consumidos) / (tempo total de execução)
    # Multiplicamos por 100 para obter a porcentagem
    measured_cpu_percent=$(echo "scale=2; ($cpu_usage_sec / $workload_time) * 100" | bc)

    # Desvio = ((Uso Real / Limite Configurado) - 1) * 100
    limit_percent=$(echo "scale=2; $limit * 100" | bc)
    deviation=$(echo "scale=2; (($measured_cpu_percent / $limit_percent) - 1) * 100" | bc)

    # 4. Imprimir na tabela de resultados
    printf "%-15.2f | %-15.2f | %-15.2f | %-15.2f | %-25.0f\n" \
        "$limit" "$measured_cpu_percent" "$deviation" "$workload_time" "$throughput"

done

echo "-----------------------------------------------------------------------------------------------"
echo ""
echo "Conclusão:"
echo "• O 'Uso Real' mostra a porcentagem de um núcleo de CPU que o processo efetivamente usou."
echo "• O 'Desvio' indica a precisão do cgroup. Valores próximos de zero são ideais."
echo "• O 'Throughput' demonstra o impacto direto do limite de CPU no desempenho da aplicação."
echo ""
