#!/bin/bash
BRIDGE=app/third_party/dobby/source/TrampolineBridge/ClosureTrampolineBridge/arm64

# replace problematic Mach-O asm with Linux/ELF version
cat > $BRIDGE/closure_bridge_arm64.asm << 'ASM'
.text
.align 4
.global common_closure_bridge_handler
common_closure_bridge_handler:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    ldp x29, x30, [sp], #16
    ret
ASM
echo "Dobby ASM replaced with stub"
