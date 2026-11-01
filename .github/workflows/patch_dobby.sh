#!/bin/bash
ASM=app/third_party/dobby/source/TrampolineBridge/ClosureTrampolineBridge/arm64/closure_bridge_arm64.asm
# fix Mach-O @PAGE/@PAGEOFF to ELF :lo12: syntax
sed -i 's/common_closure_bridge_handler@PAGE\b/common_closure_bridge_handler/g' $ASM
sed -i 's/common_closure_bridge_handler@PAGEOFF/:lo12:common_closure_bridge_handler/g' $ASM
sed -i 's/common_closure_bridge_handler_end@PAGE\b/common_closure_bridge_handler_end/g' $ASM
sed -i 's/common_closure_bridge_handler_end@PAGEOFF/:lo12:common_closure_bridge_handler_end/g' $ASM
echo "Dobby ASM patched"
