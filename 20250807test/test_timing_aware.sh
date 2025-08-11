#!/bin/bash

# Test script for timing-aware hypergraph partitioning
# Compares three modes:
# 1. Standard mapping (no partitioning)
# 2. Regular hypergraph partitioning
# 3. Timing-aware hypergraph partitioning

# Set paths
ABC_PATH="./abc"
BENCHMARK_DIR="/home/sjz/C/Data/FPGA-Mapping/benchmarks"
RESULT_FILE="timing_aware_results.txt"

# Export library path for KaHyPar
export LD_LIBRARY_PATH=/home/sjz/C/Data/FPGA-Mapping/abc20250804/lib/kahypar/build/lib:$LD_LIBRARY_PATH

# Clear result file
echo "Timing-Aware Hypergraph Partitioning Test Results" > $RESULT_FILE
echo "=================================================" >> $RESULT_FILE
echo "Date: $(date)" >> $RESULT_FILE
echo "" >> $RESULT_FILE

# Function to test a single file
test_file() {
    local file=$1
    local filename=$(basename $file)
    
    echo "Testing: $filename" | tee -a $RESULT_FILE
    echo "----------------------------------------" >> $RESULT_FILE
    
    # Test 1: Standard mapping (no partitioning)
    echo "Mode 1: Standard mapping (no -H)" >> $RESULT_FILE
    result1=$($ABC_PATH -c "read $file; if -K 6; ps" 2>&1 | grep -E "nd =|delay =")
    echo "$result1" >> $RESULT_FILE
    echo "" >> $RESULT_FILE
    
    # Test 2: Regular hypergraph partitioning
    echo "Mode 2: Regular hypergraph partitioning (-H or -H 0)" >> $RESULT_FILE
    result2=$($ABC_PATH -c "read $file; if -K 6 -H 0; ps" 2>&1 | grep -E "nd =|delay =")
    echo "$result2" >> $RESULT_FILE
    echo "" >> $RESULT_FILE
    
    # Test 3: Timing-aware hypergraph partitioning
    echo "Mode 3: Timing-aware hypergraph partitioning (-H 1)" >> $RESULT_FILE
    result3=$($ABC_PATH -c "read $file; if -K 6 -H 1; ps" 2>&1 | grep -E "nd =|delay =")
    echo "$result3" >> $RESULT_FILE
    echo "" >> $RESULT_FILE
    
    # Extract and compare values
    nodes1=$(echo "$result1" | grep -oP 'nd = \K[0-9]+' | head -1)
    delay1=$(echo "$result1" | grep -oP 'delay = \K[0-9.]+' | head -1)
    
    nodes2=$(echo "$result2" | grep -oP 'nd = \K[0-9]+' | head -1)
    delay2=$(echo "$result2" | grep -oP 'delay = \K[0-9.]+' | head -1)
    
    nodes3=$(echo "$result3" | grep -oP 'nd = \K[0-9]+' | head -1)
    delay3=$(echo "$result3" | grep -oP 'delay = \K[0-9.]+' | head -1)
    
    # Calculate improvements
    if [ -n "$nodes1" ] && [ -n "$nodes2" ] && [ -n "$nodes3" ]; then
        echo "Comparison:" >> $RESULT_FILE
        echo "  Standard:     nodes=$nodes1, delay=$delay1" >> $RESULT_FILE
        echo "  Regular Part: nodes=$nodes2, delay=$delay2" >> $RESULT_FILE
        echo "  Timing-Aware: nodes=$nodes3, delay=$delay3" >> $RESULT_FILE
        
        # Calculate percentage changes
        if [ "$nodes1" != "0" ]; then
            node_change2=$(echo "scale=2; ($nodes2 - $nodes1) * 100 / $nodes1" | bc)
            node_change3=$(echo "scale=2; ($nodes3 - $nodes1) * 100 / $nodes1" | bc)
            echo "  Node change (Regular): ${node_change2}%" >> $RESULT_FILE
            echo "  Node change (Timing):  ${node_change3}%" >> $RESULT_FILE
        fi
        
        if [ -n "$delay1" ] && [ "$delay1" != "0" ]; then
            delay_change2=$(echo "scale=2; ($delay2 - $delay1) * 100 / $delay1" | bc 2>/dev/null || echo "N/A")
            delay_change3=$(echo "scale=2; ($delay3 - $delay1) * 100 / $delay1" | bc 2>/dev/null || echo "N/A")
            echo "  Delay change (Regular): ${delay_change2}%" >> $RESULT_FILE
            echo "  Delay change (Timing):  ${delay_change3}%" >> $RESULT_FILE
        fi
        
        # Check if timing-aware is better than regular
        if [ -n "$delay2" ] && [ -n "$delay3" ]; then
            if (( $(echo "$delay3 < $delay2" | bc -l) )); then
                echo "  ✓ Timing-aware has better delay than regular partitioning" >> $RESULT_FILE
            elif (( $(echo "$delay3 == $delay2" | bc -l) )); then
                echo "  = Timing-aware has same delay as regular partitioning" >> $RESULT_FILE
            else
                echo "  ✗ Timing-aware has worse delay than regular partitioning" >> $RESULT_FILE
            fi
        fi
    fi
    
    echo "" >> $RESULT_FILE
}

# Test a few representative benchmarks
echo "Testing selected benchmarks..."
echo ""

# Test arithmetic circuits (usually have clear critical paths)
test_file "$BENCHMARK_DIR/arithmetic/adder.aig"
test_file "$BENCHMARK_DIR/arithmetic/bar.aig"
test_file "$BENCHMARK_DIR/arithmetic/max.aig"

# Test control circuits
test_file "$BENCHMARK_DIR/random_control/arbiter.aig"
test_file "$BENCHMARK_DIR/random_control/cavlc.aig"
test_file "$BENCHMARK_DIR/random_control/ctrl.aig"

# Test the i10 circuit we've been using
if [ -f "i10.aig" ]; then
    test_file "i10.aig"
fi

echo ""
echo "========================================" >> $RESULT_FILE
echo "Summary Statistics" >> $RESULT_FILE
echo "========================================" >> $RESULT_FILE

# Count improvements
better_delay=$(grep -c "✓ Timing-aware has better delay" $RESULT_FILE)
same_delay=$(grep -c "= Timing-aware has same delay" $RESULT_FILE)
worse_delay=$(grep -c "✗ Timing-aware has worse delay" $RESULT_FILE)

echo "Timing-aware vs Regular partitioning:" >> $RESULT_FILE
echo "  Better delay: $better_delay circuits" >> $RESULT_FILE
echo "  Same delay:   $same_delay circuits" >> $RESULT_FILE
echo "  Worse delay:  $worse_delay circuits" >> $RESULT_FILE

echo ""
echo "Test completed. Results saved to $RESULT_FILE"
cat $RESULT_FILE