#!/bin/bash

if [ $# -eq 0 ]; then
    echo "Usage: $0 <csv_file>"
    exit 1
fi

CSV_FILE="$1"

if [ ! -f "$CSV_FILE" ]; then
    echo "Error: File not found: $CSV_FILE"
    exit 1
fi

echo "=== CSV Data Summary ==="
echo ""
echo "Total Samples: $(tail -n +2 "$CSV_FILE" | wc -l)"
echo ""

echo "CPU Usage (%):"
tail -n +2 "$CSV_FILE" | awk -F',' '{sum+=$5; count++} END {print "  Average: " sum/count}'
tail -n +2 "$CSV_FILE" | awk -F',' '{if(max==""){max=$5}; if($5>max){max=$5}} END {print "  Maximum: " max}'

echo ""
echo "Memory RSS (MB):"
tail -n +2 "$CSV_FILE" | awk -F',' '{rss=$8/(1024*1024); sum+=rss; count++} END {print "  Average: " sum/count " MB"}'
tail -n +2 "$CSV_FILE" | awk -F',' '{rss=$8/(1024*1024); if(max==""){max=rss}; if(rss>max){max=rss}} END {print "  Maximum: " max " MB"}'

echo ""
echo "Recent Samples (last 5):"
tail -n 5 "$CSV_FILE" | column -t -s','
