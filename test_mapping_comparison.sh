#!/bin/bash

# Test script for comparing FPGA mapping results with different -H options
# Tests: 
#   - No -H: Standard mapping without hypergraph partitioning
#   - -H 0: Hypergraph partitioning WITHOUT critical path partitioning
#   - -H 1: Hypergraph partitioning WITH critical path partitioning

# Configuration
ABC_EXEC="./abc"
BENCHMARKS_DIR="/home/sjz/C/Data/FPGA-Mapping/benchmarks"
RESULT_BASE_DIR="result"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
K_VALUE=6

# Export library path for KaHyPar
export LD_LIBRARY_PATH=/home/sjz/C/Data/FPGA-Mapping/abc20250804/lib/kahypar/build/lib:$LD_LIBRARY_PATH

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Create result directory with timestamp
create_result_dir() {
    local result_dir="$RESULT_BASE_DIR/$TIMESTAMP"
    mkdir -p "$result_dir"
    echo "$result_dir"
}

# Run ABC mapping and extract statistics
run_mapping() {
    local benchmark_file=$1
    local mapping_options=$2
    local temp_output=$(mktemp)
    
    # Run ABC command
    if [ -z "$mapping_options" ]; then
        # Standard mapping
        $ABC_EXEC -c "read $benchmark_file; strash; balance; if -K $K_VALUE; ps; cec" > "$temp_output" 2>&1
    else
        # Hypergraph partitioning
        $ABC_EXEC -c "read $benchmark_file; strash; balance; if -K $K_VALUE $mapping_options; ps; cec" > "$temp_output" 2>&1
    fi
    
    # Read the output
    local output=$(cat "$temp_output")
    
    # Clean up temp files
    rm -f "$temp_output"
    
    # Extract statistics
    local stats_line=$(echo "$output" | grep "top" | tail -1)
    local nodes=$(echo "$stats_line" | grep -oE "nd\s*=\s*[0-9]+" | grep -oE "[0-9]+")
    local edges=$(echo "$stats_line" | grep -oE "edge\s*=\s*[0-9]+" | grep -oE "[0-9]+")
    local levels=$(echo "$stats_line" | grep -oE "lev\s*=\s*[0-9]+" | grep -oE "[0-9]+")
    
    # Check CEC result
    local cec_result="UNKNOWN"
    if echo "$output" | grep -q "Networks are equivalent"; then
        cec_result="PASS"
    elif echo "$output" | grep -q "Networks are NOT equivalent"; then
        cec_result="FAIL"
    fi
    
    # For hypergraph partitioning, extract partition info
    local cut_edges="-"
    if [ ! -z "$mapping_options" ]; then
        # Extract cut edges from KaHyPar output
        cut_edges=$(echo "$output" | grep "Hyperedge Cut" | grep -oE "= [0-9]+" | grep -oE "[0-9]+")
        [ -z "$cut_edges" ] && cut_edges="-"
    fi
    
    # Return results as a string
    echo "${nodes:-0}|${edges:-0}|${levels:-0}|${cut_edges}|${cec_result}"
}

