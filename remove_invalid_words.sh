#!/bin/bash

input_file=$1
temp_file=$(mktemp)

awk 'length($0) == 6' "$input_file" > "$temp_file"
mv "$temp_file" "$input_file"
