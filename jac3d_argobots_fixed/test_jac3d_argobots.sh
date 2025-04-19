#!/bin/bash

# Compile the program with optimization
gcc -O3 -o jac3d jac3d.c abt_reduction.c -labt -lm -I ~/argobots-install/include -L ~/argobots-install/libabt.so
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# Create results directory
mkdir -p results

# Results file
RESULTS="results/benchmark_results.txt"
echo "Argobots Jacobi-3D Benchmark Results" > $RESULTS
echo "=========================" >> $RESULTS

# CSV header
echo "Xstreams,Threads,Time(s),Time(nanos),Status" > results/summary.csv

# Expected epsilon value for verification
EXPECTED_EPS=5.058044

# Counter for unsuccessful runs
UNSUCCESSFUL_COUNT=0


# Configuration arrays
XSTREAMS=(1 2 4 7 8 14)
THREADS=(1 2 4 7 8 14 28)
NUM_RUNS=3


# Run tests for each configuration
for xstreams in "${XSTREAMS[@]}"; do
    for threads in "${THREADS[@]}"; do
        echo -e "\nTesting configuration: Xstreams=$xstreams, Threads=$threads"
        echo -e "\nConfiguration: Xstreams=$xstreams, Threads=$threads" >> $RESULTS
        
        # Initialize variables for aggregation
        total_time=0
        total_time_nanos=0
        verification="SUCCESSFUL"
        
        for run in $(seq 1 $NUM_RUNS); do
            echo "  Run $run of $NUM_RUNS"
            
            # Run the program and capture output
            OUTPUT_FILE="results/jac3d_x${xstreams}_t${threads}_run${run}.txt"
            ./jac3d $xstreams $threads | tee $OUTPUT_FILE
            
            # Extract execution time and verification status
            TIME=$(grep "Time in seconds" $OUTPUT_FILE | awk '{print $NF}')
            TIME_NANOS=$(grep "Real time" $OUTPUT_FILE | awk '{print $NF}')
            VERIFICATION=$(grep "Verification" $OUTPUT_FILE | awk '{print $NF}')
            
            # Update aggregation variables
            total_time=$(echo "$total_time + $TIME" | bc)
            total_time_nanos=$(echo "$total_time_nanos + $TIME_NANOS" | bc)
            if [ "$VERIFICATION" = "UNSUCCESSFUL" ]; then
                verification="UNSUCCESSFUL"
            fi
        done
        
        # Calculate mean time
        mean_time=$(echo "$total_time / $NUM_RUNS" | bc -l)
        mean_time_nanos=$(echo "$total_time_nanos / $NUM_RUNS" | bc -l)
        
        # Save to results file
        echo "Mean Time: $mean_time seconds" >> $RESULTS
        echo "Mean Time Nanos: $mean_time_nanos nanoseconds" >> $RESULTS
        echo "Verification: $verification" >> $RESULTS
        echo "------------------------" >> $RESULTS
        
        # Save to CSV
        echo "$xstreams,$threads,$mean_time,$mean_time_nanos,$verification" >> results/summary.csv
    done
done

# Generate summary report
echo -e "\nTest Summary Report" | tee results/summary_report.txt
echo "===================" | tee -a results/summary_report.txt
echo "Unsuccessful tests: $UNSUCCESSFUL_COUNT" | tee -a results/summary_report.txt

if [ $UNSUCCESSFUL_COUNT -gt 0 ]; then
    echo -e "\nWARNING: Some tests were unsuccessful!" | tee -a results/summary_report.txt
    echo "See results/unsuccessful_runs.txt for details" | tee -a results/summary_report.txt
fi

# Display summary table
echo -e "\nDetailed Results:"
echo "------------------------------------------------------------------------------------------------"
echo "Xstreams | Threads  | Time(s)                 | Time(nanos)                     | Status"
echo "------------------------------------------------------------------------------------------------"
column -t -s ',' results/summary.csv | tail -n +2 | awk '{printf "%-8s | %-8s | %-9s | %-9s | %s\n", $1, $2, $3, $4, $5}'

echo -e "\nTesting completed. Full results saved in $RESULTS"
echo "Summary report saved in results/summary_report.txt"