# Process benchmarks and save results
process_benchmarks() {
    local log_file=$1
    local category=$2
    local bench_dir="$BENCHMARKS_DIR/$category"
    
    echo "" | tee -a "$log_file"
    echo "================================================================" | tee -a "$log_file"
    echo "Category: $category" | tee -a "$log_file"
    echo "================================================================" | tee -a "$log_file"
    echo "" | tee -a "$log_file"
    
    # Statistics for this category
    local cat_total=0
    local cat_h0_improved=0
    local cat_h1_improved=0
    local cat_h0_worse=0
    local cat_h1_worse=0
    
    # Process each benchmark
    for benchmark in "$bench_dir"/*.blif; do
        if [ ! -f "$benchmark" ]; then
            continue
        fi
        
        local bench_name=$(basename "$benchmark" .blif)
        echo -e "${GREEN}Processing: $category/$bench_name${NC}" | tee -a "$log_file"
        
        # Run standard mapping
        echo -n "  Standard mapping..." | tee -a "$log_file"
        local std_result=$(run_mapping "$benchmark" "")
        IFS='|' read -r std_nodes std_edges std_levels std_cuts std_cec <<< "$std_result"
        echo " done (nodes=$std_nodes, lev=$std_levels)" | tee -a "$log_file"
        
        # Run with -H 0
        echo -n "  Hypergraph (-H 0)..." | tee -a "$log_file"
        local h0_result=$(run_mapping "$benchmark" "-H 0")
        IFS='|' read -r h0_nodes h0_edges h0_levels h0_cuts h0_cec <<< "$h0_result"
        echo " done (nodes=$h0_nodes, lev=$h0_levels)" | tee -a "$log_file"
        
        # Run with -H 1
        echo -n "  Hypergraph (-H 1)..." | tee -a "$log_file"
        local h1_result=$(run_mapping "$benchmark" "-H 1")
        IFS='|' read -r h1_nodes h1_edges h1_levels h1_cuts h1_cec <<< "$h1_result"
        echo " done (nodes=$h1_nodes, lev=$h1_levels)" | tee -a "$log_file"
        
        # Determine overall CEC status
        local cec_status="PASS"
        if [ "$std_cec" = "FAIL" ] || [ "$h0_cec" = "FAIL" ] || [ "$h1_cec" = "FAIL" ]; then
            cec_status="FAIL"
        fi
        
        # Calculate and display improvements
        echo "  Results:" | tee -a "$log_file"
        echo "    Standard: nodes=$std_nodes, lev=$std_levels" | tee -a "$log_file"
        
        if [ "$std_nodes" -gt 0 ] 2>/dev/null && [ "$h0_nodes" -gt 0 ] 2>/dev/null; then
            local h0_node_pct=$(echo "scale=1; ($h0_nodes - $std_nodes) * 100 / $std_nodes" | bc)
            local h0_lev_diff=$((h0_levels - std_levels))
            echo "    -H 0:     nodes=$h0_nodes ($h0_node_pct%), lev=$h0_levels (${h0_lev_diff:+}$h0_lev_diff)" | tee -a "$log_file"
            
            # Update statistics
            if (( $(echo "$h0_node_pct < 0" | bc -l) )); then
                cat_h0_improved=$((cat_h0_improved + 1))
            elif (( $(echo "$h0_node_pct > 0" | bc -l) )); then
                cat_h0_worse=$((cat_h0_worse + 1))
            fi
        fi
        
        if [ "$std_nodes" -gt 0 ] 2>/dev/null && [ "$h1_nodes" -gt 0 ] 2>/dev/null; then
            local h1_node_pct=$(echo "scale=1; ($h1_nodes - $std_nodes) * 100 / $std_nodes" | bc)
            local h1_lev_diff=$((h1_levels - std_levels))
            echo "    -H 1:     nodes=$h1_nodes ($h1_node_pct%), lev=$h1_levels (${h1_lev_diff:+}$h1_lev_diff)" | tee -a "$log_file"
            
            # Update statistics
            if (( $(echo "$h1_node_pct < 0" | bc -l) )); then
                cat_h1_improved=$((cat_h1_improved + 1))
            elif (( $(echo "$h1_node_pct > 0" | bc -l) )); then
                cat_h1_worse=$((cat_h1_worse + 1))
            fi
        fi
        
        echo "    CEC: $cec_status" | tee -a "$log_file"
        
        if [ "$cec_status" = "FAIL" ]; then
            echo -e "    ${RED}WARNING: Functional equivalence check failed!${NC}" | tee -a "$log_file"
        fi
        
        cat_total=$((cat_total + 1))
        echo "" | tee -a "$log_file"
    done
    
    # Category summary
    echo "Category Summary for $category:" | tee -a "$log_file"
    echo "  Total benchmarks: $cat_total" | tee -a "$log_file"
    echo "  -H 0: $cat_h0_improved improved, $cat_h0_worse worse" | tee -a "$log_file"
    echo "  -H 1: $cat_h1_improved improved, $cat_h1_worse worse" | tee -a "$log_file"
    echo "" | tee -a "$log_file"
    
    # Return statistics
    echo "$cat_total|$cat_h0_improved|$cat_h1_improved|$cat_h0_worse|$cat_h1_worse"
}

# Calculate final summary
calculate_summary() {
    local log_file=$1
    local total_benchmarks=$2
    local h0_improved=$3
    local h1_improved=$4
    local h0_worse=$5
    local h1_worse=$6
    
    echo "" | tee -a "$log_file"
    echo "================================================================" | tee -a "$log_file"
    echo "FINAL SUMMARY" | tee -a "$log_file"
    echo "================================================================" | tee -a "$log_file"
    echo "" | tee -a "$log_file"
    
    if [ $total_benchmarks -gt 0 ]; then
        local h0_improved_pct=$((h0_improved * 100 / total_benchmarks))
        local h1_improved_pct=$((h1_improved * 100 / total_benchmarks))
        local h0_worse_pct=$((h0_worse * 100 / total_benchmarks))
        local h1_worse_pct=$((h1_worse * 100 / total_benchmarks))
        
        echo "Total benchmarks tested: $total_benchmarks" | tee -a "$log_file"
        echo "" | tee -a "$log_file"
        
        echo "Results for -H 0 (Hypergraph without critical path):" | tee -a "$log_file"
        echo "  Improved (fewer nodes): $h0_improved ($h0_improved_pct%)" | tee -a "$log_file"
        echo "  Worse (more nodes):     $h0_worse ($h0_worse_pct%)" | tee -a "$log_file"
        echo "" | tee -a "$log_file"
        
        echo "Results for -H 1 (Hypergraph with critical path):" | tee -a "$log_file"
        echo "  Improved (fewer nodes): $h1_improved ($h1_improved_pct%)" | tee -a "$log_file"
        echo "  Worse (more nodes):     $h1_worse ($h1_worse_pct%)" | tee -a "$log_file"
        echo "" | tee -a "$log_file"
        
        # Check CEC status
        local cec_check=$(grep "CEC: FAIL" "$log_file" | wc -l)
        if [ $cec_check -eq 0 ]; then
            echo -e "${GREEN}✓ All mappings are functionally equivalent to original${NC}" | tee -a "$log_file"
        else
            echo -e "${RED}✗ WARNING: $cec_check benchmarks failed equivalence check!${NC}" | tee -a "$log_file"
            echo "Failed benchmarks:" | tee -a "$log_file"
            grep -B4 "CEC: FAIL" "$log_file" | grep "Processing:" | sed 's/Processing: /  - /' | tee -a "$log_file"
        fi
    else
        echo "No valid benchmark results found!" | tee -a "$log_file"
    fi
    
    echo "" | tee -a "$log_file"
}

# Main execution
main() {
    echo -e "${YELLOW}=== FPGA Mapping Comparison Test ===${NC}"
    echo "Configuration:"
    echo "  K-value: $K_VALUE"
    echo "  Standard: No hypergraph partitioning"
    echo "  -H 0: Hypergraph partitioning WITHOUT critical path"
    echo "  -H 1: Hypergraph partitioning WITH critical path"
    echo ""
    
    # Check if ABC executable exists
    if [ ! -f "$ABC_EXEC" ]; then
        echo -e "${RED}Error: ABC executable not found at $ABC_EXEC${NC}"
        exit 1
    fi
    
    # Create result directory with timestamp
    RESULT_DIR=$(create_result_dir)
    LOG_FILE="$RESULT_DIR/comparison.log"
    echo "Results will be saved to: $RESULT_DIR/"
    echo ""
    
    # Write header to log file
    echo "FPGA Mapping Comparison Report" > "$LOG_FILE"
    echo "Generated: $(date)" >> "$LOG_FILE"
    echo "K-value: $K_VALUE" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
    echo "Configuration Details:" >> "$LOG_FILE"
    echo "  Standard: Traditional mapping without hypergraph partitioning" >> "$LOG_FILE"
    echo "  -H 0: Hypergraph partitioning WITHOUT critical path consideration" >> "$LOG_FILE"
    echo "  -H 1: Hypergraph partitioning WITH critical path consideration" >> "$LOG_FILE"
    
    # Initialize global statistics
    total_benchmarks=0
    total_h0_improved=0
    total_h1_improved=0
    total_h0_worse=0
    total_h1_worse=0
    
    # Process arithmetic benchmarks
    echo -e "${BLUE}Processing Arithmetic Benchmarks...${NC}"
    arith_stats=$(process_benchmarks "$LOG_FILE" "arithmetic")
    IFS='|' read -r arith_total arith_h0_imp arith_h1_imp arith_h0_worse arith_h1_worse <<< "$arith_stats"
    total_benchmarks=$((total_benchmarks + arith_total))
    total_h0_improved=$((total_h0_improved + arith_h0_imp))
    total_h1_improved=$((total_h1_improved + arith_h1_imp))
    total_h0_worse=$((total_h0_worse + arith_h0_worse))
    total_h1_worse=$((total_h1_worse + arith_h1_worse))
    
    # Process random_control benchmarks
    echo -e "${BLUE}Processing Random Control Benchmarks...${NC}"
    control_stats=$(process_benchmarks "$LOG_FILE" "random_control")
    IFS='|' read -r control_total control_h0_imp control_h1_imp control_h0_worse control_h1_worse <<< "$control_stats"
    total_benchmarks=$((total_benchmarks + control_total))
    total_h0_improved=$((total_h0_improved + control_h0_imp))
    total_h1_improved=$((total_h1_improved + control_h1_imp))
    total_h0_worse=$((total_h0_worse + control_h0_worse))
    total_h1_worse=$((total_h1_worse + control_h1_worse))
    
    # Calculate and display final summary
    calculate_summary "$LOG_FILE" $total_benchmarks $total_h0_improved $total_h1_improved $total_h0_worse $total_h1_worse
    
    echo -e "${GREEN}Testing completed!${NC}"
    echo "Results directory: $RESULT_DIR/"
    echo "Full log: $LOG_FILE"
}

# Run main function
main "$@"