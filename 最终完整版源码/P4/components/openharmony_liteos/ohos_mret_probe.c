void ohos_mret_probe(void);

__attribute__((naked, noinline)) void ohos_mret_probe(void)
{
    __asm__ __volatile__(
        "csrr t1, mstatus\n"
        "la   t0, 1f\n"
        "csrw mepc, t0\n"
        "li   t0, 0x1880\n"
        "csrw mstatus, t0\n"
        "mret\n"
        "1:\n"
        "csrw mstatus, t1\n"
        "ret\n"
    );
}
