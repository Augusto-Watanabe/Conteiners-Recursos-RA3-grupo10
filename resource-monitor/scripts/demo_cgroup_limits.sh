#!/bin/bash

echo "=== Cgroup Limits Demonstration ==="
echo ""

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root: sudo $0"
    exit 1
fi

# Create test cgroup
echo "1. Creating test cgroup..."
mkdir -p /sys/fs/cgroup/demo_test
echo ""

# Set CPU limit to 0.5 cores
echo "2. Setting CPU limit to 50%..."
echo "50000 100000" > /sys/fs/cgroup/demo_test/cpu.max
cat /sys/fs/cgroup/demo_test/cpu.max
echo ""

# Set memory limit to 50MB
echo "3. Setting memory limit to 50MB..."
echo "52428800" > /sys/fs/cgroup/demo_test/memory.max
cat /sys/fs/cgroup/demo_test/memory.max
echo ""

# Start a test process
echo "4. Starting CPU-intensive process..."
yes > /dev/null &
PID=$!
echo "Process PID: $PID"
echo ""

# Move to cgroup
echo "5. Moving process to limited cgroup..."
echo $PID > /sys/fs/cgroup/demo_test/cgroup.procs
echo ""

# Monitor for 5 seconds
echo "6. Monitoring for 5 seconds..."
for i in {1..5}; do
    echo "Sample $i:"
    cat /sys/fs/cgroup/demo_test/cpu.stat | grep usage
    cat /sys/fs/cgroup/demo_test/memory.current
    sleep 1
done
echo ""

# Cleanup
echo "7. Cleanup..."
kill $PID
sleep 1
rmdir /sys/fs/cgroup/demo_test 2>/dev/null || echo "Cgroup will be removed when empty"

echo ""
echo "Demo complete!"
