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
    
    # Extract statistics - look for the line with network name
    local stats_line=$(echo "$output" | grep -E "^\S+\s+:" | tail -1)
    if [ -z "$stats_line" ]; then
        # Try alternative format
        stats_line=$(echo "$output" | grep "i/o" | tail -1)
    fi
    
    # Extract values more robustly
    local nodes=$(echo "$stats_line" | sed 's/.*nd = *\([0-9]*\).*/\1/')
    local levels=$(echo "$stats_line" | sed 's/.*lev = *\([0-9]*\).*/\1/')
    local edges=$(echo "$stats_line" | sed 's/.*edge = *\([0-9]*\).*/\1/')
    
    # Validate extracted values
    if ! [[ "$nodes" =~ ^[0-9]+$ ]]; then
        nodes="0"
    fi
    if ! [[ "$levels" =~ ^[0-9]+$ ]]; then
        levels="0"
    fi
    if ! [[ "$edges" =~ ^[0-9]+$ ]]; then
        edges="0"
    fi
    
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
    
    echo ""
    echo "================================================================"
    echo "Category: $category"
    echo "================================================================"
    echo ""
    
    echo "" >> "$log_file"
    echo "================================================================" >> "$log_file"
    echo "Category: $category" >> "$log_file"
    echo "================================================================" >> "$log_file"
    echo "" >> "$log_file"
    
    # Statistics for this category
    local cat_total=0
    local cat_h0_improved=0
    local cat_h1_improved=0
    local cat_h0_worse=0
    local cat_h1_worse=0
    
    # Process each benchmark
    echo "Looking for benchmarks in: $bench_dir/"
    
    # Check if directory exists
    if [ ! -d "$bench_dir" ]; then
        echo "Directory not found: $bench_dir"
        echo "Directory not found: $bench_dir" >> "$log_file"
        echo "$cat_total|$cat_h0_improved|$cat_h1_improved|$cat_h0_worse|$cat_h1_worse"
        return
    fi
    
    # Count files - support both .blif and .aig formats
    local blif_count=$(ls "$bench_dir"/*.blif 2>/dev/null | wc -l)
    local aig_count=$(ls "$bench_dir"/*.aig 2>/dev/null | wc -l)
    echo "Found $blif_count .blif files and $aig_count .aig files"
    
    if [ $blif_count -eq 0 ] && [ $aig_count -eq 0 ]; then
        echo "No benchmark files found in $bench_dir"
        echo "No benchmark files found in $bench_dir" >> "$log_file"
        echo "$cat_total|$cat_h0_improved|$cat_h1_improved|$cat_h0_worse|$cat_h1_worse"
        return
    fi
    
    # Process both .blif and .aig files
    for benchmark in "$bench_dir"/*.blif "$bench_dir"/*.aig; do
        [ ! -f "$benchmark" ] && continue
        
        local bench_name=$(basename "$benchmark" | sed 's/\.\(blif\|aig\)$//')
        echo -e "${GREEN}Processing: $category/$bench_name${NC}"
        echo "Processing: $category/$bench_name" >> "$log_file"
        
        # Run standard mapping
        echo -n "  Standard mapping..."
        echo -n "  Standard mapping..." >> "$log_file"
        local std_result=$(run_mapping "$benchmark" "")
        IFS='|' read -r std_nodes std_edges std_levels std_cuts std_cec <<< "$std_result"
        echo " done (nodes=$std_nodes, lev=$std_levels)"
        echo " done (nodes=$std_nodes, lev=$std_levels)" >> "$log_file"
        
        # Run with -H 0
        echo -n "  Hypergraph (-H 0)..."
        echo -n "  Hypergraph (-H 0)..." >> "$log_file"
        local h0_result=$(run_mapping "$benchmark" "-H 0")
        IFS='|' read -r h0_nodes h0_edges h0_levels h0_cuts h0_cec <<< "$h0_result"
        echo " done (nodes=$h0_nodes, lev=$h0_levels)"
        echo " done (nodes=$h0_nodes, lev=$h0_levels)" >> "$log_file"
        
        # Run with -H 1
        echo -n "  Hypergraph (-H 1)..."
        echo -n "  Hypergraph (-H 1)..." >> "$log_file"
        local h1_result=$(run_mapping "$benchmark" "-H 1")
        IFS='|' read -r h1_nodes h1_edges h1_levels h1_cuts h1_cec <<< "$h1_result"
        echo " done (nodes=$h1_nodes, lev=$h1_levels)"
        echo " done (nodes=$h1_nodes, lev=$h1_levels)" >> "$log_file"
        
        # Determine overall CEC status
        local cec_status="PASS"
        if [ "$std_cec" = "FAIL" ] || [ "$h0_cec" = "FAIL" ] || [ "$h1_cec" = "FAIL" ]; then
            cec_status="FAIL"
        fi
        
        # Calculate and display improvements
        echo "  Results:"
        echo "  Results:" >> "$log_file"
        echo "    Standard: nodes=$std_nodes, lev=$std_levels"
        echo "    Standard: nodes=$std_nodes, lev=$std_levels" >> "$log_file"
        
        if [ "$std_nodes" -gt 0 ] 2>/dev/null && [ "$h0_nodes" -gt 0 ] 2>/dev/null; then
            local h0_node_pct=$(echo "scale=1; ($h0_nodes - $std_nodes) * 100 / $std_nodes" | bc)
            local h0_lev_diff=$((h0_levels - std_levels))
            echo "    -H 0:     nodes=$h0_nodes ($h0_node_pct%), lev=$h0_levels (${h0_lev_diff:+}$h0_lev_diff)"
            echo "    -H 0:     nodes=$h0_nodes ($h0_node_pct%), lev=$h0_levels (${h0_lev_diff:+}$h0_lev_diff)" >> "$log_file"
            
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
            echo "    -H 1:     nodes=$h1_nodes ($h1_node_pct%), lev=$h1_levels (${h1_lev_diff:+}$h1_lev_diff)"
            echo "    -H 1:     nodes=$h1_nodes ($h1_node_pct%), lev=$h1_levels (${h1_lev_diff:+}$h1_lev_diff)" >> "$log_file"
            
            # Update statistics
            if (( $(echo "$h1_node_pct < 0" | bc -l) )); then
                cat_h1_improved=$((cat_h1_improved + 1))
            elif (( $(echo "$h1_node_pct > 0" | bc -l) )); then
                cat_h1_worse=$((cat_h1_worse + 1))
            fi
        fi
        
        echo "    CEC: $cec_status"
        echo "    CEC: $cec_status" >> "$log_file"
        
        if [ "$cec_status" = "FAIL" ]; then
            echo -e "    ${RED}WARNING: Functional equivalence check failed!${NC}"
            echo "    WARNING: Functional equivalence check failed!" >> "$log_file"
        fi
        
        cat_total=$((cat_total + 1))
        echo ""
        echo "" >> "$log_file"
    done
    
    # Category summary
    echo "Category Summary for $category:"
    echo "Category Summary for $category:" >> "$log_file"
    echo "  Total benchmarks: $cat_total"
    echo "  Total benchmarks: $cat_total" >> "$log_file"
    echo "  -H 0: $cat_h0_improved improved, $cat_h0_worse worse"
    echo "  -H 0: $cat_h0_improved improved, $cat_h0_worse worse" >> "$log_file"
    echo "  -H 1: $cat_h1_improved improved, $cat_h1_worse worse"
    echo "  -H 1: $cat_h1_improved improved, $cat_h1_worse worse" >> "$log_file"
    echo ""
    echo "" >> "$log_file"
    
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
    
    echo ""
    echo "================================================================"
    echo "FINAL SUMMARY"
    echo "================================================================"
    echo ""
    
    echo "" >> "$log_file"
    echo "================================================================" >> "$log_file"
    echo "FINAL SUMMARY" >> "$log_file"
    echo "================================================================" >> "$log_file"
    echo "" >> "$log_file"
    
    if [ $total_benchmarks -gt 0 ]; then
        local h0_improved_pct=$((h0_improved * 100 / total_benchmarks))
        local h1_improved_pct=$((h1_improved * 100 / total_benchmarks))
        local h0_worse_pct=$((h0_worse * 100 / total_benchmarks))
        local h1_worse_pct=$((h1_worse * 100 / total_benchmarks))
        
        echo "Total benchmarks tested: $total_benchmarks"
        echo "Total benchmarks tested: $total_benchmarks" >> "$log_file"
        echo ""
        echo "" >> "$log_file"
        
        echo "Results for -H 0 (Hypergraph without critical path):"
        echo "Results for -H 0 (Hypergraph without critical path):" >> "$log_file"
        echo "  Improved (fewer nodes): $h0_improved ($h0_improved_pct%)"
        echo "  Improved (fewer nodes): $h0_improved ($h0_improved_pct%)" >> "$log_file"
        echo "  Worse (more nodes):     $h0_worse ($h0_worse_pct%)"
        echo "  Worse (more nodes):     $h0_worse ($h0_worse_pct%)" >> "$log_file"
        echo ""
        echo "" >> "$log_file"
        
        echo "Results for -H 1 (Hypergraph with critical path):"
        echo "Results for -H 1 (Hypergraph with critical path):" >> "$log_file"
        echo "  Improved (fewer nodes): $h1_improved ($h1_improved_pct%)"
        echo "  Improved (fewer nodes): $h1_improved ($h1_improved_pct%)" >> "$log_file"
        echo "  Worse (more nodes):     $h1_worse ($h1_worse_pct%)"
        echo "  Worse (more nodes):     $h1_worse ($h1_worse_pct%)" >> "$log_file"
        echo ""
        echo "" >> "$log_file"
        
        # Check CEC status
        local cec_check=$(grep "CEC: FAIL" "$log_file" | wc -l)
        if [ $cec_check -eq 0 ]; then
            echo -e "${GREEN}✓ All mappings are functionally equivalent to original${NC}"
            echo "✓ All mappings are functionally equivalent to original" >> "$log_file"
        else
            echo -e "${RED}✗ WARNING: $cec_check benchmarks failed equivalence check!${NC}"
            echo "✗ WARNING: $cec_check benchmarks failed equivalence check!" >> "$log_file"
            echo "Failed benchmarks:"
            echo "Failed benchmarks:" >> "$log_file"
            grep -B4 "CEC: FAIL" "$log_file" | grep "Processing:" | sed 's/Processing: /  - /'
            grep -B4 "CEC: FAIL" "$log_file" | grep "Processing:" | sed 's/Processing: /  - /' >> "$log_file"
        fi
    else
        echo "No valid benchmark results found!"
        echo "No valid benchmark results found!" >> "$log_file"
    fi
    
    echo ""
    echo "" >> "$log_file"
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
    
    # Directly process without function call for debugging
    bench_dir="$BENCHMARKS_DIR/arithmetic"
    echo "Looking for files in: $bench_dir"
    
    if [ ! -d "$bench_dir" ]; then
        echo "Directory not found: $bench_dir"
    else
        for benchmark in "$bench_dir"/*.aig "$bench_dir"/*.blif; do
            [ ! -f "$benchmark" ] && continue
            
            bench_name=$(basename "$benchmark" | sed 's/\.\(blif\|aig\)$//')
            echo -e "${GREEN}Processing: $bench_name${NC}"
            echo "Processing: arithmetic/$bench_name" >> "$LOG_FILE"
            
            # Run standard mapping
            echo -n "  Standard mapping..."
            std_result=$(run_mapping "$benchmark" "")
            IFS='|' read -r std_nodes std_edges std_levels std_cuts std_cec <<< "$std_result"
            echo " done (nodes=$std_nodes, lev=$std_levels)"
            echo "  Standard: nodes=$std_nodes, lev=$std_levels, cec=$std_cec" >> "$LOG_FILE"
            
            # Run with -H 0
            echo -n "  Hypergraph (-H 0)..."
            h0_result=$(run_mapping "$benchmark" "-H 0")
            IFS='|' read -r h0_nodes h0_edges h0_levels h0_cuts h0_cec <<< "$h0_result"
            echo " done (nodes=$h0_nodes, lev=$h0_levels)"
            echo "  -H 0: nodes=$h0_nodes, lev=$h0_levels, cec=$h0_cec" >> "$LOG_FILE"
            
            # Run with -H 1
            echo -n "  Hypergraph (-H 1)..."
            h1_result=$(run_mapping "$benchmark" "-H 1")
            IFS='|' read -r h1_nodes h1_edges h1_levels h1_cuts h1_cec <<< "$h1_result"
            echo " done (nodes=$h1_nodes, lev=$h1_levels)"
            echo "  -H 1: nodes=$h1_nodes, lev=$h1_levels, cec=$h1_cec" >> "$LOG_FILE"
            
            echo ""
            echo "" >> "$LOG_FILE"
            total_benchmarks=$((total_benchmarks + 1))
        done
    fi
    
    # Process random_control benchmarks
    echo -e "${BLUE}Processing Random Control Benchmarks...${NC}"
    
    bench_dir="$BENCHMARKS_DIR/random_control"
    echo "Looking for files in: $bench_dir"
    
    if [ ! -d "$bench_dir" ]; then
        echo "Directory not found: $bench_dir"
    else
        for benchmark in "$bench_dir"/*.aig "$bench_dir"/*.blif; do
            [ ! -f "$benchmark" ] && continue
            
            bench_name=$(basename "$benchmark" | sed 's/\.\(blif\|aig\)$//')
            echo -e "${GREEN}Processing: $bench_name${NC}"
            echo "Processing: random_control/$bench_name" >> "$LOG_FILE"
            
            # Run standard mapping
            echo -n "  Standard mapping..."
            std_result=$(run_mapping "$benchmark" "")
            IFS='|' read -r std_nodes std_edges std_levels std_cuts std_cec <<< "$std_result"
            echo " done (nodes=$std_nodes, lev=$std_levels)"
            echo "  Standard: nodes=$std_nodes, lev=$std_levels, cec=$std_cec" >> "$LOG_FILE"
            
            # Run with -H 0
            echo -n "  Hypergraph (-H 0)..."
            h0_result=$(run_mapping "$benchmark" "-H 0")
            IFS='|' read -r h0_nodes h0_edges h0_levels h0_cuts h0_cec <<< "$h0_result"
            echo " done (nodes=$h0_nodes, lev=$h0_levels)"
            echo "  -H 0: nodes=$h0_nodes, lev=$h0_levels, cec=$h0_cec" >> "$LOG_FILE"
            
            # Run with -H 1
            echo -n "  Hypergraph (-H 1)..."
            h1_result=$(run_mapping "$benchmark" "-H 1")
            IFS='|' read -r h1_nodes h1_edges h1_levels h1_cuts h1_cec <<< "$h1_result"
            echo " done (nodes=$h1_nodes, lev=$h1_levels)"
            echo "  -H 1: nodes=$h1_nodes, lev=$h1_levels, cec=$h1_cec" >> "$LOG_FILE"
            
            echo ""
            echo "" >> "$LOG_FILE"
            total_benchmarks=$((total_benchmarks + 1))
        done
    fi
    
    # Calculate and display final summary
    calculate_summary "$LOG_FILE" $total_benchmarks $total_h0_improved $total_h1_improved $total_h0_worse $total_h1_worse
    
    echo -e "${GREEN}Testing completed!${NC}"
    echo "Results directory: $RESULT_DIR/"
    echo "Full log: $LOG_FILE"
}

# Run main function
main "$@"