#!/bin/bash

# ==============================================================================
# Experimento 1: Medição de Overhead do Profiler
# ==============================================================================

# Cores para a saída
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configurações
WORKLOAD_SRC="experimentos/cpu_workload.c"
WORKLOAD_BIN="bin/cpu_workload"
PROFILER_BIN="bin/resource-monitor"
NUM_RUNS=5 # Número de execuções para calcular a média
INTERVALS=(1 0.5 0.1) # Intervalos de monitoramento a serem testados (em segundos)

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗"
echo -e "║         Experimento 1: Overhead de Monitoramento           ║"
echo -e "╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# --- Passo 1: Compilação ---
echo "Compilando o workload de referência e o profiler..."
make $WORKLOAD_BIN > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${RED}Falha ao compilar o workload. Verifique o Makefile.${NC}"
    exit 1
fi

make $PROFILER_BIN > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${RED}Falha ao compilar o profiler. Verifique o Makefile.${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Compilação concluída.${NC}"
echo ""

# --- Passo 2: Cenário A (Baseline) ---
echo "Executando Cenário A: Workload sem monitoramento (Baseline)..."
total_baseline_time=0
for i in $(seq 1 $NUM_RUNS); do
    # O comando `time -p` imprime o tempo "real" (wall-clock) no stderr
    exec_time=$( { time -p ./$WORKLOAD_BIN > /dev/null; } 2>&1 | awk '/real/ {print $2}' )
    total_baseline_time=$(echo "$total_baseline_time + $exec_time" | bc)
    printf "  Run %d/%d: %.4f segundos\n" "$i" "$NUM_RUNS" "$exec_time"
done
avg_baseline_time=$(echo "scale=4; $total_baseline_time / $NUM_RUNS" | bc)
echo -e "${GREEN}✓ Tempo médio da baseline: ${avg_baseline_time} segundos${NC}"
echo ""

# --- Passo 4: Documentação dos Resultados ---
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗"
echo -e "║                 Resultados do Experimento                  ║"
echo -e "╚════════════════════════════════════════════════════════════╝${NC}"
echo ""
printf "%-20s | %-20s | %-15s | %-15s\n" "Intervalo (s)" "Tempo Médio (s)" "Overhead (%)" "Latência Média (ms)"
echo "------------------------------------------------------------------------------------"
printf "%-20s | %-20.4f | %-15s | %-15s\n" "Baseline" "$avg_baseline_time" "N/A" "N/A"

# --- Passo 3: Cenário B (Com Profiler em diferentes intervalos) ---
for interval in "${INTERVALS[@]}"; do
    echo ""
    echo "Executando Cenário B: Workload com monitoramento (Intervalo: ${interval}s)..."
    total_monitored_time=0
    total_latency=0
    sample_count=0

    for i in $(seq 1 $NUM_RUNS); do
        # Inicia o workload em background
        ./$WORKLOAD_BIN > /dev/null &
        WORKLOAD_PID=$!

        # Inicia o profiler para monitorar o workload e capturar a latência
        latency_output=$(./$PROFILER_BIN -q -i "$interval" $WORKLOAD_PID)
        PROFILER_PID=$(pgrep -f "$PROFILER_BIN.*$WORKLOAD_PID")

        # Mede o tempo de execução do workload
        exec_time=$( { time -p wait $WORKLOAD_PID; } 2>&1 | awk '/real/ {print $2}' )
        total_monitored_time=$(echo "$total_monitored_time + $exec_time" | bc)
        
        # Garante que o profiler também termine
        kill $PROFILER_PID 2>/dev/null

        # Processa a latência
        current_run_latency=$(echo "$latency_output" | awk -F: '/sample_latency_ms/ {sum+=$2; count++} END {if (count>0) print sum; else print 0}')
        current_run_samples=$(echo "$latency_output" | awk -F: '/sample_latency_ms/ {count++} END {print count}')
        total_latency=$(echo "$total_latency + $current_run_latency" | bc)
        sample_count=$(echo "$sample_count + $current_run_samples" | bc)

        printf "  Run %d/%d: %.4f segundos\n" "$i" "$NUM_RUNS" "$exec_time"
    done

    avg_monitored_time=$(echo "scale=4; $total_monitored_time / $NUM_RUNS" | bc)
    avg_latency=$(echo "scale=4; if($sample_count > 0) $total_latency / $sample_count else 0" | bc)

    # Calcula overhead
    overhead_abs=$(echo "scale=4; $avg_monitored_time - $avg_baseline_time" | bc)
    overhead_rel="0.00"
    if (( $(echo "$avg_baseline_time > 0" | bc -l) )); then
        overhead_rel=$(echo "scale=2; ($overhead_abs / $avg_baseline_time) * 100" | bc)
    fi

    # Imprime na tabela de resultados
    printf "%-20.1f | %-20.4f | %-15.2f | %-15.4f\n" "$interval" "$avg_monitored_time" "$overhead_rel" "$avg_latency"
done

echo "------------------------------------------------------------------------------------"
echo ""
echo "Conclusão: O overhead (impacto no tempo de execução) aumenta conforme o intervalo de"
echo "monitoramento diminui. A latência de amostragem representa o tempo gasto pelo profiler"
echo "em cada ciclo de coleta de dados."
echo ""
