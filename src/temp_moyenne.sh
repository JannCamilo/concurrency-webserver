#!/bin/bash

# Variables
command="./wclient 127.0.0.1 8111 /index.html 5"
num_runs=100
total_time=0

# Loop to execute the command 100 times
for i in $(seq 1 $num_runs)
do
  # Execute the command and capture the last line of output
  result=$(eval "$command" | tail -n 1)
  
  # Extract the numeric value of time using regex
  time=$(echo "$result" | grep -oE '[0-9]+\.[0-9]+')

  # Check if the time was extracted successfully
  if [[ -n $time ]]; then
    total_time=$(echo "$total_time + $time" | bc)
  else
    echo "Failed to extract time on iteration $i"
  fi

done

# Calculate the average time
average_time=$(echo "scale=2; $total_time / $num_runs" | bc)

# Output the average time
echo "Average time over $num_runs runs: $average_time ms"

