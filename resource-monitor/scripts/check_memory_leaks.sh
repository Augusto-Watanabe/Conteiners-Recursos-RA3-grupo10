#!/bin/bash

echo "Checking for memory leaks with valgrind..."
echo ""

if ! command -v valgrind &> /dev/null; then
    echo "Error: valgrind not installed"
    echo "Install with: sudo apt install valgrind"
    exit 1
fi

echo "Test 1: Basic monitoring"
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind_basic.log \
         ./bin/resource-monitor -c 3 self

if grep -q "ERROR SUMMARY: 0 errors" valgrind_basic.log; then
    echo "✓ No memory errors detected"
else
    echo "✗ Memory errors found - check valgrind_basic.log"
fi

echo ""
echo "Test 2: With export"
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --log-file=valgrind_export.log \
         ./bin/resource-monitor -c 3 -o /tmp/test.csv self

rm -f /tmp/test.csv

if grep -q "ERROR SUMMARY: 0 errors" valgrind_export.log; then
    echo "✓ No memory errors in export"
else
    echo "✗ Memory errors found - check valgrind_export.log"
fi

echo ""
echo "Valgrind logs saved to:"
echo "  - valgrind_basic.log"
echo "  - valgrind_export.log"
