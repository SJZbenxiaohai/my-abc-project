#!/bin/bash

# Test script for partition-aware FPGA mapping
# Output results to a log file

# Set up environment
export LD_LIBRARY_PATH=/home/sjz/C/Data/FPGA-Mapping/abc20250804/lib/kahypar/build/lib:$LD_LIBRARY_PATH

# Create log file with timestamp
LOG_FILE="test_results_$(date +%Y%m%d_%H%M%S).log"

echo "Starting FPGA mapping tests..."
echo "Results will be saved to: $LOG_FILE"
echo ""

# Write header to log file
{
    echo "====================================="
    echo "FPGA Mapping Test Results"
    echo "====================================="
    echo "Test Date: $(date)"
    echo "LUT Size: 6"
    echo ""
} > "$LOG_FILE"

# Arrays to store results
declare -a FILES
declare -a STD_NODES
declare -a STD_LEVELS
declare -a PART_NODES
declare -a PART_LEVELS

# Counter for successful tests
SUCCESS_COUNT=0
BETTER_COUNT=0
FILE_INDEX=0

# Function to test a single file
test_file() {
    local aig_file=$1
    local file_name=$(basename "$aig_file")
    
    echo "Testing: $file_name"
    
    {
        echo "-------------------------------------"
        echo "File: $aig_file"
        echo "-------------------------------------"
    } >> "$LOG_FILE"
    
    # Test standard mapping
    echo "  Running standard mapping..."
    {
        echo ""
        echo "Standard mapping (if -K 6):"
    } >> "$LOG_FILE"
    
    STD_OUTPUT=$(echo "read $aig_file; if -K 6; ps; cec" | ./abc 2>&1)
    echo "$STD_OUTPUT" | tail -20 >> "$LOG_FILE"
    
    # Extract statistics
    STD_STATS=$(echo "$STD_OUTPUT" | grep "nd =")
    if [ -n "$STD_STATS" ]; then
        STD_ND=$(echo "$STD_STATS" | sed -n 's/.*nd = \([0-9]*\).*/\1/p')
        STD_LEV=$(echo "$STD_STATS" | sed -n 's/.*lev = \([0-9]*\).*/\1/p')
        
        # Test partition-aware mapping
        echo "  Running partition-aware mapping..."
        {
            echo ""
            echo "Partition-aware mapping (if -K 6 -H):"
        } >> "$LOG_FILE"
        
        PART_OUTPUT=$(echo "read $aig_file; if -K 6 -H; ps; cec" | ./abc 2>&1)
        echo "$PART_OUTPUT" | tail -20 >> "$LOG_FILE"
        
        # Extract statistics
        PART_STATS=$(echo "$PART_OUTPUT" | grep "nd =")
        if [ -n "$PART_STATS" ]; then
            PART_ND=$(echo "$PART_STATS" | sed -n 's/.*nd = \([0-9]*\).*/\1/p')
            PART_LEV=$(echo "$PART_STATS" | sed -n 's/.*lev = \([0-9]*\).*/\1/p')
            
            # Store results
            FILES[$FILE_INDEX]="$file_name"
            STD_NODES[$FILE_INDEX]=$STD_ND
            STD_LEVELS[$FILE_INDEX]=$STD_LEV
            PART_NODES[$FILE_INDEX]=$PART_ND
            PART_LEVELS[$FILE_INDEX]=$PART_LEV
            
            # Compare results
            {
                echo ""
                echo "Results:"
                echo "  Standard:  nodes=$STD_ND, levels=$STD_LEV"
                echo "  Partition: nodes=$PART_ND, levels=$PART_LEV"
                
                if [ "$PART_ND" -lt "$STD_ND" ] || [ "$PART_LEV" -lt "$STD_LEV" ]; then
                    echo "  ** PARTITION IS BETTER **"
                    ((BETTER_COUNT++))
                fi
                echo ""
            } >> "$LOG_FILE"
            
            echo "  Standard:  nodes=$STD_ND, levels=$STD_LEV"
            echo "  Partition: nodes=$PART_ND, levels=$PART_LEV"
            
            if [ "$PART_ND" -lt "$STD_ND" ] || [ "$PART_LEV" -lt "$STD_LEV" ]; then
                echo "  ✓ PARTITION BETTER"
            fi
            
            ((SUCCESS_COUNT++))
            ((FILE_INDEX++))
        else
            echo "  ✗ Failed: Partition mapping failed" | tee -a "$LOG_FILE"
        fi
    else
        echo "  ✗ Failed: Standard mapping failed" | tee -a "$LOG_FILE"
    fi
    
    echo ""
}

# Test arithmetic benchmarks
echo "Testing arithmetic benchmarks..."
echo ""

for file in /home/sjz/C/Data/FPGA-Mapping/benchmarks/arithmetic/*.aig; do
    if [ -f "$file" ]; then
        test_file "$file"
    fi
done

# Test random_control benchmarks
echo "Testing random_control benchmarks..."
echo ""

for file in /home/sjz/C/Data/FPGA-Mapping/benchmarks/random_control/*.aig; do
    if [ -f "$file" ]; then
        test_file "$file"
    fi
done

# Write summary
{
    echo "====================================="
    echo "SUMMARY"
    echo "====================================="
    echo "Total files tested: $FILE_INDEX"
    echo "Successful tests: $SUCCESS_COUNT"
    echo "Files where partition is better: $BETTER_COUNT"
    echo ""
    
    if [ $BETTER_COUNT -gt 0 ]; then
        echo "Files with improvements:"
        for ((i=0; i<$FILE_INDEX; i++)); do
            NODES_DIFF=$((PART_NODES[i] - STD_NODES[i]))
            LEVELS_DIFF=$((PART_LEVELS[i] - STD_LEVELS[i]))
            
            if [ $NODES_DIFF -lt 0 ] || [ $LEVELS_DIFF -lt 0 ]; then
                echo "  ${FILES[i]}:"
                if [ $NODES_DIFF -lt 0 ]; then
                    echo "    Nodes reduced by ${NODES_DIFF#-} ($((100 * NODES_DIFF / STD_NODES[i]))%)"
                fi
                if [ $LEVELS_DIFF -lt 0 ]; then
                    echo "    Levels reduced by ${LEVELS_DIFF#-} ($((100 * LEVELS_DIFF / STD_LEVELS[i]))%)"
                fi
            fi
        done
    fi
} >> "$LOG_FILE"

# Print summary to console
echo "====================================="
echo "SUMMARY"
echo "====================================="
echo "Total files tested: $FILE_INDEX"
echo "Successful tests: $SUCCESS_COUNT"
echo "Files where partition is better: $BETTER_COUNT"
echo ""
echo "Full results saved to: $LOG_FILE"
echo "View with: cat $LOG_FILE"
