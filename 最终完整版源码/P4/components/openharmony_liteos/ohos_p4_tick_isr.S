    .section .iram1, "ax", @progbits

    .extern _panic_handler
    .extern _interrupt_handler
    .extern ohos_p4_tick_body

    .global ohos_p4_mtvt_table_vec20
    .type ohos_p4_mtvt_table_vec20, @object
    .balign 0x40
ohos_p4_mtvt_table_vec20:
    .word _panic_handler        /* 0 */
    .word _panic_handler        /* 1 */
    .word _panic_handler        /* 2 */
    .word _panic_handler        /* 3 */
    .word _panic_handler        /* 4 */
    .word _panic_handler        /* 5 */
    .word _panic_handler        /* 6 */
    .word _panic_handler        /* 7 */
    .word _panic_handler        /* 8 */
    .word _panic_handler        /* 9 */
    .word _panic_handler        /* 10 */
    .word _panic_handler        /* 11 */
    .word _panic_handler        /* 12 */
    .word _panic_handler        /* 13 */
    .word _panic_handler        /* 14 */
    .word _panic_handler        /* 15 */
    .word _interrupt_handler    /* 16 */
    .word _interrupt_handler    /* 17 */
    .word _interrupt_handler    /* 18 */
    .word _interrupt_handler    /* 19 */
    .word ohos_p4_tick_isr      /* 20 */
    .word _interrupt_handler    /* 21 */
    .word _interrupt_handler    /* 22 */
    .word _interrupt_handler    /* 23 */
    .word _interrupt_handler    /* 24 */
    .word _interrupt_handler    /* 25 */
    .word _interrupt_handler    /* 26 */
    .word _interrupt_handler    /* 27 */
    .word _interrupt_handler    /* 28 */
    .word _interrupt_handler    /* 29 */
    .word _interrupt_handler    /* 30 */
    .word _interrupt_handler    /* 31 */
    .word _interrupt_handler    /* 32 */
    .word _interrupt_handler    /* 33 */
    .word _interrupt_handler    /* 34 */
    .word _interrupt_handler    /* 35 */
    .word _interrupt_handler    /* 36 */
    .word _interrupt_handler    /* 37 */
    .word _interrupt_handler    /* 38 */
    .word _interrupt_handler    /* 39 */
    .word _interrupt_handler    /* 40 */
    .word _interrupt_handler    /* 41 */
    .word _interrupt_handler    /* 42 */
    .word _interrupt_handler    /* 43 */
    .word _interrupt_handler    /* 44 */
    .word _interrupt_handler    /* 45 */
    .word _interrupt_handler    /* 46 */
    .word _interrupt_handler    /* 47 */
    .size ohos_p4_mtvt_table_vec20, .-ohos_p4_mtvt_table_vec20

    .global ohos_p4_mtvt_table_vec21
    .type ohos_p4_mtvt_table_vec21, @object
    .balign 0x40
ohos_p4_mtvt_table_vec21:
    .word _panic_handler        /* 0 */
    .word _panic_handler        /* 1 */
    .word _panic_handler        /* 2 */
    .word _panic_handler        /* 3 */
    .word _panic_handler        /* 4 */
    .word _panic_handler        /* 5 */
    .word _panic_handler        /* 6 */
    .word _panic_handler        /* 7 */
    .word _panic_handler        /* 8 */
    .word _panic_handler        /* 9 */
    .word _panic_handler        /* 10 */
    .word _panic_handler        /* 11 */
    .word _panic_handler        /* 12 */
    .word _panic_handler        /* 13 */
    .word _panic_handler        /* 14 */
    .word _panic_handler        /* 15 */
    .word _interrupt_handler    /* 16 */
    .word _interrupt_handler    /* 17 */
    .word _interrupt_handler    /* 18 */
    .word _interrupt_handler    /* 19 */
    .word _interrupt_handler    /* 20 */
    .word ohos_p4_tick_isr      /* 21 */
    .word _interrupt_handler    /* 22 */
    .word _interrupt_handler    /* 23 */
    .word _interrupt_handler    /* 24 */
    .word _interrupt_handler    /* 25 */
    .word _interrupt_handler    /* 26 */
    .word _interrupt_handler    /* 27 */
    .word _interrupt_handler    /* 28 */
    .word _interrupt_handler    /* 29 */
    .word _interrupt_handler    /* 30 */
    .word _interrupt_handler    /* 31 */
    .word _interrupt_handler    /* 32 */
    .word _interrupt_handler    /* 33 */
    .word _interrupt_handler    /* 34 */
    .word _interrupt_handler    /* 35 */
    .word _interrupt_handler    /* 36 */
    .word _interrupt_handler    /* 37 */
    .word _interrupt_handler    /* 38 */
    .word _interrupt_handler    /* 39 */
    .word _interrupt_handler    /* 40 */
    .word _interrupt_handler    /* 41 */
    .word _interrupt_handler    /* 42 */
    .word _interrupt_handler    /* 43 */
    .word _interrupt_handler    /* 44 */
    .word _interrupt_handler    /* 45 */
    .word _interrupt_handler    /* 46 */
    .word _interrupt_handler    /* 47 */
    .size ohos_p4_mtvt_table_vec21, .-ohos_p4_mtvt_table_vec21

    .global ohos_p4_tick_isr
    .type ohos_p4_tick_isr, @function
    .balign 4
ohos_p4_tick_isr:
    addi sp, sp, -128
    sw ra, 0(sp)
    sw gp, 4(sp)
    sw tp, 8(sp)
    sw t0, 12(sp)
    sw t1, 16(sp)
    sw t2, 20(sp)
    sw s0, 24(sp)
    sw s1, 28(sp)
    sw a0, 32(sp)
    sw a1, 36(sp)
    sw a2, 40(sp)
    sw a3, 44(sp)
    sw a4, 48(sp)
    sw a5, 52(sp)
    sw a6, 56(sp)
    sw a7, 60(sp)
    sw s2, 64(sp)
    sw s3, 68(sp)
    sw s4, 72(sp)
    sw s5, 76(sp)
    sw s6, 80(sp)
    sw s7, 84(sp)
    sw s8, 88(sp)
    sw s9, 92(sp)
    sw s10, 96(sp)
    sw s11, 100(sp)
    sw t3, 104(sp)
    sw t4, 108(sp)
    sw t5, 112(sp)
    sw t6, 116(sp)
    csrr t0, mepc
    sw t0, 120(sp)
    csrr t0, mstatus
    sw t0, 124(sp)

    call ohos_p4_tick_body

    lw t0, 124(sp)
    csrw mstatus, t0
    lw t0, 120(sp)
    csrw mepc, t0
    lw ra, 0(sp)
    lw gp, 4(sp)
    lw tp, 8(sp)
    lw t0, 12(sp)
    lw t1, 16(sp)
    lw t2, 20(sp)
    lw s0, 24(sp)
    lw s1, 28(sp)
    lw a0, 32(sp)
    lw a1, 36(sp)
    lw a2, 40(sp)
    lw a3, 44(sp)
    lw a4, 48(sp)
    lw a5, 52(sp)
    lw a6, 56(sp)
    lw a7, 60(sp)
    lw s2, 64(sp)
    lw s3, 68(sp)
    lw s4, 72(sp)
    lw s5, 76(sp)
    lw s6, 80(sp)
    lw s7, 84(sp)
    lw s8, 88(sp)
    lw s9, 92(sp)
    lw s10, 96(sp)
    lw s11, 100(sp)
    lw t3, 104(sp)
    lw t4, 108(sp)
    lw t5, 112(sp)
    lw t6, 116(sp)
    addi sp, sp, 128
    mret
