# ESP32-P4 LiteOS-M Port Status

Current status: kernel port bring-up passed.

Verified:
- LOS_KernelInit and LOS_Start enter the scheduler.
- RISC-V task context switch works.
- GPTimer raw ISR drives LiteOS tick.
- LOS_TaskDelay wakes from tick.
- Idle task stack is enlarged and protected with OS_TASK_MAGIC_WORD.
- Native OsTaskSwitchCheck is enabled.
- Different-priority task delay scheduling passed.
- Same-priority LOS_TaskYield scheduling passed.
- Semaphore pend/post wake path passed.
- Queue read/write copy wake path passed.
- Quiet queue smoke test is the default development test.

Port knobs:
- OHOS_LITEOS_ESP32P4_TRACE: enable low-level scheduler diagnostics.
- OHOS_LITEOS_ESP32P4_SMOKE_TEST: enable/disable queue smoke test.
- OHOS_LITEOS_ESP32P4_IDLE_STACK_SIZE: idle stack size override.
- OHOS_LITEOS_ESP32P4_QUEUE_SMOKE_PERIOD_MS: queue smoke producer period.

Prebuilt archive note:
The current app links components/openharmony_liteos/lib/libopenharmony_liteos_m.a.
If that archive contains old bring-up strings such as [TOP] or [SWITCH-ENTER],
run components/openharmony_liteos/tools/quiet_liteos_prebuilt_archive.py and
then run riscv32-esp-elf-ranlib on the archive.

Long-term cleanup:
Rebuild libopenharmony_liteos_m.a from LiteOS-M source with ESP32-P4 target
configuration instead of patching a prebuilt archive.
