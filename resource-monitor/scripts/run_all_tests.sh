#!/bin/bash

set -e

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║        Resource Monitor - Complete Test Suite             ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# 1. Compilação
echo "Step 1: Compilation Check"
echo "========================="
make clean
make
echo "✓ Compilation successful"
echo ""

# 2. Testes unitários
echo "Step 2: Unit Tests"
echo "=================="
if [ -f "./bin/test_validation" ]; then
    sudo ./bin/test_validation
else
    echo "Warning: test_validation not found"
fi
echo ""

# 3. Testes com processos reais
echo "Step 3: Real Process Tests"
echo "=========================="
sudo ./scripts/test_real_processes.sh
echo ""

# 4. Verificação de memory leaks (opcional)
if command -v valgrind &> /dev/null; then
    echo "Step 4: Memory Leak Check"
    echo "========================="
    ./scripts/check_memory_leaks.sh
    echo ""
else
    echo "Step 4: Memory Leak Check (Skipped - valgrind not installed)"
    echo ""
fi

# 5. Testes de outros componentes
echo "Step 5: Component Tests"
echo "======================="
if [ -f "./bin/test_profiler" ]; then
    ./bin/test_profiler
fi
echo ""

# Resumo
echo "╔════════════════════════════════════════════════════════════╗"
echo "║                  All Tests Complete!                       ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
