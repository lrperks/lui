/* Embeds src/lui.lua in the lui binary as lui_runtime / lui_runtime_size. */
    .section .rodata
    .global lui_runtime
    .global lui_runtime_size
lui_runtime:
    .incbin "src/lui.lua"
lui_runtime_end:
    .balign 8
lui_runtime_size:
    .quad lui_runtime_end - lui_runtime
    .section .note.GNU-stack,"",@progbits
