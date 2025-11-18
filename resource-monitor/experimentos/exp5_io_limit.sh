#!/bin/bash

# ==============================================================================
# Experimento 5: Precisão da Limitação de I/O
# ==============================================================================

# Cores para a saída
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configurações
WORKLOAD_BIN="bin/io_workload"
PROFILER_BIN="bin/resource-monitor"
TEST_FILE="/tmp/io_workload_testfile.tmp"

# Limites de I/O para testar (em Bytes por Segundo)
ONE_MBPS=$((1024 * 1024))
TEN_MBPS=$((10 * 1024 * 1024))
FIFTY_MBPS=$((50 * 1024 * 1024))
IO_LIMITS=($TEN_MBPS $FIFTY_MBPS)

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗"
echo -e "║            Experimento 5: Precisão da Limitação de I/O         ║"
echo -e "╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# --- Passo 1: Compilação e Preparação ---
echo "Compilando as ferramentas e encontrando o dispositivo de teste..."
make $WORKLOAD_BIN $PROFILER_BIN > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${RED}Falha na compilação. Verifique o Makefile.${NC}"
    exit 1
fi

# Encontra o dispositivo (major:minor) para o diretório /tmp
# We use stat to get the device number of the filesystem containing the test file,
# then use ls -l on the device file to get the major:minor numbers. This is more portable.
touch $TEST_FILE
DEVICE_SOURCE=$(df $TEST_FILE | tail -n 1 | awk '{print $1}')
if [ ! -b "$DEVICE_SOURCE" ]; then
    echo -e "${RED}Could not find block device for $TEST_FILE. Source: '$DEVICE_SOURCE'${NC}"
    exit 1
fi
DEVICE_MAJ_MIN=$(ls -l "$DEVICE_SOURCE" | awk '{gsub(",", ""); printf "%s:%s", $5, $6}')
if [ -z "$DEVICE_MAJ_MIN" ]; then
    echo -e "${RED}Não foi possível determinar o dispositivo para $TEST_FILE.${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Compilação concluída. Dispositivo de teste: $DEVICE_MAJ_MIN${NC}"
echo ""

# --- Passo 2: Cenário A (Baseline) ---
echo "Executando Cenário A: Workload sem limite (Baseline)..."
baseline_output=$(./$WORKLOAD_BIN)
baseline_write_mbps=$(echo "$baseline_output" | awk -F'[,=]' '/WORKLOAD_RESULT/ {print $2}')
baseline_read_mbps=$(echo "$baseline_output" | awk -F'[,=]' '/WORKLOAD_RESULT/ {print $3}')
baseline_time=$(echo "$baseline_output" | awk -F'[,=]' '/WORKLOAD_RESULT/ {w=$4; r=$5; print w+r}')
echo -e "${GREEN}✓ Baseline concluída. Tempo: ${baseline_time}s, Throughput (W/R): ${baseline_write_mbps}/${baseline_read_mbps} MB/s${NC}"
echo ""

# --- Passo 3: Documentação dos Resultados ---
echo -e "${BLUE}╔═════════════════════════════════════════════════════════════════════════════════════════════════════════╗"
echo -e "║                                       Resultados do Experimento                                         ║"
echo -e "╚═════════════════════════════════════════════════════════════════════════════════════════════════════════╝${NC}"
echo ""
printf "%-20s | %-20s | %-20s | %-15s\n" "Limite (MB/s)" "Throughput Real (MB/s)" "Desvio (%)" "Tempo Total (s)"
echo "---------------------------------------------------------------------------------------------------"
printf "%-20s | %-20s | %-20s | %-15.2f\n" "Ilimitado (Write)" "$baseline_write_mbps" "N/A" "$baseline_time"
printf "%-20s | %-20s | %-20s | %-15s\n" "Ilimitado (Read)" "$baseline_read_mbps" "N/A" ""

# --- Passo 4: Cenário B (Com Limites de I/O) ---
for limit_bps in "${IO_LIMITS[@]}"; do
    limit_mbps=$(($limit_bps / 1024 / 1024))
    echo ""
    echo "Executando Cenário B: Workload com limite de ${limit_mbps} MB/s..."

    # Formato do limite: "major:minor read_bps write_bps"
    io_limit_arg="${DEVICE_MAJ_MIN}:${limit_bps}:${limit_bps}"

    # Executa o profiler em modo de execução e captura toda a saída
    output=$(sudo $PROFILER_BIN --io-limit "$io_limit_arg" -- ./$WORKLOAD_BIN 2>&1)

    # 1. Extrair métricas do workload
    write_mbps=$(echo "$output" | awk -F'[,=]' '/WORKLOAD_RESULT/ {print $2}')
    read_mbps=$(echo "$output" | awk -F'[,=]' '/WORKLOAD_RESULT/ {print $3}')
    total_time=$(echo "$output" | awk -F'[,=]' '/WORKLOAD_RESULT/ {w=$4; r=$5; print w+r}')

    if [ -z "$write_mbps" ] || [ -z "$read_mbps" ]; then
        echo -e "${RED}  Falha ao extrair o throughput do workload.${NC}"
        continue
    fi

    # 2. Calcular desvio
    write_deviation=$(echo "scale=2; (($write_mbps / $limit_mbps) - 1) * 100" | bc)
    read_deviation=$(echo "scale=2; (($read_mbps / $limit_mbps) - 1) * 100" | bc)

    # 3. Imprimir na tabela de resultados
    printf "%-20s | %-20.2f | %-20.2f | %-15.2f\n" "${limit_mbps} MB/s (Write)" "$write_mbps" "$write_deviation" "$total_time"
    printf "%-20s | %-20.2f | %-20.2f | %-15s\n" "${limit_mbps} MB/s (Read)" "$read_mbps" "$read_deviation" ""

done

echo "---------------------------------------------------------------------------------------------------"
echo ""
echo "Conclusão:"
echo "• O 'Throughput Real' mostra a velocidade de leitura/escrita efetivamente medida."
echo "• O 'Desvio' indica a precisão do cgroup de I/O. Valores próximos de zero são ideais."
echo "• O 'Tempo Total' demonstra o impacto direto da limitação de I/O no tempo de execução."
echo ""
