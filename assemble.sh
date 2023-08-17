#!/usr/bin/bash

# This script will be used by pocl

# echo "converting object file:"
# echo $1
script_dir=$(cd "$(dirname "$0")" && pwd)
export PATH=$script_dir/install/bin:$PATH
if [[ -f $1.vmem ]];then
  rm $1.vmem
fi
llvm-objdump -d --mattr=+v,+zfinx $1.riscv > $1.dump

# Add a flag to mark .text section
found_text_section=0

# Traverse line in dump file
while IFS= read -r line; do
    # Judge if meet .text section
    if [[ $line == *"Disassembly of section .text:"* ]]; then
        found_text_section=1
        continue
    fi

    # Ignore other non .text section
    if [[ $line == *"Disassembly of section"* ]]; then
        found_text_section=0
    fi

    if [[ $found_text_section -eq 1 ]]; then
        # Only deal with the line starting with hex data
        if [[ $line =~ ^[0-9a-fA-F]+: ]]; then
            # Print column 2-5 in reverse order
            hex_data=$(awk 'function is_hex(s) { return match(s, /^[0-9a-fA-F]+$/) } { for (i = 5; i >= 2; i--) { if (is_hex($i)) printf "%s", $i } print "" }' <<< "$line")
            echo "$hex_data" >> $1.vmem
        fi
    fi
done < $1.dump

# echo "finish converting to vmem file!"
