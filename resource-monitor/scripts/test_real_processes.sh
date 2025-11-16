#!/bin/bash

set -e

COLOR_GREEN="\033[0;32m"
COLOR_RED="\033[0;31m"
COLOR_YELLOW="\033[0;33m"
COLOR_RESET="\033[0m"

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║      Testing Resource Monitor with Real Processes         ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Verificar se está executando como root
if [ "$EUID" -ne 0 ]; then
    echo -e "${COLOR_YELLOW}Warning: Not running as root. I/O monitoring will fail.${COLOR_RESET}"
    echo "Run with: sudo $0"
    echo ""
fi

# Verificar se binário existe
if [ ! -f "./bin/resource-monitor" ]; then
    echo -e "${COLOR_RED}Error: ./bin/resource-monitor not found${COLOR_RESET}"
    echo "Run 'make' first"
    exit 1
fi

echo "Test 1: Monitor init process (PID 1)"
echo "-------------------------------------"
timeout 5s ./bin/resource-monitor -c 3 1 || true
echo ""

echo "Test 2: Monitor bash shell"
echo "-------------------------------------"
BASH_PID=$(pgrep -o bash | head -1)
if [ -n "$BASH_PID" ]; then
    timeout 5s ./bin/resource-monitor -c 3 "$BASH_PID" || true
else
    echo -e "${COLOR_YELLOW}No bash process found, skipping${COLOR_RESET}"
fi
echo ""

echo "Test 3: Monitor self process"
echo "-------------------------------------"
timeout 5s ./bin/resource-monitor -c 3 self || true
echo ""

echo "Test 4: Export to CSV"
echo "-------------------------------------"
TEST_CSV="/tmp/test_monitor.csv"
rm -f "$TEST_CSV"
./bin/resource-monitor -c 5 -o "$TEST_CSV" self
if [ -f "$TEST_CSV" ]; then
    echo -e "${COLOR_GREEN}✓ CSV file created${COLOR_RESET}"
    echo "First 3 lines:"
    head -3 "$TEST_CSV"
    rm -f "$TEST_CSV"
else
    echo -e "${COLOR_RED}✗ CSV file not created${COLOR_RESET}"
fi
echo ""

echo "Test 5: Export to JSON"
echo "-------------------------------------"
TEST_JSON="/tmp/test_monitor.json"
rm -f "$TEST_JSON"
./bin/resource-monitor -c 3 -o "$TEST_JSON" -f json self
if [ -f "$TEST_JSON" ]; then
    echo -e "${COLOR_GREEN}✓ JSON file created${COLOR_RESET}"
    echo "Content preview:"
    head -20 "$TEST_JSON"
    rm -f "$TEST_JSON"
else
    echo -e "${COLOR_RED}✗ JSON file not created${COLOR_RESET}"
fi
echo ""

echo "Test 6: CPU-only monitoring"
echo "-------------------------------------"
timeout 5s ./bin/resource-monitor -m cpu -c 3 self || true
echo ""

echo "Test 7: Memory-only monitoring"
echo "-------------------------------------"
timeout 5s ./bin/resource-monitor -m mem -c 3 self || true
echo ""

echo "Test 8: Summary mode"
echo "-------------------------------------"
timeout 10s ./bin/resource-monitor -s -c 10 self || true
echo ""

echo "Test 9: Invalid PID handling"
echo "-------------------------------------"
./bin/resource-monitor 999999 2>&1 | head -5 || true
echo ""

echo "Test 10: Stress test - CPU intensive process"
echo "-------------------------------------"
echo "Starting CPU-intensive background process..."
yes > /dev/null &
STRESS_PID=$!
sleep 2
echo "Monitoring stressed process (PID: $STRESS_PID)..."
timeout 10s ./bin/resource-monitor -c 5 "$STRESS_PID" || true
kill $STRESS_PID 2>/dev/null || true
echo ""

echo "╔════════════════════════════════════════════════════════════╗"
echo "║                   Testing Complete                         ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
