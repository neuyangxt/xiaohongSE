#include "../ohos_liteos_print_redirect.h"
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include "los_task.h"
#include "los_queue.h"
#include "los_sem.h"
#include "los_mux.h"
#include "los_event.h"
#include "los_memory.h"
#include "los_timer.h"
#include "ohos_types.h"
#include "ohos_errno.h"
#include "utils_list.h"
#include "ohos_init.h"
#include "hilog_lite/log.h"
#include "samgr_lite.h"
#include "memory_adapter.h"
#include "queue_adapter.h"
#include "thread_adapter.h"
#include "time_adapter.h"
#include "feature_impl.h"
#include "service_impl.h"
#include "message_inner.h"
#include "task_manager.h"
#include "service_registry.h"
#include "los_arch_context.h"
#include "soc_common.h"
#include "esp_rom_sys.h"
#include "ohos_liteos_media_task.h"

#ifndef OHOS_BRINGUP_TRACE
#define OHOS_BRINGUP_TRACE 0
#endif

#ifndef OHOS_SMOKE_LOG_EVERY
#define OHOS_SMOKE_LOG_EVERY 10U
#endif

#ifndef OHOS_ENABLE_BRINGUP_SMOKE
#define OHOS_ENABLE_BRINGUP_SMOKE 1
#endif

#ifndef OHOS_ENABLE_KERNEL_CONTINUOUS_SMOKE
#define OHOS_ENABLE_KERNEL_CONTINUOUS_SMOKE 1
#endif

#ifndef OHOS_ENABLE_SAMGR_RUNTIME_VERIFY
#define OHOS_ENABLE_SAMGR_RUNTIME_VERIFY 1
#endif

#ifndef OHOS_ENABLE_DEMO_SERVICE
#define OHOS_ENABLE_DEMO_SERVICE 0
#endif

#if OHOS_ENABLE_DEMO_SERVICE
extern uint32_t OhosDemoServiceStart(void);
#endif

#ifndef OHOS_ENABLE_LED_SERVICE
#define OHOS_ENABLE_LED_SERVICE 0
#endif

#if OHOS_ENABLE_LED_SERVICE
extern uint32_t OhosLedServiceStart(void);
#endif

#ifndef OHOS_ENABLE_KEY_SERVICE
#define OHOS_ENABLE_KEY_SERVICE 0
#endif

#if OHOS_ENABLE_KEY_SERVICE
extern uint32_t OhosKeyServiceStart(void);
#endif

#ifndef OHOS_ENABLE_K2_VERIFY_TASK
#define OHOS_ENABLE_K2_VERIFY_TASK 1
#endif

#if OHOS_ENABLE_K2_VERIFY_TASK
extern uint32_t OhosKeyServiceStartK2VerifyTask(void);
#endif

#ifndef OHOS_ENABLE_UART_LINK_SERVICE
#define OHOS_ENABLE_UART_LINK_SERVICE 0
#endif

#if OHOS_ENABLE_UART_LINK_SERVICE
extern uint32_t OhosUartLinkServiceStart(void);
#endif


#ifndef OHOS_ENABLE_MULTI_SERVICE_RUNTIME
#define OHOS_ENABLE_MULTI_SERVICE_RUNTIME 0
#endif

#if OHOS_ENABLE_LED_SERVICE
extern uint32_t OhosLedServiceRegister(void);
extern uint32_t OhosLedServiceStartTasks(void);
#endif

#if OHOS_ENABLE_KEY_SERVICE
extern uint32_t OhosKeyServiceRegister(void);
extern uint32_t OhosKeyServiceStartTasks(void);
#endif

#if OHOS_ENABLE_UART_LINK_SERVICE
extern uint32_t OhosUartLinkServiceRegister(void);
extern uint32_t OhosUartLinkServiceStartTasks(void);
#endif

#ifndef OHOS_ENABLE_CONFIG_SERVICE
#define OHOS_ENABLE_CONFIG_SERVICE 0
#endif

#if OHOS_ENABLE_CONFIG_SERVICE
extern uint32_t ConfigServiceStartTask(void);
#endif

#ifndef OHOS_ENABLE_FONT_SERVICE
#define OHOS_ENABLE_FONT_SERVICE 0
#endif

#if OHOS_ENABLE_FONT_SERVICE
extern uint32_t FontServiceStartTask(void);
#endif


#ifndef OHOS_ENABLE_DISPLAY_SERVICE
#define OHOS_ENABLE_DISPLAY_SERVICE 0
#endif

#if OHOS_ENABLE_DISPLAY_SERVICE
extern uint32_t OhosDisplayServiceRegister(void);
extern uint32_t OhosDisplayServiceStartTasks(void);
extern uint32_t OhosDisplayServiceStart(void);
#endif


#ifndef OHOS_ENABLE_AUDIO_SERVICE
#define OHOS_ENABLE_AUDIO_SERVICE 0
#endif

#if OHOS_ENABLE_AUDIO_SERVICE
extern uint32_t OhosAudioServiceRegister(void);
extern uint32_t OhosAudioServiceStartTasks(void);
extern uint32_t OhosAudioServiceStart(void);
#endif


#ifndef OHOS_ENABLE_CAMERA_SERVICE
#define OHOS_ENABLE_CAMERA_SERVICE 0
#endif

#if OHOS_ENABLE_CAMERA_SERVICE
extern uint32_t OhosCameraServiceRegister(void);
extern uint32_t OhosCameraServiceStartTasks(void);
extern uint32_t OhosCameraServiceStart(void);
#endif

#ifndef OHOS_ENABLE_MULTI_SERVICE_SELFTEST
#define OHOS_ENABLE_MULTI_SERVICE_SELFTEST 0
#endif

#if OHOS_ENABLE_LED_SERVICE
extern uint32_t OhosLedServiceSelfTest(void);
#endif
#if OHOS_ENABLE_KEY_SERVICE
extern uint32_t OhosKeyServiceSelfTest(void);
#endif
#if OHOS_ENABLE_UART_LINK_SERVICE
extern uint32_t OhosUartLinkServiceSelfTest(void);
#endif
#if OHOS_ENABLE_DISPLAY_SERVICE
extern uint32_t OhosDisplayServiceSelfTest(void);
#endif
#if OHOS_ENABLE_AUDIO_SERVICE
extern uint32_t OhosAudioServiceSelfTest(void);
#endif
#if OHOS_ENABLE_CAMERA_SERVICE
extern uint32_t OhosCameraServiceSelfTest(void);
#endif
#if OHOS_ENABLE_MULTI_SERVICE_SELFTEST
static void OhosMultiServiceSelfTestTask(void *arg);
static UINT32 OhosMultiServiceSelfTestStart(void);
#endif

#if OHOS_BRINGUP_TRACE
#define OHOS_TRACE_PRINTF(...) esp_rom_printf(__VA_ARGS__)
#else
#define OHOS_TRACE_PRINTF(...) do { } while (0)
#endif

extern BOOL OsSchedTaskSwitch(VOID);
extern UINT32 LOS_IntLock(VOID);
extern VOID LOS_IntRestore(UINT32 intSave);

#define LOS_OK 0U

extern int64_t esp_timer_get_time(void);
extern UINT64 LOS_TickCountGet(VOID);
extern UINT32 LOS_MS2Tick(UINT32 millisec);
extern UINT8 *m_aucSysMem0;
volatile UINT64 g_ohos_tick_count;

volatile UINT32 g_ohos_in_tick_isr;
volatile UINT32 g_ohos_defer_sched;
static volatile UINT32 g_ohos_sched_trace_budget = 12U;

#define OHOS_IDLE_STACK_S5H_SIZE 0x2000U
#define OHOS_SMOKE_TASK_STACK_SIZE 0x1000U
static UINT8 g_ohos_idle_stack_s5h[OHOS_IDLE_STACK_S5H_SIZE] __attribute__((aligned(16)));
extern VOID OsSchedStart(VOID);
extern VOID HalStartToRun(VOID);
extern VOID OsTaskEntry(UINT32 taskID);
extern LosTaskCB *OsGetTopTask(VOID);

static ArchTickTimer g_esp32p4TickTimer;

void HalSetLocalInterPri(unsigned int hwiNum, unsigned char priority)
{
    (void)hwiNum;
    (void)priority;
}

void HalIrqEnable(unsigned int hwiNum)
{
    (void)hwiNum;
}

void HalIrqDisable(unsigned int hwiNum)
{
    (void)hwiNum;
}

unsigned int OsVfsInit(void)
{
    return LOS_OK;
}

unsigned int OsPipeInit(void)
{
    return LOS_OK;
}

unsigned int OsSignalInit(void)
{
    return LOS_OK;
}

unsigned int OsSwtmrInit(void)
{
    return LOS_OK;
}

void OsSwtmrTask(void)
{
    while (1) {
    }
}

void OsSwtmrResponseTimeReset(unsigned long long startTime)
{
    (void)startTime;
}

extern UINT32 ohos_esp32p4_tick_timer_start(VOID (*tickHandler)(VOID *));

static UINT32 Esp32p4SysTickStart(VOID (*tickHandler)(VOID *))
{
    return ohos_esp32p4_tick_timer_start(tickHandler);
}


static UINT64 Esp32p4SysTickCycleGet(UINT32 *period)
{
    if (period != NULL) {
        *period = 1;
    }
    return g_ohos_tick_count;
}

static UINT64 Esp32p4SysTickReload(UINT64 nextResponseTime)
{
    (void)nextResponseTime;
    return g_ohos_tick_count;
}

static VOID Esp32p4SysTickLock(VOID)
{
}

static VOID Esp32p4SysTickUnlock(VOID)
{
}

static ArchTickTimer g_esp32p4TickTimer = {
    .freq = 100U,
    .irqNum = 0xffffffffU,
    .periodMax = 0xffffffffULL,
    .init = Esp32p4SysTickStart,
    .getCycle = Esp32p4SysTickCycleGet,
    .reload = Esp32p4SysTickReload,
    .lock = Esp32p4SysTickLock,
    .unlock = Esp32p4SysTickUnlock,
};

ArchTickTimer *ArchSysTickTimerGet(VOID)
{
    return &g_esp32p4TickTimer;
}

VOID ArchTaskSchedule(VOID);

UINT32 ArchEnterSleep(VOID)
{
    if (g_ohos_defer_sched != 0U) {
        g_ohos_defer_sched = 0U;
        ArchTaskSchedule();
    }

    return LOS_OK;
}

VOID ArchInit(VOID)
{
}

VOID HalIrqEndCheckNeedSched(VOID)
{
}


VOID ohos_esp32p4_task_context_switch(UINT32 intSave);

__attribute__((naked, noinline)) VOID ohos_esp32p4_task_context_switch(UINT32 intSave)
{
    __asm__ __volatile__(
        "addi sp, sp, -32 * 4\n"

        "sw   t6, 2 * 4(sp)\n"
        "sw   t5, 3 * 4(sp)\n"
        "sw   t4, 4 * 4(sp)\n"
        "sw   t3, 5 * 4(sp)\n"
        "sw   t2, 6 * 4(sp)\n"
        "sw   t1, 7 * 4(sp)\n"
        "sw   t0, 8 * 4(sp)\n"
        "sw   s11, 9 * 4(sp)\n"
        "sw   s10, 10 * 4(sp)\n"
        "sw   s9, 11 * 4(sp)\n"
        "sw   s8, 12 * 4(sp)\n"
        "sw   s7, 13 * 4(sp)\n"
        "sw   s6, 14 * 4(sp)\n"
        "sw   s5, 15 * 4(sp)\n"
        "sw   a7, 18 * 4(sp)\n"
        "sw   a6, 19 * 4(sp)\n"
        "sw   a5, 20 * 4(sp)\n"
        "sw   a4, 21 * 4(sp)\n"
        "sw   a3, 22 * 4(sp)\n"
        "sw   a2, 23 * 4(sp)\n"
        "sw   a1, 24 * 4(sp)\n"
        "sw   a0, 25 * 4(sp)\n"
        "sw   s4, 26 * 4(sp)\n"
        "sw   s3, 27 * 4(sp)\n"
        "sw   s2, 28 * 4(sp)\n"
        "sw   s1, 29 * 4(sp)\n"
        "sw   s0, 30 * 4(sp)\n"
        "sw   ra, 31 * 4(sp)\n"

        "li   t0, 0x1880\n"
        "sw   t0, 16 * 4(sp)\n"
        "sw   ra, 17 * 4(sp)\n"

        "la   t0, g_losTask\n"
        "lw   t1, 0(t0)\n"
        "sw   sp, 0(t1)\n"
        "lw   t1, 4(t0)\n"
        "sw   t1, 0(t0)\n"
        "lw   sp, 0(t1)\n"

        "lw   t0, 16 * 4(sp)\n"
        "csrw mstatus, t0\n"
        "lw   t0, 17 * 4(sp)\n"
        "csrw mepc, t0\n"

        "lw   t6, 2 * 4(sp)\n"
        "lw   t5, 3 * 4(sp)\n"
        "lw   t4, 4 * 4(sp)\n"
        "lw   t3, 5 * 4(sp)\n"
        "lw   t2, 6 * 4(sp)\n"
        "lw   t1, 7 * 4(sp)\n"
        "lw   t0, 8 * 4(sp)\n"
        "lw   s11, 9 * 4(sp)\n"
        "lw   s10, 10 * 4(sp)\n"
        "lw   s9, 11 * 4(sp)\n"
        "lw   s8, 12 * 4(sp)\n"
        "lw   s7, 13 * 4(sp)\n"
        "lw   s6, 14 * 4(sp)\n"
        "lw   s5, 15 * 4(sp)\n"
        "lw   a7, 18 * 4(sp)\n"
        "lw   a6, 19 * 4(sp)\n"
        "lw   a5, 20 * 4(sp)\n"
        "lw   a4, 21 * 4(sp)\n"
        "lw   a3, 22 * 4(sp)\n"
        "lw   a2, 23 * 4(sp)\n"
        "lw   a1, 24 * 4(sp)\n"
        "lw   a0, 25 * 4(sp)\n"
        "lw   s4, 26 * 4(sp)\n"
        "lw   s3, 27 * 4(sp)\n"
        "lw   s2, 28 * 4(sp)\n"
        "lw   s1, 29 * 4(sp)\n"
        "lw   s0, 30 * 4(sp)\n"
        "lw   ra, 31 * 4(sp)\n"

        "addi sp, sp, 32 * 4\n"
        "mret\n"
    );
}

VOID ArchTaskSchedule(VOID)
{
    if (g_ohos_in_tick_isr != 0U) {
        g_ohos_defer_sched = 1U;
        return;
    }

    UINT32 intSave = LOS_IntLock();
    BOOL isSwitch = OsSchedTaskSwitch();

    if (0) OHOS_TRACE_PRINTF("[OHOS-SCHED] isSwitch=%u run=%p new=%p runId=%u newId=%u runStatus=0x%x newStatus=0x%x\n",
           isSwitch,
           g_losTask.runTask,
           g_losTask.newTask,
           g_losTask.runTask ? g_losTask.runTask->taskID : 0xffffffffU,
           g_losTask.newTask ? g_losTask.newTask->taskID : 0xffffffffU,
           g_losTask.runTask ? g_losTask.runTask->taskStatus : 0xffffffffU,
           g_losTask.newTask ? g_losTask.newTask->taskStatus : 0xffffffffU);

    if (isSwitch) {
        if (g_ohos_sched_trace_budget != 0U) {
            g_ohos_sched_trace_budget--;
            UINT32 *newCtx = (UINT32 *)(g_losTask.newTask ? g_losTask.newTask->stackPointer : 0U);
            UINT32 *runCtx = (UINT32 *)(g_losTask.runTask ? g_losTask.runTask->stackPointer : 0U);
            esp_rom_printf("[OHOS-SCHED] switch runId=%u newId=%u runSp=%p newSp=%p newCtx16=0x%08x newCtx17=0x%08x newCtx31=0x%08x tick=%llu\n",
                           g_losTask.runTask ? g_losTask.runTask->taskID : 0xffffffffU,
                           g_losTask.newTask ? g_losTask.newTask->taskID : 0xffffffffU,
                           runCtx,
                           newCtx,
                           newCtx ? newCtx[16] : 0xffffffffU,
                           newCtx ? newCtx[17] : 0xffffffffU,
                           newCtx ? newCtx[31] : 0xffffffffU,
                           g_ohos_tick_count);
        }
        ohos_esp32p4_task_context_switch(intSave);
        return;
    }

    LOS_IntRestore(intSave);
}


VOID ArchSysExit(VOID)
{
    while (1) {
    }
}

VOID wfi(VOID)
{
    if (g_ohos_defer_sched != 0U) {
        g_ohos_defer_sched = 0U;
        ArchTaskSchedule();
        return;
    }

    __asm__ __volatile__("wfi");
}

VOID mb(VOID)
{
    __asm__ __volatile__("fence":::"memory");
}

VOID dsb(VOID)
{
    __asm__ __volatile__("fence":::"memory");
}

VOID *ArchTskStackInit(UINT32 taskID, UINT32 stackSize, VOID *topStack)
{
    if ((taskID == 0U) && (stackSize < OHOS_IDLE_STACK_S5H_SIZE)) {
        UINT32 *idleStackWord = (UINT32 *)g_ohos_idle_stack_s5h;
        for (UINT32 i = 0; i < (OHOS_IDLE_STACK_S5H_SIZE / sizeof(UINT32)); i++) {
            idleStackWord[i] = OS_TASK_STACK_INIT;
        }
        idleStackWord[0] = OS_TASK_MAGIC_WORD;

        topStack = (VOID *)g_ohos_idle_stack_s5h;
        stackSize = OHOS_IDLE_STACK_S5H_SIZE;
        if (g_taskCBArray != NULL) {
            g_taskCBArray[taskID].topOfStack = (UINTPTR)topStack;
            g_taskCBArray[taskID].stackSize = stackSize;
        }
    }

    TaskContext *context = (TaskContext *)((UINTPTR)topStack + stackSize - sizeof(TaskContext));

    context->mstatus = RISCV_MSTATUS_MPP | RISCV_MSTATUS_MPIE;
    context->mepc = (UINT32)(UINTPTR)OsTaskEntry;
    context->tp = TP_INIT_VALUE;
    context->sp = SP_INIT_VALUE;
    context->s11 = S11_INIT_VALUE;
    context->s10 = S10_INIT_VALUE;
    context->s9 = S9_INIT_VALUE;
    context->s8 = S8_INIT_VALUE;
    context->s7 = S7_INIT_VALUE;
    context->s6 = S6_INIT_VALUE;
    context->s5 = S5_INIT_VALUE;
    context->s4 = S4_INIT_VALUE;
    context->s3 = S3_INIT_VALUE;
    context->s2 = S2_INIT_VALUE;
    context->s1 = S1_INIT_VALUE;
    context->s0 = FP_INIT_VALUE;
    context->t6 = T6_INIT_VALUE;
    context->t5 = T5_INIT_VALUE;
    context->t4 = T4_INIT_VALUE;
    context->t3 = T3_INIT_VALUE;
    context->a7 = A7_INIT_VALUE;
    context->a6 = A6_INIT_VALUE;
    context->a5 = A5_INIT_VALUE;
    context->a4 = A4_INIT_VALUE;
    context->a3 = A3_INIT_VALUE;
    context->a2 = A2_INIT_VALUE;
    context->a1 = A1_INIT_VALUE;
    context->a0 = taskID;
    context->t2 = T2_INIT_VALUE;
    context->t1 = T1_INIT_VALUE;
    context->t0 = T0_INIT_VALUE;
    context->ra = (UINT32)(UINTPTR)ArchSysExit;

    OHOS_TRACE_PRINTF("[OHOS-STACKINIT] task=%u top=%p size=0x%x ctx=%p mepc=%p ra=%p\n",
        taskID, topStack, stackSize, context, (VOID *)context->mepc, (VOID *)context->ra);

    return (VOID *)context;
}

void ohos_liteos_dump_sched_state(void)
{
    LosTaskCB *runTask = g_losTask.runTask;
    LosTaskCB *newTask = g_losTask.newTask;
    LosTaskCB *topTask = OsGetTopTask();

    OHOS_TRACE_PRINTF("[OHOS-DUMP] runTask=%p newTask=%p topTask=%p OsTaskEntry=%p\n",
        runTask, newTask, topTask, OsTaskEntry);

    if (runTask != NULL) {
        OHOS_TRACE_PRINTF("[OHOS-DUMP] run id=%u status=0x%x prio=%u entry=%p arg=%u sp=%p top=0x%x size=0x%x\n",
            runTask->taskID, runTask->taskStatus, runTask->priority,
            runTask->taskEntry, runTask->arg, runTask->stackPointer,
            runTask->topOfStack, runTask->stackSize);
    }

    if (topTask != NULL) {
        OHOS_TRACE_PRINTF("[OHOS-DUMP] top id=%u status=0x%x prio=%u entry=%p arg=%u sp=%p top=0x%x size=0x%x\n",
            topTask->taskID, topTask->taskStatus, topTask->priority,
            topTask->taskEntry, topTask->arg, topTask->stackPointer,
            topTask->topOfStack, topTask->stackSize);
    }
}


void ohos_start_task_mret_probe(UINT32 *ctx);

__attribute__((naked, noinline)) void ohos_start_task_mret_probe(UINT32 *ctx)
{
    __asm__ __volatile__(
        /* ctx pointer is in a0. Keep it in t0 while restoring registers. */
        "mv   t0, a0\n"

        /* CSR restore. */
        "lw   t1, 16 * 4(t0)\n"
        "csrw mstatus, t1\n"
        "lw   t1, 17 * 4(t0)\n"
        "csrw mepc, t1\n"

        /* Restore tp too, but still intentionally skip sp(x2). */
        "lw   tp, 1 * 4(t0)\n"
        "lw   ra, 31 * 4(t0)\n"

        "lw   t6, 2 * 4(t0)\n"
        "lw   t5, 3 * 4(t0)\n"
        "lw   t4, 4 * 4(t0)\n"
        "lw   t3, 5 * 4(t0)\n"
        "lw   t2, 6 * 4(t0)\n"
        "lw   t1, 7 * 4(t0)\n"

        "lw   s11, 9 * 4(t0)\n"
        "lw   s10, 10 * 4(t0)\n"
        "lw   s9, 11 * 4(t0)\n"
        "lw   s8, 12 * 4(t0)\n"
        "lw   s7, 13 * 4(t0)\n"
        "lw   s6, 14 * 4(t0)\n"
        "lw   s5, 15 * 4(t0)\n"

        "lw   a7, 18 * 4(t0)\n"
        "lw   a6, 19 * 4(t0)\n"
        "lw   a5, 20 * 4(t0)\n"
        "lw   a4, 21 * 4(t0)\n"
        "lw   a3, 22 * 4(t0)\n"
        "lw   a2, 23 * 4(t0)\n"
        "lw   a1, 24 * 4(t0)\n"
        "lw   a0, 25 * 4(t0)\n"

        "lw   s4, 26 * 4(t0)\n"
        "lw   s3, 27 * 4(t0)\n"
        "lw   s2, 28 * 4(t0)\n"
        "lw   s1, 29 * 4(t0)\n"
        "lw   s0, 30 * 4(t0)\n"

        /*
         * Switch to the LiteOS task stack after reading all needed context.
         * ctx points to the saved TaskContext. After a full restore, sp should
         * point just above this context block.
         */
        "addi sp, t0, 32 * 4\n"

        /* Do not restore t0 from ctx[8], because t0 holds ctx during restore. */
        "mret\n"
    );
}


void ohos_start_task_pop_probe(UINT32 *ctx);

__attribute__((naked, noinline)) void ohos_start_task_pop_probe(UINT32 *ctx)
{
    __asm__ __volatile__(
        "mv   sp, a0\n"

        "lw   t0, 16 * 4(sp)\n"
        "csrw mstatus, t0\n"
        "lw   t0, 17 * 4(sp)\n"
        "csrw mepc, t0\n"

        "lw   t6, 2 * 4(sp)\n"
        "lw   t5, 3 * 4(sp)\n"
        "lw   t4, 4 * 4(sp)\n"
        "lw   t3, 5 * 4(sp)\n"
        "lw   t2, 6 * 4(sp)\n"
        "lw   t1, 7 * 4(sp)\n"
        "lw   t0, 8 * 4(sp)\n"
        "lw   s11, 9 * 4(sp)\n"
        "lw   s10, 10 * 4(sp)\n"
        "lw   s9, 11 * 4(sp)\n"
        "lw   s8, 12 * 4(sp)\n"
        "lw   s7, 13 * 4(sp)\n"
        "lw   s6, 14 * 4(sp)\n"
        "lw   s5, 15 * 4(sp)\n"
        "lw   a7, 18 * 4(sp)\n"
        "lw   a6, 19 * 4(sp)\n"
        "lw   a5, 20 * 4(sp)\n"
        "lw   a4, 21 * 4(sp)\n"
        "lw   a3, 22 * 4(sp)\n"
        "lw   a2, 23 * 4(sp)\n"
        "lw   a1, 24 * 4(sp)\n"
        "lw   a0, 25 * 4(sp)\n"
        "lw   s4, 26 * 4(sp)\n"
        "lw   s3, 27 * 4(sp)\n"
        "lw   s2, 28 * 4(sp)\n"
        "lw   s1, 29 * 4(sp)\n"
        "lw   s0, 30 * 4(sp)\n"
        "lw   ra, 31 * 4(sp)\n"

        "addi sp, sp, 32 * 4\n"
        "mret\n"
    );
}


void ohos_jump_real_hal_start(LosTaskCB *task);

__attribute__((naked, noinline)) void ohos_jump_real_hal_start(LosTaskCB *task)
{
    __asm__ __volatile__(
        "la   t0, g_losTask\n"
        "sw   a0, 0(t0)\n"
        "sw   a0, 4(t0)\n"
        "la   t1, HalStartToRun\n"
        "jr   t1\n"
    );
}


VOID ohos_esp32p4_start_to_run(VOID);

__attribute__((naked, noinline)) VOID ohos_esp32p4_start_to_run(VOID)
{
    __asm__ __volatile__(
        "la   t0, g_losTask\n"
        "lw   t0, 4(t0)\n"
        "lw   sp, 0(t0)\n"

        "lw   t0, 16 * 4(sp)\n"
        "csrw mstatus, t0\n"
        "lw   t0, 17 * 4(sp)\n"
        "csrw mepc, t0\n"

        "lw   t6, 2 * 4(sp)\n"
        "lw   t5, 3 * 4(sp)\n"
        "lw   t4, 4 * 4(sp)\n"
        "lw   t3, 5 * 4(sp)\n"
        "lw   t2, 6 * 4(sp)\n"
        "lw   t1, 7 * 4(sp)\n"
        "lw   t0, 8 * 4(sp)\n"
        "lw   s11, 9 * 4(sp)\n"
        "lw   s10, 10 * 4(sp)\n"
        "lw   s9, 11 * 4(sp)\n"
        "lw   s8, 12 * 4(sp)\n"
        "lw   s7, 13 * 4(sp)\n"
        "lw   s6, 14 * 4(sp)\n"
        "lw   s5, 15 * 4(sp)\n"
        "lw   a7, 18 * 4(sp)\n"
        "lw   a6, 19 * 4(sp)\n"
        "lw   a5, 20 * 4(sp)\n"
        "lw   a4, 21 * 4(sp)\n"
        "lw   a3, 22 * 4(sp)\n"
        "lw   a2, 23 * 4(sp)\n"
        "lw   a1, 24 * 4(sp)\n"
        "lw   a0, 25 * 4(sp)\n"
        "lw   s4, 26 * 4(sp)\n"
        "lw   s3, 27 * 4(sp)\n"
        "lw   s2, 28 * 4(sp)\n"
        "lw   s1, 29 * 4(sp)\n"
        "lw   s0, 30 * 4(sp)\n"
        "lw   ra, 31 * 4(sp)\n"

        "addi sp, sp, 32 * 4\n"
        "mret\n"
    );
}

UINT32 ArchStartSchedule(VOID)
{
    LosTaskCB *peek = OsGetTopTask();
    OHOS_TRACE_PRINTF("[OHOS-START] peek id=%u status=0x%x prio=%u sp=%p\n",
                   peek ? peek->taskID : 0xffffffffU,
                   peek ? peek->taskStatus : 0xffffffffU,
                   peek ? peek->priority : 0xffffffffU,
                   peek ? peek->stackPointer : NULL);

    /* Do not touch taskStatus here. OsSchedStart sets RUNNING and dequeues READY. */
    OsSchedStart();

    LosTaskCB *newTask = g_losTask.runTask;
    if (newTask != NULL) {
        UINT32 *ctx = (UINT32 *)newTask->stackPointer;
        OHOS_TRACE_PRINTF("[OHOS-START] selected id=%u status=0x%x sp=%p\n",
                       newTask->taskID, newTask->taskStatus, ctx);
        if (ctx != NULL) {
            OHOS_TRACE_PRINTF("[OHOS-START] ctx[16]=0x%08x ctx[17]=0x%08x ctx[31]=0x%08x\n",
                           ctx[16], ctx[17], ctx[31]);
        }
    }

    ohos_esp32p4_start_to_run();
    return LOS_OK;
}

#ifndef LOS_WAIT_FOREVER
#define LOS_WAIT_FOREVER 0xFFFFFFFFU
#endif

typedef struct {
    UINT32 seq;
    UINT32 producerTick;
    UINT32 producerUsLow;
    UINT32 magic;
} OhosLiteosQueueMsg;

static UINT32 g_ohos_s7_queue;
static UINT32 g_ohos_s8_sem;
static UINT32 g_ohos_s8_mux;
static UINT32 g_ohos_s8_shared_counter;
static EVENT_CB_S g_ohos_s8_event;

#define OHOS_S8_EVENT_BIT_A 0x00000001U
#define OHOS_S8_EVENT_BIT_B 0x00000002U

static VOID *OhosLiteosQueueConsumerTask(UINT32 arg)
{
    (void)arg;

    esp_rom_printf("[OHOS-S7] queue smoke consumer entered prio=10 queue=%u\n", g_ohos_s7_queue);

    UINT32 count = 0;
    while (1) {
        OhosLiteosQueueMsg msg = {0};
        UINT32 msgSize = sizeof(msg);
        UINT64 tick0 = LOS_TickCountGet();

        UINT32 readRet = LOS_QueueReadCopy(g_ohos_s7_queue, &msg, &msgSize, LOS_WAIT_FOREVER);

        long long us1 = esp_timer_get_time();
        UINT64 tick1 = LOS_TickCountGet();
        UINT32 expectedMagic = 0xD7000000U | (msg.seq & 0x00FFFFFFU);
        BOOL badMsg = (readRet != LOS_OK) || (msgSize != sizeof(msg)) || (msg.magic != expectedMagic);

        if (badMsg || ((count % OHOS_SMOKE_LOG_EVERY) == 0U)) {
            LosTaskCB *runAfter = g_losTask.runTask;
            esp_rom_printf("[OHOS-S7] queue smoke count=%u ret=0x%x msgSize=%u seq=%u magic=0x%x tick_delta=%llu us=%lld runId=%u status=0x%x\n",
                           count,
                           readRet,
                           msgSize,
                           msg.seq,
                           msg.magic,
                           tick1 - tick0,
                           us1,
                           runAfter ? runAfter->taskID : 0xffffffffU,
                           runAfter ? runAfter->taskStatus : 0xffffffffU);
        }

        count++;
    }

    return NULL;
}

static VOID OhosLiteosThreadCreateRuntimeSmoke(void);

static VOID OhosLiteosSamgrTaskPoolStandaloneSmoke(void);

static VOID OhosLiteosSamgrSingleTaskRuntimeVerifySmoke(void);

static VOID OhosLiteosS31BTrueAckRuntimeVerifySmoke(void);

static VOID *OhosLiteosQueueProducerTask(UINT32 arg)
{
#if OHOS_ENABLE_BRINGUP_SMOKE && OHOS_ENABLE_SAMGR_RUNTIME_VERIFY
    OhosLiteosS31BTrueAckRuntimeVerifySmoke();
    OhosLiteosSamgrSingleTaskRuntimeVerifySmoke();
#endif

    (void)arg;

    UINT32 delayTicks = LOS_MS2Tick(200);
    if (delayTicks == 0) {
        delayTicks = 1;
    }

    esp_rom_printf("[OHOS-S7] queue smoke producer entered prio=11 queue=%u periodMs=200 delayTicks=%u\n",
                   g_ohos_s7_queue, delayTicks);

    UINT32 count = 0;
    while (1) {
        (void)LOS_TaskDelay(delayTicks);

        long long us0 = esp_timer_get_time();
        UINT64 tick0 = LOS_TickCountGet();
        OhosLiteosQueueMsg msg = {
            .seq = count,
            .producerTick = (UINT32)tick0,
            .producerUsLow = (UINT32)us0,
            .magic = 0xD7000000U | (count & 0x00FFFFFFU),
        };

        UINT32 writeRet = LOS_QueueWriteCopy(g_ohos_s7_queue, &msg, sizeof(msg), 0);
        if (writeRet != LOS_OK) {
            esp_rom_printf("[OHOS-S7] queue smoke producer write error ret=0x%x count=%u\n",
                           writeRet, count);
        }

        count++;
    }

    return NULL;
}

static VOID *OhosLiteosSemTakerTask(UINT32 arg)
{
    (void)arg;

    esp_rom_printf("[OHOS-S8] sem taker entered prio=9 sem=%u\n", g_ohos_s8_sem);

    UINT32 count = 0;
    while (1) {
        UINT64 tick0 = LOS_TickCountGet();
        UINT32 ret = LOS_SemPend(g_ohos_s8_sem, LOS_WAIT_FOREVER);
        UINT64 tick1 = LOS_TickCountGet();

        UINT32 events = 0;
        if (ret == LOS_OK) {
            events = LOS_EventRead(&g_ohos_s8_event,
                                   OHOS_S8_EVENT_BIT_A | OHOS_S8_EVENT_BIT_B,
                                   LOS_WAITMODE_AND | LOS_WAITMODE_CLR,
                                   0);
        }

        if ((ret != LOS_OK) ||
            ((events & (OHOS_S8_EVENT_BIT_A | OHOS_S8_EVENT_BIT_B)) !=
             (OHOS_S8_EVENT_BIT_A | OHOS_S8_EVENT_BIT_B)) ||
            ((count % OHOS_SMOKE_LOG_EVERY) == 0U)) {
            esp_rom_printf("[OHOS-S8] sem+event smoke count=%u pendRet=0x%x events=0x%x tick_delta=%llu us=%lld\n",
                           count,
                           ret,
                           events,
                           tick1 - tick0,
                           esp_timer_get_time());
        }

        count++;
    }

    return NULL;
}

static VOID *OhosLiteosSemGiverTask(UINT32 arg)
{
    (void)arg;

    UINT32 delayTicks = LOS_MS2Tick(300);
    if (delayTicks == 0) {
        delayTicks = 1;
    }

    esp_rom_printf("[OHOS-S8] sem giver entered prio=12 sem=%u periodMs=300 delayTicks=%u\n",
                   g_ohos_s8_sem, delayTicks);

    UINT32 count = 0;
    while (1) {
        (void)LOS_TaskDelay(delayTicks);

        UINT32 eventRet = LOS_EventWrite(&g_ohos_s8_event,
                                       OHOS_S8_EVENT_BIT_A | OHOS_S8_EVENT_BIT_B);
        if (eventRet != LOS_OK) {
            esp_rom_printf("[OHOS-S8] event write error ret=0x%x count=%u\n",
                           eventRet, count);
        }

        UINT32 ret = LOS_SemPost(g_ohos_s8_sem);
        if (ret != LOS_OK) {
            esp_rom_printf("[OHOS-S8] sem post error ret=0x%x count=%u\n",
                           ret, count);
        }

        count++;
    }

    return NULL;
}

static VOID *OhosLiteosMuxTask(UINT32 arg)
{
    UINT32 taskNo = arg;

    UINT32 delayTicks = LOS_MS2Tick(150);
    if (delayTicks == 0) {
        delayTicks = 1;
    }

    esp_rom_printf("[OHOS-S8] mux task%u entered prio=13 mux=%u delayTicks=%u\n",
                   taskNo, g_ohos_s8_mux, delayTicks);

    UINT32 count = 0;
    while (1) {
        UINT32 ret = LOS_MuxPend(g_ohos_s8_mux, LOS_WAIT_FOREVER);
        if (ret != LOS_OK) {
            esp_rom_printf("[OHOS-S8] mux task%u pend error ret=0x%x count=%u\n",
                           taskNo, ret, count);
        } else {
            UINT32 before = g_ohos_s8_shared_counter;
            g_ohos_s8_shared_counter = before + 1;

            VOID *mem = LOS_MemAlloc(m_aucSysMem0, 64);
            if (mem == NULL) {
                esp_rom_printf("[OHOS-S8] memory alloc failed task%u count=%u\n",
                               taskNo, count);
            } else {
                UINT8 *buf = (UINT8 *)mem;
                for (UINT32 i = 0; i < 64; i++) {
                    buf[i] = (UINT8)(taskNo + count + i);
                }

                UINT32 freeRet = LOS_MemFree(m_aucSysMem0, mem);
                if (freeRet != LOS_OK) {
                    esp_rom_printf("[OHOS-S8] memory free error task%u ret=0x%x count=%u\n",
                                   taskNo, freeRet, count);
                }
            }

            ret = LOS_MuxPost(g_ohos_s8_mux);
            if (ret != LOS_OK) {
                esp_rom_printf("[OHOS-S8] mux task%u post error ret=0x%x count=%u\n",
                               taskNo, ret, count);
            }

            if ((count % OHOS_SMOKE_LOG_EVERY) == 0U) {
                esp_rom_printf("[OHOS-S8] mux+memory smoke task%u count=%u shared=%u ret=0x%x tick=%llu us=%lld\n",
                               taskNo,
                               count,
                               g_ohos_s8_shared_counter,
                               ret,
                               LOS_TickCountGet(),
                               esp_timer_get_time());
            }
        }

        count++;
        (void)LOS_TaskDelay(delayTicks);
    }

    return NULL;
}



/* S10: minimal hilog_lite adapter for ESP32-P4 bring-up.
 * This is not the full hilog_lite service. It only maps HILOG_xxx to UART output.
 */
const char * const FUN_ARG_S = "01234567";

static const char *OhosLiteosHilogLevelName(uint8 level)
{
    switch (level) {
        case HILOG_LV_DEBUG:
            return "D";
        case HILOG_LV_INFO:
            return "I";
        case HILOG_LV_WARN:
            return "W";
        case HILOG_LV_ERROR:
            return "E";
        case HILOG_LV_FATAL:
            return "F";
        default:
            return "?";
    }
}

void HiLogPrintf(uint8 module, uint8 level, const char *nums, const char *fmt, ...)
{
    (void)nums;

    char buf[160];

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n < 0) {
        esp_rom_printf("[OHOS-S10][HILOG][%s][mod=%u] format error\n",
                       OhosLiteosHilogLevelName(level), module);
        return;
    }

    buf[sizeof(buf) - 1] = '\0';
    esp_rom_printf("[OHOS-S10][HILOG][%s][mod=%u] %s\n",
                   OhosLiteosHilogLevelName(level), module, buf);
}

void HiLogFlush(boolean syncFlag)
{
    (void)syncFlag;
}

boolean HiLogRegisterModule(uint16 id, const char *name)
{
    esp_rom_printf("[OHOS-S10][HILOG] register module id=%u name=%s\n", id, name);
    return TRUE;
}

const char *HiLogGetModuleName(uint8 id)
{
    (void)id;
    return "ESP32P4";
}


/* S11: startup/bootstrap_lite pre-check.
 * This verifies that ohos_init.h init macros can be compiled into the ESP-IDF image.
 * It does NOT run full OHOS_SystemInit yet.
 */
static VOID OhosLiteosStartupInitcallProbe(void)
{
    esp_rom_printf("[OHOS-S11] startup initcall probe body executed\n");
}

SYS_SERVICE_INIT(OhosLiteosStartupInitcallProbe);



/* S13: minimal samgr_lite adapter for ESP32-P4 + LiteOS-M.
 * This is not full samgr_lite. It only verifies adapter APIs on top of LOS_xxx.
 */
typedef struct {
    UINT32 queueId;
    UINT32 msgSize;
} OhosLiteosSamgrQueue;

void *SAMGR_Malloc(uint32 size)
{
    if (size == 0) {
        return NULL;
    }
    return LOS_MemAlloc(m_aucSysMem0, size);
}

void SAMGR_Free(void *buffer)
{
    if (buffer == NULL) {
        return;
    }
    (void)LOS_MemFree(m_aucSysMem0, buffer);
}

MQueueId QUEUE_Create(const char *name, int size, int count)
{
    if ((size <= 0) || (count <= 0)) {
        return NULL;
    }

    OhosLiteosSamgrQueue *q = (OhosLiteosSamgrQueue *)SAMGR_Malloc(sizeof(OhosLiteosSamgrQueue));
    if (q == NULL) {
        return NULL;
    }

    UINT32 queueId = 0;
    UINT32 ret = LOS_QueueCreate((char *)name, (UINT16)count, &queueId, 0, (UINT16)size);
    if (ret != LOS_OK) {
        SAMGR_Free(q);
        return NULL;
    }

    q->queueId = queueId;
    q->msgSize = (UINT32)size;
    return (MQueueId)q;
}

int QUEUE_Put(MQueueId queueId, const void *element, uint8 pri, int timeout)
{
    (void)pri;
    (void)timeout;

    if ((queueId == NULL) || (element == NULL)) {
        return EC_BADPTR;
    }

    OhosLiteosSamgrQueue *q = (OhosLiteosSamgrQueue *)queueId;
    UINT32 ret = LOS_QueueWriteCopy(q->queueId, (VOID *)element, q->msgSize, 0);
    return (ret == LOS_OK) ? EC_SUCCESS : EC_BUSBUSY;
}

int QUEUE_Pop(MQueueId queueId, void *element, uint8 *pri, int timeout)
{
    if ((queueId == NULL) || (element == NULL)) {
        return EC_BADPTR;
    }

    if (pri != NULL) {
        *pri = 0;
    }

    OhosLiteosSamgrQueue *q = (OhosLiteosSamgrQueue *)queueId;
    UINT32 msgSize = q->msgSize;
    UINT32 waitTicks = (timeout <= 0) ? LOS_WAIT_FOREVER : LOS_MS2Tick((UINT32)timeout);
    if ((timeout > 0) && (waitTicks == 0)) {
        waitTicks = 1;
    }

    UINT32 ret = LOS_QueueReadCopy(q->queueId, element, &msgSize, waitTicks);
    return (ret == LOS_OK) ? EC_SUCCESS : EC_BUSBUSY;
}

int QUEUE_Destroy(MQueueId queueId)
{
    if (queueId == NULL) {
        return EC_BADPTR;
    }

    OhosLiteosSamgrQueue *q = (OhosLiteosSamgrQueue *)queueId;
    UINT32 ret = LOS_QueueDelete(q->queueId);
    SAMGR_Free(q);

    return (ret == LOS_OK) ? EC_SUCCESS : EC_FAILURE;
}

MutexId MUTEX_InitValue(void)
{
    UINT32 *mux = (UINT32 *)SAMGR_Malloc(sizeof(UINT32));
    if (mux == NULL) {
        return NULL;
    }

    UINT32 ret = LOS_MuxCreate(mux);
    if (ret != LOS_OK) {
        SAMGR_Free(mux);
        return NULL;
    }

    return (MutexId)mux;
}

void MUTEX_Lock(MutexId mutex)
{
    if (mutex == NULL) {
        return;
    }

    UINT32 *mux = (UINT32 *)mutex;
    (void)LOS_MuxPend(*mux, LOS_WAIT_FOREVER);
}

void MUTEX_Unlock(MutexId mutex)
{
    if (mutex == NULL) {
        return;
    }

    UINT32 *mux = (UINT32 *)mutex;
    (void)LOS_MuxPost(*mux);
}

static UINT32 g_samgr_global_int_save;

void MUTEX_GlobalLock(void)
{
    g_samgr_global_int_save = LOS_IntLock();
}

void MUTEX_GlobalUnlock(void)
{
    LOS_IntRestore(g_samgr_global_int_save);
}

/* S28 THREAD adapter state begin */
#define OHOS_LITEOS_THREAD_CTX_MAX 16

typedef struct {
    BOOL used;
    Runnable run;
    void *argv;
    void *local;
    UINT32 taskId;
} OhosLiteosThreadCtx;

static OhosLiteosThreadCtx g_ohos_thread_ctx[OHOS_LITEOS_THREAD_CTX_MAX];
static volatile UINT32 g_ohos_thread_total = 0;
static OhosLiteosThreadCtx *g_ohos_current_thread_ctx = NULL;
static void *g_ohos_main_thread_local = NULL;
static VOID *OhosLiteosThreadEntry(UINT32 arg)
{
    OhosLiteosThreadCtx *ctx = (OhosLiteosThreadCtx *)(UINTPTR)arg;

    if (ctx == NULL || ctx->run == NULL) {
        return NULL;
    }

    g_ohos_current_thread_ctx = ctx;
    ctx->local = ctx->argv;

    void *ret = ctx->run(ctx->argv);

    g_ohos_current_thread_ctx = NULL;
    return ret;
}
/* S28 THREAD adapter state end */


ThreadId THREAD_Create(Runnable run, void *argv, const ThreadAttr *attr)
{
    if (run == NULL) {
        return NULL;
    }

    OhosLiteosThreadCtx *ctx = NULL;
    UINT32 intSave = LOS_IntLock();

    for (int i = 0; i < OHOS_LITEOS_THREAD_CTX_MAX; ++i) {
        if (!g_ohos_thread_ctx[i].used) {
            ctx = &g_ohos_thread_ctx[i];
            ctx->used = TRUE;
            ctx->run = run;
            ctx->argv = argv;
            ctx->local = argv;
            ctx->taskId = 0;
            break;
        }
    }

    LOS_IntRestore(intSave);

    if (ctx == NULL) {
        esp_rom_printf("[OHOS-S28A-SAFE] THREAD_Create no free ctx\n");
        return NULL;
    }

    TSK_INIT_PARAM_S initParam = {0};
    initParam.pfnTaskEntry = OhosLiteosThreadEntry;
    initParam.usTaskPrio = (attr != NULL) ? attr->priority : 24;
    initParam.uwStackSize = (attr != NULL && attr->stackSize >= 0x1000) ? attr->stackSize : OHOS_SMOKE_TASK_STACK_SIZE;
    initParam.pcName = (CHAR *)((attr != NULL && attr->name != NULL) ? attr->name : "ohos_thread");
    initParam.uwArg = (UINT32)(UINTPTR)ctx;
    initParam.uwResved = 0;

    UINT32 taskId = 0;
    UINT32 ret = LOS_TaskCreate(&taskId, &initParam);
    if (ret != LOS_OK) {
        ctx->used = FALSE;
        ctx->run = NULL;
        ctx->argv = NULL;
        ctx->local = NULL;
        ctx->taskId = 0;

        esp_rom_printf("[OHOS-S28A-SAFE] THREAD_Create LOS_TaskCreate failed ret=0x%x prio=%u stack=0x%x\n",
                       ret, initParam.usTaskPrio, initParam.uwStackSize);
        return NULL;
    }

    ctx->taskId = taskId;
    g_ohos_thread_total++;

    esp_rom_printf("[OHOS-S28A-SAFE] THREAD_Create mapped taskId=%u prio=%u stack=0x%x ctx=%p\n",
                   taskId, initParam.usTaskPrio, initParam.uwStackSize, ctx);

    return (ThreadId)ctx;
}


int THREAD_Total(void)
{
    return (int)g_ohos_thread_total;
}


void *THREAD_GetThreadLocal(void)
{
    if (g_ohos_current_thread_ctx != NULL) {
        return g_ohos_current_thread_ctx->local;
    }

    return g_ohos_main_thread_local;
}


void THREAD_SetThreadLocal(const void *local)
{
    if (g_ohos_current_thread_ctx != NULL) {
        g_ohos_current_thread_ctx->local = (void *)local;
        return;
    }

    g_ohos_main_thread_local = (void *)local;
}

int32 WDT_Start(uint32 ms)
{
    (void)ms;
    return EC_FAILURE;
}

int32 WDT_Reset(uint32 ms)
{
    (void)ms;
    return EC_FAILURE;
}

int32 WDT_Stop(void)
{
    return EC_FAILURE;
}

uint64 SAMGR_GetProcessTime(void)
{
    return (uint64)(LOS_TickCountGet() * 10ULL);
}


static VOID OhosLiteosSamgrLiteHeaderSmoke(void)
{
    TaskConfig config = {
        .level = LEVEL_HIGH,
        .priority = PRI_NORMAL,
        .stackSize = 0x800,
        .queueSize = 20,
        .taskFlags = SHARED_TASK,
    };

    BootMessage bootMsg = BOOT_SYS_COMPLETED;

    esp_rom_printf("[OHOS-S12] samgr_lite header smoke service=%s bootMsg=%d taskPrio=%d stack=0x%x queue=%u flags=%u serviceSize=%u featureSize=%u\n",
                   BOOTSTRAP_SERVICE,
                   bootMsg,
                   config.priority,
                   config.stackSize,
                   config.queueSize,
                   config.taskFlags,
                   (UINT32)sizeof(Service),
                   (UINT32)sizeof(Feature));

    HILOG_INFO(HILOG_MODULE_INIT, "samgr_lite header smoke ok prio=%d", config.priority);
}


static VOID OhosLiteosStartupLiteSmoke(void)
{
    esp_rom_printf("[OHOS-S11] startup_lite macro smoke SYS_SERVICE_INIT compiled, manual call next\n");
    OhosLiteosStartupInitcallProbe();
    HILOG_INFO(HILOG_MODULE_INIT, "startup_lite init macro smoke ok");
}


static VOID OhosLiteosUtilsLiteSmoke(void)
{
    UTILS_DL_LIST list;
    UtilsListInit(&list);

    uint32 arr[] = {1, 2, 3, 4};
    uint32 arrSize = ARRAY_SIZE(arr);
    boolean listOk = (list.pstNext == &list) && (list.pstPrev == &list);

    esp_rom_printf("[OHOS-S9] utils_lite smoke OHOS_SUCCESS=%d EC_SUCCESS=%d EC_NOMEMORY=%d arrSize=%u listOk=%d\n",
                   OHOS_SUCCESS,
                   EC_SUCCESS,
                   EC_NOMEMORY,
                   arrSize,
                   listOk);
}


static VOID OhosLiteosHilogLiteSmoke(void)
{
    boolean regOk = HiLogRegisterModule(HILOG_MODULE_INIT, "INIT");

    HILOG_INFO(HILOG_MODULE_INIT, "hilog_lite info smoke reg=%u value=%u", regOk, 1234U);
    HILOG_ERROR(HILOG_MODULE_INIT, "hilog_lite error smoke code=%d", EC_FAILURE);
}




typedef struct {
    INHERIT_IUNKNOWNENTRY(IUnknown);
    uint32 magic;
} OhosLiteosIUnknownSmokeObj;


typedef struct {
    INHERIT_IUNKNOWNENTRY(IUnknown);
    uint32 magic;
} OhosLiteosFeatureIUnknownObj;

static const char *OhosLiteosFeatureGetName(Feature *feature)
{
    (void)feature;
    return "S16Feature";
}

static void OhosLiteosFeatureOnInitialize(Feature *feature, Service *parent, Identity identity)
{
    (void)feature;
    (void)parent;
    (void)identity;
}

static void OhosLiteosFeatureOnStop(Feature *feature, Identity identity)
{
    (void)feature;
    (void)identity;
}

static BOOL OhosLiteosFeatureOnMessage(Feature *feature, Request *request)
{
    (void)feature;
    (void)request;
    return TRUE;
}


static const char *OhosLiteosServiceGetName(Service *service)
{
    (void)service;
    return "S17Service";
}

static BOOL OhosLiteosServiceInitialize(Service *service, Identity identity)
{
    (void)service;
    (void)identity;
    return TRUE;
}

static BOOL OhosLiteosServiceMessageHandle(Service *service, Request *request)
{
    (void)service;
    (void)request;
    return TRUE;
}

static TaskConfig OhosLiteosServiceGetTaskConfig(Service *service)
{
    (void)service;
    TaskConfig config = {
        .level = LEVEL_HIGH,
        .priority = PRI_NORMAL,
        .stackSize = 0x800,
        .queueSize = 4,
        .taskFlags = SHARED_TASK,
    };
    return config;
}

static const char *OhosLiteosS17FeatureGetName(Feature *feature)
{
    (void)feature;
    return "S17Feature";
}

static void OhosLiteosS17FeatureOnInitialize(Feature *feature, Service *parent, Identity identity)
{
    (void)feature;
    (void)parent;
    (void)identity;
}

static void OhosLiteosS17FeatureOnStop(Feature *feature, Identity identity)
{
    (void)feature;
    (void)identity;
}

static BOOL OhosLiteosS17FeatureOnMessage(Feature *feature, Request *request)
{
    (void)feature;
    (void)request;
    return TRUE;
}


/* S18 temporary stub.
 * Full SAMGR_GetCurrentQueueID will be provided by task_manager.c later.
 */

static const char *OhosLiteosS22ServiceGetName(Service *service)
{
    (void)service;
    return "S22Service";
}

static BOOL OhosLiteosS22ServiceInit(Service *service, Identity identity)
{
    (void)service;
    esp_rom_printf("[OHOS-S22] service init should not run serviceId=%d featureId=%d queue=%p\n",
                   identity.serviceId, identity.featureId, identity.queueId);
    return TRUE;
}

static BOOL OhosLiteosS22ServiceMsg(Service *service, Request *request)
{
    (void)service;
    esp_rom_printf("[OHOS-S22] service message should not run req=%p\n", request);
    return TRUE;
}

static TaskConfig OhosLiteosS22ServiceTaskConfig(Service *service)
{
    (void)service;

    TaskConfig config = {
        .level = LEVEL_HIGH,
        .priority = PRI_NORMAL,
        .stackSize = 0x800,
        .queueSize = 2,
        .taskFlags = SHARED_TASK,
    };

    return config;
}

static Service g_ohos_s22_service = {
    .GetName = OhosLiteosS22ServiceGetName,
    .Initialize = OhosLiteosS22ServiceInit,
    .MessageHandle = OhosLiteosS22ServiceMsg,
    .GetTaskConfig = OhosLiteosS22ServiceTaskConfig,
};

static const char *OhosLiteosS23FeatureGetName(Feature *feature)
{
    (void)feature;
    return "S23Feature";
}

static void OhosLiteosS23FeatureInit(Feature *feature, Service *parent, Identity identity)
{
    (void)feature;
    (void)parent;
    esp_rom_printf("[OHOS-S23] feature init should not run serviceId=%d featureId=%d queue=%p\n",
                   identity.serviceId, identity.featureId, identity.queueId);
}

static void OhosLiteosS23FeatureStop(Feature *feature, Identity identity)
{
    (void)feature;
    esp_rom_printf("[OHOS-S23] feature stop should not run serviceId=%d featureId=%d queue=%p\n",
                   identity.serviceId, identity.featureId, identity.queueId);
}

static BOOL OhosLiteosS23FeatureMsg(Feature *feature, Request *request)
{
    (void)feature;
    esp_rom_printf("[OHOS-S23] feature message should not run req=%p\n", request);
    return TRUE;
}

static Feature g_ohos_s23_feature = {
    .GetName = OhosLiteosS23FeatureGetName,
    .OnInitialize = OhosLiteosS23FeatureInit,
    .OnStop = OhosLiteosS23FeatureStop,
    .OnMessage = OhosLiteosS23FeatureMsg,
};

static int g_ohos_s24_ref = 1;

static int OhosLiteosS24QueryInterface(IUnknown *iUnknown, int version, void **target)
{
    (void)version;

    if (iUnknown == NULL || target == NULL) {
        return EC_INVALID;
    }

    *target = iUnknown;
    return EC_SUCCESS;
}

static int OhosLiteosS24AddRef(IUnknown *iUnknown)
{
    if (iUnknown == NULL) {
        return EC_INVALID;
    }

    g_ohos_s24_ref++;
    return g_ohos_s24_ref;
}

static int OhosLiteosS24Release(IUnknown *iUnknown)
{
    if (iUnknown == NULL) {
        return EC_INVALID;
    }

    if (g_ohos_s24_ref > 0) {
        g_ohos_s24_ref--;
    }

    return g_ohos_s24_ref;
}

static IUnknown g_ohos_s24_default_api = {
    .QueryInterface = OhosLiteosS24QueryInterface,
    .AddRef = OhosLiteosS24AddRef,
    .Release = OhosLiteosS24Release,
};

static IUnknown g_ohos_s24_feature_api = {
    .QueryInterface = OhosLiteosS24QueryInterface,
    .AddRef = OhosLiteosS24AddRef,
    .Release = OhosLiteosS24Release,
};

static volatile UINT32 g_ohos_s27_bootstrap_init_count = 0;
static volatile UINT32 g_ohos_s27_bootstrap_msg_count = 0;

static const char *OhosLiteosS26BootstrapGetName(Service *service)
{
    (void)service;
    return BOOTSTRAP_SERVICE;
}


static BOOL OhosLiteosS26BootstrapInit(Service *service, Identity identity)
{
    (void)service;
    g_ohos_s27_bootstrap_init_count++;

    esp_rom_printf("[OHOS-S26B] Bootstrap service init called count=%u serviceId=%d featureId=%d queue=%p\n",
                   g_ohos_s27_bootstrap_init_count,
                   identity.serviceId,
                   identity.featureId,
                   identity.queueId);
    return TRUE;
}


static BOOL OhosLiteosS26BootstrapMsg(Service *service, Request *request)
{
    (void)service;
    g_ohos_s27_bootstrap_msg_count++;

    esp_rom_printf("[OHOS-S26B] Bootstrap service message called count=%u req=%p\n",
                   g_ohos_s27_bootstrap_msg_count,
                   request);
    return TRUE;
}

static TaskConfig OhosLiteosS26BootstrapTaskConfig(Service *service)
{
    (void)service;

    /*
     * S26-B only registers Bootstrap service object.
     * Do not rely on this config to start task pool in this step.
     */
    TaskConfig config = {
        .level = LEVEL_HIGH,
        .priority = PRI_NORMAL,
        .stackSize = 0x800,
        .queueSize = 2,
        .taskFlags = NO_TASK,
    };

    return config;
}

static Service g_ohos_s26_bootstrap_service = {
    .GetName = OhosLiteosS26BootstrapGetName,
    .Initialize = OhosLiteosS26BootstrapInit,
    .MessageHandle = OhosLiteosS26BootstrapMsg,
    .GetTaskConfig = OhosLiteosS26BootstrapTaskConfig,
};

static volatile UINT32 g_ohos_s28_runtime_started = 0;
static volatile UINT32 g_ohos_s28_thread_enter_count = 0;
static volatile UINT32 g_ohos_s28_thread_arg_ok = 0;
static volatile UINT32 g_ohos_s28_thread_local_ok = 0;

static void *OhosLiteosS28ThreadRunnable(void *argv)
{
    g_ohos_s28_thread_enter_count++;

    void *localBefore = THREAD_GetThreadLocal();
    THREAD_SetThreadLocal(argv);
    void *localAfter = THREAD_GetThreadLocal();

    g_ohos_s28_thread_arg_ok = (argv == (void *)0x28282828);
    g_ohos_s28_thread_local_ok = (localBefore == argv) && (localAfter == argv);

    esp_rom_printf("[OHOS-S28A-SAFE] THREAD runnable entered count=%u argv=%p localBefore=%p localAfter=%p argOk=%u localOk=%u\n",
                   g_ohos_s28_thread_enter_count,
                   argv,
                   localBefore,
                   localAfter,
                   g_ohos_s28_thread_arg_ok,
                   g_ohos_s28_thread_local_ok);

    return NULL;
}

static VOID OhosLiteosThreadCreateRuntimeSmoke(void)
{
    if (g_ohos_s28_runtime_started != 0) {
        return;
    }
    g_ohos_s28_runtime_started = 1;

    UINT32 enterBefore = g_ohos_s28_thread_enter_count;
    int totalBefore = THREAD_Total();

    ThreadAttr attr = {
        .name = "s28_runtime",
        .stackSize = OHOS_SMOKE_TASK_STACK_SIZE,
        .priority = 4,
        .reserved1 = 0,
        .reserved2 = 0,
    };

    ThreadId tid = THREAD_Create(OhosLiteosS28ThreadRunnable, (void *)0x28282828, &attr);

    for (int i = 0; i < 100; ++i) {
        if (g_ohos_s28_thread_enter_count > enterBefore) {
            break;
        }
        LOS_TaskDelay(1);
    }

    int totalAfter = THREAD_Total();

    BOOL createOk = (tid != NULL);
    BOOL enterOk = (g_ohos_s28_thread_enter_count == enterBefore + 1);
    BOOL argOk = (g_ohos_s28_thread_arg_ok == TRUE);
    BOOL localOk = (g_ohos_s28_thread_local_ok == TRUE);
    BOOL totalOk = (totalAfter == totalBefore + 1);

    BOOL ok = createOk && enterOk && argOk && localOk && totalOk;

    esp_rom_printf("[OHOS-S28A-SAFE] THREAD_Create runtime smoke create=%u enter=%u arg=%u local=%u totalBefore=%d totalAfter=%d tid=%p ok=%u\n",
                   createOk,
                   enterOk,
                   argOk,
                   localOk,
                   totalBefore,
                   totalAfter,
                   (void *)tid,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "THREAD_Create runtime smoke ok=%u create=%u enter=%u total=%d",
               ok, createOk, enterOk, totalAfter);
}

static volatile UINT32 g_ohos_s28b_started = 0;
static volatile UINT32 g_ohos_s28b_direct_count = 0;
static volatile UINT32 g_ohos_s28b_direct_msg_ok = 0;

static void OhosLiteosS28BDirectHandler(const Request *request, const Response *response)
{
    (void)response;

    g_ohos_s28b_direct_count++;

    if (request != NULL &&
        request->msgId == 28 &&
        request->msgValue == 0x28280002) {
        g_ohos_s28b_direct_msg_ok = TRUE;
    }

    esp_rom_printf("[OHOS-S28B] TaskPool direct handler count=%u req=%p msgId=%d msgValue=0x%x msgOk=%u\n",
                   g_ohos_s28b_direct_count,
                   request,
                   request ? request->msgId : -1,
                   request ? request->msgValue : 0,
                   g_ohos_s28b_direct_msg_ok);
}

static VOID OhosLiteosSamgrTaskPoolStandaloneSmoke(void)
{
    if (g_ohos_s28b_started != 0) {
        return;
    }
    g_ohos_s28b_started = 1;

    UINT32 directBefore = g_ohos_s28b_direct_count;
    int totalBefore = THREAD_Total();

    TaskConfig config = {
        .level = LEVEL_HIGH,
        .priority = 5,
        .stackSize = OHOS_SMOKE_TASK_STACK_SIZE,
        .queueSize = 4,
        .taskFlags = SINGLE_TASK,
    };

    TaskPool *pool = SAMGR_CreateFixedTaskPool(&config, "s28b_pool", 1);

    BOOL createPoolOk = (pool != NULL);
    BOOL queueOk = (pool != NULL && pool->queueId != NULL);

    int32 startRet = EC_INVALID;
    BOOL startOk = FALSE;
    BOOL topOk = FALSE;

    if (pool != NULL) {
        startRet = SAMGR_StartTaskPool(pool, "s28b_pool");
        startOk = (startRet == EC_SUCCESS);
        topOk = (pool->top == 1);
    }

    int totalAfterStart = THREAD_Total();
    BOOL totalIncOk = (totalAfterStart == totalBefore + 1);

    int32 directRet = EC_INVALID;
    BOOL directSendOk = FALSE;
    BOOL directHandledOk = FALSE;

    if (pool != NULL && pool->queueId != NULL) {
        Identity id = {
            .serviceId = INVALID_INDEX,
            .featureId = INVALID_INDEX,
            .queueId = pool->queueId,
        };

        Request req = {
            .msgId = 28,
            .len = 0,
            .data = NULL,
            .msgValue = 0x28280002,
        };

        uint32 *ref = NULL;
        directRet = SAMGR_SendSharedDirectRequest(&id, &req, NULL, &ref, OhosLiteosS28BDirectHandler);
        directSendOk = (directRet == EC_SUCCESS);

        for (int i = 0; i < 100; ++i) {
            if (g_ohos_s28b_direct_count > directBefore) {
                break;
            }
            LOS_TaskDelay(1);
        }

        directHandledOk =
            (g_ohos_s28b_direct_count == directBefore + 1) &&
            (g_ohos_s28b_direct_msg_ok == TRUE);
    }

    int32 releaseRet = EC_INVALID;
    BOOL releaseOk = FALSE;

    if (pool != NULL) {
        releaseRet = SAMGR_ReleaseTaskPool(pool);
        releaseOk = (releaseRet == EC_SUCCESS);
    }

    for (int i = 0; i < 20; ++i) {
        LOS_TaskDelay(1);
    }

    BOOL ok = createPoolOk &&
              queueOk &&
              startOk &&
              topOk &&
              totalIncOk &&
              directSendOk &&
              directHandledOk &&
              releaseOk;

    esp_rom_printf("[OHOS-S28B] TaskPool standalone smoke create=%u queue=%u start=%u top=%u totalBefore=%d totalAfterStart=%d totalInc=%u directRet=%d directSend=%u directHandled=%u releaseRet=%d release=%u ok=%u\n",
                   createPoolOk,
                   queueOk,
                   startOk,
                   topOk,
                   totalBefore,
                   totalAfterStart,
                   totalIncOk,
                   directRet,
                   directSendOk,
                   directHandledOk,
                   releaseRet,
                   releaseOk,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "TaskPool standalone smoke ok=%u create=%u start=%u direct=%u release=%u",
               ok, createPoolOk, startOk, directHandledOk, releaseOk);
}

static VOID OhosLiteosSamgrBootstrapRepeatNoTaskSmoke(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL apiBeforeOk = FALSE;
    BOOL secondBootstrapCalledOk = FALSE;
    BOOL initCalledAgainOk = FALSE;
    BOOL msgStillNotCalledOk = FALSE;
    BOOL apiAfterOk = FALSE;

    UINT32 initBefore = g_ohos_s27_bootstrap_init_count;
    UINT32 msgBefore = g_ohos_s27_bootstrap_msg_count;

    if (samgr != NULL) {
        apiBeforeOk =
            (samgr->GetDefaultFeatureApi(BOOTSTRAP_SERVICE) == &g_ohos_s24_default_api);

        /*
         * Diagnostic only:
         * Call SAMGR_Bootstrap again with NO_TASK Bootstrap service.
         * Current expected behavior: Bootstrap Initialize is called again,
         * but MessageHandle is still not called.
         */
        SAMGR_Bootstrap();
        secondBootstrapCalledOk = TRUE;

        initCalledAgainOk = (g_ohos_s27_bootstrap_init_count == initBefore + 1);
        msgStillNotCalledOk = (g_ohos_s27_bootstrap_msg_count == msgBefore);

        apiAfterOk =
            (samgr->GetDefaultFeatureApi(BOOTSTRAP_SERVICE) == &g_ohos_s24_default_api);
    }

    BOOL ok = instOk &&
              apiBeforeOk &&
              secondBootstrapCalledOk &&
              initCalledAgainOk &&
              msgStillNotCalledOk &&
              apiAfterOk;

    esp_rom_printf("[OHOS-S27B] bootstrap repeat NO_TASK smoke inst=%u apiBefore=%u called2=%u initAgain=%u msgStill0=%u apiAfter=%u initBefore=%u initNow=%u msgBefore=%u msgNow=%u ok=%u\n",
                   instOk,
                   apiBeforeOk,
                   secondBootstrapCalledOk,
                   initCalledAgainOk,
                   msgStillNotCalledOk,
                   apiAfterOk,
                   initBefore,
                   g_ohos_s27_bootstrap_init_count,
                   msgBefore,
                   g_ohos_s27_bootstrap_msg_count,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "bootstrap repeat NO_TASK smoke ok=%u initNow=%u msgNow=%u",
               ok, g_ohos_s27_bootstrap_init_count, g_ohos_s27_bootstrap_msg_count);
}

static VOID OhosLiteosSamgrBootstrapNoTaskSmoke(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL bootApiBeforeOk = FALSE;
    BOOL bootstrapCalledOk = FALSE;
    BOOL initCalledOk = FALSE;
    BOOL msgNotCalledOk = FALSE;
    BOOL bootApiAfterOk = FALSE;

    UINT32 initBefore = g_ohos_s27_bootstrap_init_count;
    UINT32 msgBefore = g_ohos_s27_bootstrap_msg_count;

    if (samgr != NULL) {
        bootApiBeforeOk =
            (samgr->GetDefaultFeatureApi(BOOTSTRAP_SERVICE) == &g_ohos_s24_default_api);

        /*
         * Controlled Bootstrap call:
         * Bootstrap service uses NO_TASK, so this should only exercise DEFAULT_Initialize path.
         * It must not rely on real THREAD_Create / task pool startup.
         */
        SAMGR_Bootstrap();
        bootstrapCalledOk = TRUE;

        initCalledOk = (g_ohos_s27_bootstrap_init_count == initBefore + 1);
        msgNotCalledOk = (g_ohos_s27_bootstrap_msg_count == msgBefore);

        bootApiAfterOk =
            (samgr->GetDefaultFeatureApi(BOOTSTRAP_SERVICE) == &g_ohos_s24_default_api);
    }

    BOOL ok = instOk &&
              bootApiBeforeOk &&
              bootstrapCalledOk &&
              initCalledOk &&
              msgNotCalledOk &&
              bootApiAfterOk;

    esp_rom_printf("[OHOS-S27A] bootstrap NO_TASK smoke inst=%u apiBefore=%u called=%u initCalled=%u msgNotCalled=%u apiAfter=%u initCnt=%u msgCnt=%u ok=%u\n",
                   instOk,
                   bootApiBeforeOk,
                   bootstrapCalledOk,
                   initCalledOk,
                   msgNotCalledOk,
                   bootApiAfterOk,
                   g_ohos_s27_bootstrap_init_count,
                   g_ohos_s27_bootstrap_msg_count,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "bootstrap NO_TASK smoke ok=%u init=%u msg=%u",
               ok, g_ohos_s27_bootstrap_init_count, g_ohos_s27_bootstrap_msg_count);
}

static volatile UINT32 g_ohos_s29_runtime_checked = 0;
static volatile UINT32 g_ohos_s29_init_count = 0;
static volatile UINT32 g_ohos_s29_msg_count = 0;
static volatile UINT32 g_ohos_s29_init_queue_ok = 0;
static volatile UINT32 g_ohos_s29_init_service_id = 0xFFFFFFFFU;
static volatile UINT32 g_ohos_s29_init_feature_id = 0xFFFFFFFFU;
static MQueueId g_ohos_s29_init_queue_id = NULL;
static volatile UINT32 g_ohos_s30_msg_ok = 0;
static volatile UINT32 g_ohos_s31_msg_ok = 0;
static volatile UINT32 g_ohos_s31_resp_send_ok = 0;
static volatile UINT32 g_ohos_s31_resp_handler_count = 0;
static volatile UINT32 g_ohos_s31_resp_handler_ok = 0;

static const char *OhosLiteosS29GetName(Service *service)
{
    (void)service;
    return "S29SingleTask";
}

static BOOL OhosLiteosS29Init(Service *service, Identity identity)
{
    (void)service;

    g_ohos_s29_init_count++;
    g_ohos_s29_init_queue_ok = (identity.queueId != NULL);
    g_ohos_s29_init_queue_id = identity.queueId;
    g_ohos_s29_init_service_id = (UINT32)identity.serviceId;
    g_ohos_s29_init_feature_id = (UINT32)identity.featureId;

    esp_rom_printf("[OHOS-S29] SINGLE_TASK service init count=%u serviceId=%d featureId=%d queue=%p queueOk=%u\n",
                   g_ohos_s29_init_count,
                   identity.serviceId,
                   identity.featureId,
                   identity.queueId,
                   g_ohos_s29_init_queue_ok);
    return TRUE;
}



static volatile UINT32 g_ohos_s31b_target_msg_ok = 0;
static volatile UINT32 g_ohos_s31b_target_resp_ok = 0;
static volatile UINT32 g_ohos_s32_shared_send_ok = 0;
static volatile UINT32 g_ohos_s32_shared_token_ok = 0;
static volatile UINT32 g_ohos_s32_shared_msg_ok = 0;
static volatile UINT32 g_ohos_s32_shared_data_ok = 0;
static volatile UINT32 g_ohos_s32b_direct_send_ok = 0;
static volatile UINT32 g_ohos_s32b_direct_token_ok = 0;
static volatile UINT32 g_ohos_s32b_direct_handler_count = 0;
static volatile UINT32 g_ohos_s32b_direct_req_ok = 0;
static volatile UINT32 g_ohos_s32b_direct_resp_ok = 0;


static BOOL OhosLiteosS29Msg(Service *service, Request *request)
{
    (void)service;

    g_ohos_s29_msg_count++;

    if (request != NULL &&
        request->msgId == 30 &&
        request->msgValue == 0x30300030) {
        g_ohos_s30_msg_ok = TRUE;
    }

    if (request != NULL &&
        request->msgId == 31 &&
        request->msgValue == 0x31310031) {
        g_ohos_s31_msg_ok = TRUE;

        Response resp = {
            .data = NULL,
            .len = 0,
            .reply = (void *)0x31313131,
        };

        int32 respRet = SAMGR_SendResponse(request, &resp);
        g_ohos_s31_resp_send_ok = (respRet == EC_SUCCESS);

        esp_rom_printf("[OHOS-S31A] SINGLE_TASK service response sent respRet=%d respOk=%u reply=%p\n",
                       respRet,
                       g_ohos_s31_resp_send_ok,
                       resp.reply);
    }

    if (request != NULL &&
        request->msgId == 32 &&
        request->msgValue == 0x31310032) {
        g_ohos_s31b_target_msg_ok = TRUE;

        Response resp = {
            .data = NULL,
            .len = 0,
            .reply = (void *)0x31313232,
        };

        int32 respRet = SAMGR_SendResponse(request, &resp);
        g_ohos_s31b_target_resp_ok = (respRet == EC_SUCCESS);

        esp_rom_printf("[OHOS-S31B] target service response sent respRet=%d respOk=%u reply=%p\n",
                       respRet,
                       g_ohos_s31b_target_resp_ok,
                       resp.reply);
    }

    if (request != NULL &&
        request->msgId == 33 &&
        request->msgValue == 0x32330033) {
        g_ohos_s32_shared_msg_ok = TRUE;

        if (request->data != NULL &&
            request->len == (int16)sizeof(UINT32) &&
            *((UINT32 *)request->data) == 0x32323333U) {
            g_ohos_s32_shared_data_ok = TRUE;
        }

        esp_rom_printf("[OHOS-S32A] SharedRequest received req=%p data=%p len=%d msgValue=0x%x msgOk=%u dataOk=%u\n",
                       request,
                       request->data,
                       request->len,
                       request->msgValue,
                       g_ohos_s32_shared_msg_ok,
                       g_ohos_s32_shared_data_ok);
    }

    esp_rom_printf("[OHOS-S30/S31/S32] SINGLE_TASK service message count=%u req=%p msgId=%d msgValue=0x%x msg30Ok=%u msg31Ok=%u msg32Ok=%u msg33Ok=%u\n",
                   g_ohos_s29_msg_count,
                   request,
                   request ? request->msgId : -1,
                   request ? request->msgValue : 0,
                   g_ohos_s30_msg_ok,
                   g_ohos_s31_msg_ok,
                   g_ohos_s31b_target_msg_ok,
                   g_ohos_s32_shared_msg_ok);
    return TRUE;
}

static TaskConfig OhosLiteosS29TaskConfig(Service *service)
{
    (void)service;

    TaskConfig config = {
        .level = LEVEL_HIGH,
        .priority = 9,
        .stackSize = OHOS_SMOKE_TASK_STACK_SIZE,
        .queueSize = 4,
        .taskFlags = SINGLE_TASK,
    };

    return config;
}

static Service g_ohos_s29_single_task_service = {
    .GetName = OhosLiteosS29GetName,
    .Initialize = OhosLiteosS29Init,
    .MessageHandle = OhosLiteosS29Msg,
    .GetTaskConfig = OhosLiteosS29TaskConfig,
};

static volatile UINT32 g_ohos_s31b_caller_init_count = 0;
static volatile UINT32 g_ohos_s31b_caller_queue_ok = 0;
static volatile UINT32 g_ohos_s31b_caller_service_id = 0xFFFFFFFFU;
static volatile UINT32 g_ohos_s31b_send_ret_ok = 0;
static volatile UINT32 g_ohos_s31b_handler_count = 0;
static volatile UINT32 g_ohos_s31b_handler_req_ok = 0;
static volatile UINT32 g_ohos_s31b_handler_resp_ok = 0;
static volatile UINT32 g_ohos_s31b_handler_queue_ok = 0;
static volatile UINT32 g_ohos_s31b_runtime_checked = 0;
static MQueueId g_ohos_s31b_caller_queue_id = NULL;

static void OhosLiteosS31BResponseHandler(const Request *request, const Response *response)
{
    g_ohos_s31b_handler_count++;

    MQueueId currentQueue = SAMGR_GetCurrentQueueID();

    BOOL reqOk = (request != NULL &&
                  request->msgId == 32 &&
                  request->msgValue == 0x31310032);

    BOOL respOk = (response != NULL &&
                   response->data == NULL &&
                   response->len == 0 &&
                   response->reply == (void *)0x31313232);

    BOOL queueOk = (currentQueue != NULL &&
                    g_ohos_s31b_caller_queue_id != NULL &&
                    currentQueue == g_ohos_s31b_caller_queue_id);

    g_ohos_s31b_handler_req_ok = reqOk;
    g_ohos_s31b_handler_resp_ok = respOk;
    g_ohos_s31b_handler_queue_ok = queueOk;

    esp_rom_printf("[OHOS-S31B] ACK handler count=%u reqOk=%u respOk=%u queueOk=%u currentQ=%p callerQ=%p reply=%p\n",
                   g_ohos_s31b_handler_count,
                   reqOk,
                   respOk,
                   queueOk,
                   currentQueue,
                   g_ohos_s31b_caller_queue_id,
                   response ? response->reply : NULL);
}

static const char *OhosLiteosS31BCallerGetName(Service *service)
{
    (void)service;
    return "S31BCaller";
}

static BOOL OhosLiteosS31BCallerInit(Service *service, Identity identity)
{
    (void)service;

    g_ohos_s31b_caller_init_count++;
    g_ohos_s31b_caller_queue_id = identity.queueId;
    g_ohos_s31b_caller_queue_ok = (identity.queueId != NULL);
    g_ohos_s31b_caller_service_id = (UINT32)identity.serviceId;

    for (int i = 0; i < 200; ++i) {
        if (g_ohos_s29_init_queue_id != NULL &&
            g_ohos_s29_init_service_id != 0xFFFFFFFFU) {
            break;
        }
        LOS_TaskDelay(1);
    }

    BOOL targetReady = (g_ohos_s29_init_queue_id != NULL &&
                        g_ohos_s29_init_service_id != 0xFFFFFFFFU);

    int32 sendRet = EC_INVALID;
    if (targetReady) {
        Identity target = {
            .serviceId = (int16)g_ohos_s29_init_service_id,
            .featureId = INVALID_INDEX,
            .queueId = g_ohos_s29_init_queue_id,
        };

        Request req = {
            .msgId = 32,
            .len = 0,
            .data = NULL,
            .msgValue = 0x31310032,
        };

        sendRet = SAMGR_SendRequest(&target, &req, OhosLiteosS31BResponseHandler);
        g_ohos_s31b_send_ret_ok = (sendRet == EC_SUCCESS);
    }

    esp_rom_printf("[OHOS-S31B] Caller init count=%u serviceId=%d queue=%p queueOk=%u targetReady=%u sendRet=%d sendOk=%u\n",
                   g_ohos_s31b_caller_init_count,
                   identity.serviceId,
                   identity.queueId,
                   g_ohos_s31b_caller_queue_ok,
                   targetReady,
                   sendRet,
                   g_ohos_s31b_send_ret_ok);

    return TRUE;
}

static BOOL OhosLiteosS31BCallerMsg(Service *service, Request *request)
{
    (void)service;
    (void)request;
    return TRUE;
}

static TaskConfig OhosLiteosS31BCallerTaskConfig(Service *service)
{
    (void)service;

    TaskConfig config = {
        .level = LEVEL_HIGH,
        .priority = 9,
        .stackSize = OHOS_SMOKE_TASK_STACK_SIZE,
        .queueSize = 4,
        .taskFlags = SINGLE_TASK,
    };

    return config;
}

static Service g_ohos_s31b_caller_service = {
    .GetName = OhosLiteosS31BCallerGetName,
    .Initialize = OhosLiteosS31BCallerInit,
    .MessageHandle = OhosLiteosS31BCallerMsg,
    .GetTaskConfig = OhosLiteosS31BCallerTaskConfig,
};

static VOID OhosLiteosSamgrRegisterAckCallerServiceSmoke(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL regOk = FALSE;
    BOOL dupOk = FALSE;

    if (samgr != NULL) {
        regOk = (samgr->RegisterService(&g_ohos_s31b_caller_service) == TRUE);
        dupOk = (samgr->RegisterService(&g_ohos_s31b_caller_service) == FALSE);
    }

    BOOL ok = instOk && regOk && dupOk;

    esp_rom_printf("[OHOS-S31B] register ACK caller service smoke inst=%u reg=%u dup=%u ok=%u\n",
                   instOk, regOk, dupOk, ok);

    HILOG_INFO(HILOG_MODULE_INIT, "register ACK caller service smoke ok=%u reg=%u dup=%u",
               ok, regOk, dupOk);
}

static VOID OhosLiteosS31BTrueAckRuntimeVerifySmoke(void)
{
    if (g_ohos_s31b_runtime_checked != 0) {
        return;
    }
    g_ohos_s31b_runtime_checked = 1;

    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL callerInitOk = FALSE;
    BOOL callerQueueOk = FALSE;
    BOOL sendOk = FALSE;
    BOOL targetMsgOk = FALSE;
    BOOL targetRespOk = FALSE;
    BOOL handlerCalledOk = FALSE;
    BOOL handlerReqOk = FALSE;
    BOOL handlerRespOk = FALSE;
    BOOL handlerQueueOk = FALSE;
    BOOL unregCallerOk = FALSE;

    for (int i = 0; i < 300; ++i) {
        if (g_ohos_s31b_handler_count > 0) {
            break;
        }
        LOS_TaskDelay(1);
    }

    callerInitOk = (g_ohos_s31b_caller_init_count == 1);
    callerQueueOk = (g_ohos_s31b_caller_queue_ok == TRUE);
    sendOk = (g_ohos_s31b_send_ret_ok == TRUE);
    targetMsgOk = (g_ohos_s31b_target_msg_ok == TRUE);
    targetRespOk = (g_ohos_s31b_target_resp_ok == TRUE);
    handlerCalledOk = (g_ohos_s31b_handler_count == 1);
    handlerReqOk = (g_ohos_s31b_handler_req_ok == TRUE);
    handlerRespOk = (g_ohos_s31b_handler_resp_ok == TRUE);
    handlerQueueOk = (g_ohos_s31b_handler_queue_ok == TRUE);

    if (samgr != NULL) {
        Service *caller = samgr->UnregisterService("S31BCaller");
        unregCallerOk = (caller == &g_ohos_s31b_caller_service);
    }

    for (int i = 0; i < 20; ++i) {
        LOS_TaskDelay(1);
    }

    BOOL ok = instOk &&
              callerInitOk &&
              callerQueueOk &&
              sendOk &&
              targetMsgOk &&
              targetRespOk &&
              handlerCalledOk &&
              handlerReqOk &&
              handlerRespOk &&
              handlerQueueOk &&
              unregCallerOk;

    esp_rom_printf("[OHOS-S31B] TRUE ACK queue verify inst=%u callerInit=%u callerQueue=%u send=%u targetMsg=%u targetResp=%u handlerCalled=%u handlerReq=%u handlerResp=%u handlerQueue=%u unregCaller=%u handlerCnt=%u ok=%u\n",
                   instOk,
                   callerInitOk,
                   callerQueueOk,
                   sendOk,
                   targetMsgOk,
                   targetRespOk,
                   handlerCalledOk,
                   handlerReqOk,
                   handlerRespOk,
                   handlerQueueOk,
                   unregCallerOk,
                   g_ohos_s31b_handler_count,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "TRUE ACK queue verify ok=%u send=%u handler=%u queue=%u",
               ok, sendOk, handlerCalledOk, handlerQueueOk);
}

static VOID OhosLiteosSamgrRegisterSingleTaskServiceSmoke(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL regOk = FALSE;
    BOOL dupOk = FALSE;
    BOOL apiBeforeOk = FALSE;

    if (samgr != NULL) {
        regOk = (samgr->RegisterService(&g_ohos_s29_single_task_service) == TRUE);
        dupOk = (samgr->RegisterService(&g_ohos_s29_single_task_service) == FALSE);
        apiBeforeOk = (samgr->GetDefaultFeatureApi("S29SingleTask") == NULL);
    }

    BOOL ok = instOk && regOk && dupOk && apiBeforeOk;

    esp_rom_printf("[OHOS-S29] register SINGLE_TASK service smoke inst=%u reg=%u dup=%u apiBefore=%u ok=%u\n",
                   instOk, regOk, dupOk, apiBeforeOk, ok);

    HILOG_INFO(HILOG_MODULE_INIT, "register SINGLE_TASK service smoke ok=%u reg=%u dup=%u",
               ok, regOk, dupOk);
}

static void OhosLiteosS32BSharedDirectHandler(const Request *request, const Response *response)
{
    g_ohos_s32b_direct_handler_count++;

    BOOL reqOk = FALSE;
    BOOL respOk = FALSE;

    if (request != NULL &&
        request->msgId == 34 &&
        request->msgValue == 0x32330034 &&
        request->data != NULL &&
        request->len == (int16)sizeof(UINT32) &&
        *((UINT32 *)request->data) == 0x32324434U) {
        reqOk = TRUE;
    }

    if (response != NULL &&
        response->data != NULL &&
        response->len == (int16)sizeof(UINT32) &&
        *((UINT32 *)response->data) == 0x32325234U &&
        response->reply == (void *)0x32323232) {
        respOk = TRUE;
    }

    g_ohos_s32b_direct_req_ok = reqOk;
    g_ohos_s32b_direct_resp_ok = respOk;

    esp_rom_printf("[OHOS-S32B] SharedDirect handler count=%u reqOk=%u respOk=%u reqData=%p respData=%p reply=%p\n",
                   g_ohos_s32b_direct_handler_count,
                   reqOk,
                   respOk,
                   request ? request->data : NULL,
                   response ? response->data : NULL,
                   response ? response->reply : NULL);
}

static void OhosLiteosS31ResponseHandler(const Request *request, const Response *response)
{
    g_ohos_s31_resp_handler_count++;

    BOOL reqOk = (request != NULL &&
                  request->msgId == 31 &&
                  request->msgValue == 0x31310031);

    BOOL respOk = (response != NULL &&
                   response->data == NULL &&
                   response->len == 0 &&
                   response->reply == (void *)0x31313131);

    g_ohos_s31_resp_handler_ok = (reqOk && respOk);

    esp_rom_printf("[OHOS-S31A] response handler count=%u reqOk=%u respOk=%u reply=%p ok=%u\n",
                   g_ohos_s31_resp_handler_count,
                   reqOk,
                   respOk,
                   response ? response->reply : NULL,
                   g_ohos_s31_resp_handler_ok);
}

static VOID OhosLiteosSamgrSingleTaskRuntimeVerifySmoke(void)
{
    if (g_ohos_s29_runtime_checked != 0) {
        return;
    }
    g_ohos_s29_runtime_checked = 1;

    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL initOk = FALSE;
    BOOL queueOk = FALSE;
    BOOL msgBeforeOk = FALSE;
    BOOL sendReqOk = FALSE;
    BOOL msgCalledOk = FALSE;
    BOOL msgContentOk = FALSE;
    BOOL unregisterOk = FALSE;
    BOOL afterUnregisterOk = FALSE;

    for (int i = 0; i < 200; ++i) {
        if (g_ohos_s29_init_count > 0) {
            break;
        }
        LOS_TaskDelay(1);
    }

    initOk = (g_ohos_s29_init_count >= 1);
    queueOk = (g_ohos_s29_init_queue_ok == TRUE) && (g_ohos_s29_init_queue_id != NULL);
    msgBeforeOk = TRUE; /* S31B may already send one message before S31A baseline */

    UINT32 msgBefore = g_ohos_s29_msg_count;

    if (samgr != NULL && queueOk) {
        Identity id = {
            .serviceId = (int16)g_ohos_s29_init_service_id,
            .featureId = INVALID_INDEX,
            .queueId = g_ohos_s29_init_queue_id,
        };

        Request req = {
            .msgId = 30,
            .len = 0,
            .data = NULL,
            .msgValue = 0x30300030,
        };

        int32 sendRet = SAMGR_SendRequest(&id, &req, NULL);
        sendReqOk = (sendRet == EC_SUCCESS);

        for (int i = 0; i < 200; ++i) {
            if (g_ohos_s29_msg_count > msgBefore) {
                break;
            }
            LOS_TaskDelay(1);
        }

        msgCalledOk = (g_ohos_s29_msg_count == msgBefore + 1);
        msgContentOk = (g_ohos_s30_msg_ok == TRUE);
    }

    BOOL respSendReqOk = FALSE;
    BOOL respMsgCalledOk = FALSE;
    BOOL respMsgContentOk = FALSE;
    BOOL respSentOk = FALSE;
    BOOL respHandlerCalledOk = FALSE;
    BOOL respHandlerContentOk = FALSE;

    UINT32 msgBeforeResp = g_ohos_s29_msg_count;
    UINT32 handlerBefore = g_ohos_s31_resp_handler_count;

    if (samgr != NULL && queueOk) {
        Identity id = {
            .serviceId = (int16)g_ohos_s29_init_service_id,
            .featureId = INVALID_INDEX,
            .queueId = g_ohos_s29_init_queue_id,
        };

        Request req = {
            .msgId = 31,
            .len = 0,
            .data = NULL,
            .msgValue = 0x31310031,
        };

        int32 sendRet = SAMGR_SendRequest(&id, &req, OhosLiteosS31ResponseHandler);
        respSendReqOk = (sendRet == EC_SUCCESS);

        for (int i = 0; i < 200; ++i) {
            if (g_ohos_s29_msg_count > msgBeforeResp &&
                g_ohos_s31_resp_handler_count > handlerBefore) {
                break;
            }
            LOS_TaskDelay(1);
        }

        respMsgCalledOk = (g_ohos_s29_msg_count == msgBeforeResp + 1);
        respMsgContentOk = (g_ohos_s31_msg_ok == TRUE);
        respSentOk = (g_ohos_s31_resp_send_ok == TRUE);
        respHandlerCalledOk = (g_ohos_s31_resp_handler_count == handlerBefore + 1);
        respHandlerContentOk = (g_ohos_s31_resp_handler_ok == TRUE);
    }


    BOOL sharedAllocOk = FALSE;
    BOOL sharedSendOk = FALSE;
    BOOL sharedTokenOk = FALSE;
    BOOL sharedMsgOk = FALSE;
    BOOL sharedDataOk = FALSE;

    if (samgr != NULL && queueOk) {
        UINT32 *payload = (UINT32 *)SAMGR_Malloc(sizeof(UINT32));
        sharedAllocOk = (payload != NULL);

        if (payload != NULL) {
            *payload = 0x32323333U;

            Identity id = {
                .serviceId = (int16)g_ohos_s29_init_service_id,
                .featureId = INVALID_INDEX,
                .queueId = g_ohos_s29_init_queue_id,
            };

            Request req = {
                .msgId = 33,
                .len = (int16)sizeof(UINT32),
                .data = payload,
                .msgValue = 0x32330033,
            };

            UINT32 msgBeforeShared = g_ohos_s29_msg_count;

            uint32 *token = SAMGR_SendSharedRequest(&id, &req, NULL, NULL);
            sharedTokenOk = (token != NULL);
            sharedSendOk = sharedTokenOk;
            g_ohos_s32_shared_send_ok = sharedSendOk;
            g_ohos_s32_shared_token_ok = sharedTokenOk;

            if (!sharedSendOk) {
                SAMGR_Free(payload);
            } else {
                for (int i = 0; i < 200; ++i) {
                    if (g_ohos_s29_msg_count > msgBeforeShared) {
                        break;
                    }
                    LOS_TaskDelay(1);
                }
            }

            sharedMsgOk = (g_ohos_s32_shared_msg_ok == TRUE);
            sharedDataOk = (g_ohos_s32_shared_data_ok == TRUE);
        }
    }

    BOOL s32Ok = sharedAllocOk &&
                 sharedSendOk &&
                 sharedTokenOk &&
                 sharedMsgOk &&
                 sharedDataOk;

    esp_rom_printf("[OHOS-S32A] SharedRequest verify alloc=%u send=%u token=%u msg=%u data=%u msgCnt=%u ok=%u\n",
                   sharedAllocOk,
                   sharedSendOk,
                   sharedTokenOk,
                   sharedMsgOk,
                   sharedDataOk,
                   g_ohos_s29_msg_count,
                   s32Ok);

    HILOG_INFO(HILOG_MODULE_INIT, "SharedRequest verify ok=%u send=%u msg=%u data=%u",
               s32Ok, sharedSendOk, sharedMsgOk, sharedDataOk);


    BOOL directAllocOk = FALSE;
    BOOL directSendOk = FALSE;
    BOOL directTokenOk = FALSE;
    BOOL directHandledOk = FALSE;
    BOOL directReqOk = FALSE;
    BOOL directRespOk = FALSE;

    if (samgr != NULL && queueOk) {
        UINT32 *reqPayload = (UINT32 *)SAMGR_Malloc(sizeof(UINT32));
        UINT32 *respPayload = (UINT32 *)SAMGR_Malloc(sizeof(UINT32));

        directAllocOk = (reqPayload != NULL && respPayload != NULL);

        if (directAllocOk) {
            *reqPayload = 0x32324434U;
            *respPayload = 0x32325234U;

            Identity id = {
                .serviceId = (int16)g_ohos_s29_init_service_id,
                .featureId = INVALID_INDEX,
                .queueId = g_ohos_s29_init_queue_id,
            };

            Request req = {
                .msgId = 34,
                .len = (int16)sizeof(UINT32),
                .data = reqPayload,
                .msgValue = 0x32330034,
            };

            Response resp = {
                .data = respPayload,
                .len = (int16)sizeof(UINT32),
                .reply = (void *)0x32323232,
            };

            UINT32 handlerBefore = g_ohos_s32b_direct_handler_count;
            uint32 *ref = NULL;

            int32 directRet = SAMGR_SendSharedDirectRequest(
                &id,
                &req,
                &resp,
                &ref,
                OhosLiteosS32BSharedDirectHandler
            );

            directSendOk = (directRet == EC_SUCCESS);
            directTokenOk = (ref != NULL);

            g_ohos_s32b_direct_send_ok = directSendOk;
            g_ohos_s32b_direct_token_ok = directTokenOk;

            if (!directSendOk) {
                SAMGR_Free(reqPayload);
                SAMGR_Free(respPayload);
            } else {
                for (int i = 0; i < 200; ++i) {
                    if (g_ohos_s32b_direct_handler_count > handlerBefore) {
                        break;
                    }
                    LOS_TaskDelay(1);
                }
            }

            directHandledOk = (g_ohos_s32b_direct_handler_count == handlerBefore + 1);
            directReqOk = (g_ohos_s32b_direct_req_ok == TRUE);
            directRespOk = (g_ohos_s32b_direct_resp_ok == TRUE);

            esp_rom_printf("[OHOS-S32B] SharedDirect send directRet=%d ref=%p\n",
                           directRet, ref);
        } else {
            if (reqPayload != NULL) {
                SAMGR_Free(reqPayload);
            }
            if (respPayload != NULL) {
                SAMGR_Free(respPayload);
            }
        }
    }

    BOOL s32bOk = directAllocOk &&
                  directSendOk &&
                  directTokenOk &&
                  directHandledOk &&
                  directReqOk &&
                  directRespOk;

    esp_rom_printf("[OHOS-S32B] SharedDirect verify alloc=%u send=%u token=%u handled=%u req=%u resp=%u handlerCnt=%u ok=%u\n",
                   directAllocOk,
                   directSendOk,
                   directTokenOk,
                   directHandledOk,
                   directReqOk,
                   directRespOk,
                   g_ohos_s32b_direct_handler_count,
                   s32bOk);

    HILOG_INFO(HILOG_MODULE_INIT, "SharedDirect verify ok=%u send=%u handled=%u req=%u resp=%u",
               s32bOk, directSendOk, directHandledOk, directReqOk, directRespOk);

    if (samgr != NULL) {
        Service *svc = samgr->UnregisterService("S29SingleTask");
        unregisterOk = (svc == &g_ohos_s29_single_task_service);

        afterUnregisterOk =
            (samgr->RegisterDefaultFeatureApi("S29SingleTask", &g_ohos_s24_default_api) == FALSE);
    }

    for (int i = 0; i < 20; ++i) {
        LOS_TaskDelay(1);
    }

    BOOL ok = instOk &&
              initOk &&
              queueOk &&
              msgBeforeOk &&
              sendReqOk &&
              msgCalledOk &&
              msgContentOk &&
              respSendReqOk &&
              respMsgCalledOk &&
              respMsgContentOk &&
              respSentOk &&
              respHandlerCalledOk &&
              respHandlerContentOk &&
              unregisterOk &&
              afterUnregisterOk;

    esp_rom_printf("[OHOS-S31A] SINGLE_TASK response verify inst=%u init=%u queue=%u s30send=%u s30msg=%u s30content=%u respSend=%u respMsg=%u respContent=%u respSent=%u handlerCalled=%u handlerContent=%u unreg=%u afterUnreg=%u initCnt=%u msgCnt=%u handlerCnt=%u ok=%u\n",
                   instOk,
                   initOk,
                   queueOk,
                   sendReqOk,
                   msgCalledOk,
                   msgContentOk,
                   respSendReqOk,
                   respMsgCalledOk,
                   respMsgContentOk,
                   respSentOk,
                   respHandlerCalledOk,
                   respHandlerContentOk,
                   unregisterOk,
                   afterUnregisterOk,
                   g_ohos_s29_init_count,
                   g_ohos_s29_msg_count,
                   g_ohos_s31_resp_handler_count,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "SINGLE_TASK response verify ok=%u respSend=%u handler=%u unreg=%u",
               ok, respSendReqOk, respHandlerCalledOk, unregisterOk);
}

static VOID OhosLiteosSamgrRegisterBootstrapServiceSmoke(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL regBootSvcOk = FALSE;
    BOOL dupBootSvcOk = FALSE;
    BOOL bootDefaultBeforeApiOk = FALSE;
    BOOL regBootApiOk = FALSE;
    BOOL dupBootApiOk = FALSE;
    BOOL getBootApiOk = FALSE;
    BOOL noBootFeatureApiOk = FALSE;
    BOOL regMissingFeatureApiFailOk = FALSE;
    BOOL bootstrapSkippedOk = TRUE;

    if (samgr != NULL) {
        regBootSvcOk = (samgr->RegisterService(&g_ohos_s26_bootstrap_service) == TRUE);

        dupBootSvcOk = (samgr->RegisterService(&g_ohos_s26_bootstrap_service) == FALSE);

        bootDefaultBeforeApiOk = (samgr->GetDefaultFeatureApi(BOOTSTRAP_SERVICE) == NULL);

        regBootApiOk =
            (samgr->RegisterDefaultFeatureApi(BOOTSTRAP_SERVICE, &g_ohos_s24_default_api) == TRUE);

        dupBootApiOk =
            (samgr->RegisterDefaultFeatureApi(BOOTSTRAP_SERVICE, &g_ohos_s24_default_api) == FALSE);

        getBootApiOk =
            (samgr->GetDefaultFeatureApi(BOOTSTRAP_SERVICE) == &g_ohos_s24_default_api);

        noBootFeatureApiOk =
            (samgr->GetFeatureApi(BOOTSTRAP_SERVICE, "NoFeature") == NULL);

        regMissingFeatureApiFailOk =
            (samgr->RegisterFeatureApi(BOOTSTRAP_SERVICE, "NoFeature", &g_ohos_s24_feature_api) == FALSE);
    }

    /*
     * Do NOT call SAMGR_Bootstrap here.
     * This step only confirms Bootstrap service can be registered into samgr table.
     */
    BOOL ok = instOk &&
              regBootSvcOk &&
              dupBootSvcOk &&
              bootDefaultBeforeApiOk &&
              regBootApiOk &&
              dupBootApiOk &&
              getBootApiOk &&
              noBootFeatureApiOk &&
              regMissingFeatureApiFailOk &&
              bootstrapSkippedOk;

    esp_rom_printf("[OHOS-S26B] register Bootstrap service smoke inst=%u regSvc=%u dupSvc=%u beforeApi=%u regApi=%u dupApi=%u getApi=%u noFeatApi=%u regMissingFeatApiFail=%u bootstrapSkipped=%u ok=%u\n",
                   instOk,
                   regBootSvcOk,
                   dupBootSvcOk,
                   bootDefaultBeforeApiOk,
                   regBootApiOk,
                   dupBootApiOk,
                   getBootApiOk,
                   noBootFeatureApiOk,
                   regMissingFeatureApiFailOk,
                   bootstrapSkippedOk,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "register Bootstrap service smoke ok=%u regSvc=%u regApi=%u",
               ok, regBootSvcOk, regBootApiOk);
}

static VOID OhosLiteosSamgrBootstrapPrecheckSmoke(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL noBootstrapDefaultApiOk = FALSE;
    BOOL noBootstrapFeatureApiOk = FALSE;
    BOOL regBootstrapDefaultApiFailOk = FALSE;
    BOOL regBootstrapFeatureFailOk = FALSE;
    BOOL bootstrapSkippedOk = TRUE;

    if (samgr != NULL) {
        noBootstrapDefaultApiOk = (samgr->GetDefaultFeatureApi(BOOTSTRAP_SERVICE) == NULL);
        noBootstrapFeatureApiOk = (samgr->GetFeatureApi(BOOTSTRAP_SERVICE, "NoFeature") == NULL);

        /*
         * At this point no Bootstrap service is registered.
         * These calls should fail safely.
         */
        regBootstrapDefaultApiFailOk =
            (samgr->RegisterDefaultFeatureApi(BOOTSTRAP_SERVICE, &g_ohos_s24_default_api) == FALSE);

        regBootstrapFeatureFailOk =
            (samgr->RegisterFeature(BOOTSTRAP_SERVICE, &g_ohos_s23_feature) == FALSE);
    }

    /*
     * Do NOT call SAMGR_Bootstrap here.
     * Without a valid Bootstrap task queue, SendBootRequest may reach SAMGR_SendRequest
     * with queueId == NULL.
     */
    BOOL ok = instOk &&
              noBootstrapDefaultApiOk &&
              noBootstrapFeatureApiOk &&
              regBootstrapDefaultApiFailOk &&
              regBootstrapFeatureFailOk &&
              bootstrapSkippedOk;

    esp_rom_printf("[OHOS-S26A-SAFE] bootstrap precheck inst=%u noBootDefApi=%u noBootFeatApi=%u regBootDefFail=%u regBootFeatureFail=%u bootstrapSkipped=%u ok=%u\n",
                   instOk,
                   noBootstrapDefaultApiOk,
                   noBootstrapFeatureApiOk,
                   regBootstrapDefaultApiFailOk,
                   regBootstrapFeatureFailOk,
                   bootstrapSkippedOk,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "bootstrap precheck smoke ok=%u skipped=%u",
               ok, bootstrapSkippedOk);
}

static VOID OhosLiteosSamgrUnregisterServiceSmoke(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL unregSvcOk = FALSE;
    BOOL afterDefaultOk = FALSE;
    BOOL afterFeatureOk = FALSE;
    BOOL repeatSvcOk = FALSE;
    BOOL missingSvcOk = FALSE;
    BOOL regFeatureAfterSvcDelOk = FALSE;
    BOOL regApiAfterSvcDelOk = FALSE;

    if (samgr != NULL) {
        Service *service = samgr->UnregisterService("S22Service");
        unregSvcOk = (service == &g_ohos_s22_service);

        afterDefaultOk = (samgr->GetDefaultFeatureApi("S22Service") == NULL);
        afterFeatureOk = (samgr->GetFeatureApi("S22Service", "S23Feature") == NULL);

        /* Repeated service unregister should fail safely. */
        repeatSvcOk = (samgr->UnregisterService("S22Service") == NULL);

        /* Missing service unregister should fail safely. */
        missingSvcOk = (samgr->UnregisterService("NoSuchService") == NULL);

        /* Service has been removed, so these registrations should fail. */
        regFeatureAfterSvcDelOk =
            (samgr->RegisterFeature("S22Service", &g_ohos_s23_feature) == FALSE);

        regApiAfterSvcDelOk =
            (samgr->RegisterDefaultFeatureApi("S22Service", &g_ohos_s24_default_api) == FALSE);
    }

    BOOL ok = instOk &&
              unregSvcOk &&
              afterDefaultOk &&
              afterFeatureOk &&
              repeatSvcOk &&
              missingSvcOk &&
              regFeatureAfterSvcDelOk &&
              regApiAfterSvcDelOk;

    esp_rom_printf("[OHOS-S25B] samgr unregister service smoke inst=%u unregSvc=%u afterDef=%u afterFeat=%u repeatSvc=%u missingSvc=%u regFeatureAfterDel=%u regApiAfterDel=%u ok=%u\n",
                   instOk,
                   unregSvcOk,
                   afterDefaultOk,
                   afterFeatureOk,
                   repeatSvcOk,
                   missingSvcOk,
                   regFeatureAfterSvcDelOk,
                   regApiAfterSvcDelOk,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "samgr unregister service smoke ok=%u unregSvc=%u repeat=%u",
               ok, unregSvcOk, repeatSvcOk);
}

static VOID OhosLiteosSamgrUnregisterApiFeatureSmoke(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL unregFeatureApiOk = FALSE;
    BOOL afterFeatureApiOk = FALSE;
    BOOL unregDefaultApiOk = FALSE;
    BOOL afterDefaultApiOk = FALSE;
    BOOL unregFeatureOk = FALSE;
    BOOL afterFeatureOk = FALSE;
    BOOL repeatUnregFeatureOk = FALSE;
    BOOL apiRegAfterFeatureDelOk = FALSE;

    if (samgr != NULL) {
        IUnknown *featureApi = samgr->UnregisterFeatureApi("S22Service", "S23Feature");
        unregFeatureApiOk = (featureApi == &g_ohos_s24_feature_api);
        afterFeatureApiOk = (samgr->GetFeatureApi("S22Service", "S23Feature") == NULL);

        IUnknown *defaultApi = samgr->UnregisterDefaultFeatureApi("S22Service");
        unregDefaultApiOk = (defaultApi == &g_ohos_s24_default_api);
        afterDefaultApiOk = (samgr->GetDefaultFeatureApi("S22Service") == NULL);

        Feature *feature = samgr->UnregisterFeature("S22Service", "S23Feature");
        unregFeatureOk = (feature == &g_ohos_s23_feature);
        afterFeatureOk = (samgr->GetFeatureApi("S22Service", "S23Feature") == NULL);

        repeatUnregFeatureOk = (samgr->UnregisterFeature("S22Service", "S23Feature") == NULL);

        apiRegAfterFeatureDelOk =
            (samgr->RegisterFeatureApi("S22Service", "S23Feature", &g_ohos_s24_feature_api) == FALSE);
    }

    BOOL ok = instOk &&
              unregFeatureApiOk &&
              afterFeatureApiOk &&
              unregDefaultApiOk &&
              afterDefaultApiOk &&
              unregFeatureOk &&
              afterFeatureOk &&
              repeatUnregFeatureOk &&
              apiRegAfterFeatureDelOk;

    esp_rom_printf("[OHOS-S25A] samgr unregister api/feature smoke inst=%u unregFeatApi=%u afterFeatApi=%u unregDefApi=%u afterDefApi=%u unregFeature=%u afterFeature=%u repeatFeature=%u apiAfterDel=%u ok=%u\n",
                   instOk,
                   unregFeatureApiOk,
                   afterFeatureApiOk,
                   unregDefaultApiOk,
                   afterDefaultApiOk,
                   unregFeatureOk,
                   afterFeatureOk,
                   repeatUnregFeatureOk,
                   apiRegAfterFeatureDelOk,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "samgr unregister api/feature smoke ok=%u feature=%u defaultApi=%u",
               ok, unregFeatureOk, unregDefaultApiOk);
}

static VOID OhosLiteosSamgrRegisterApiSmoke(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL regDefaultOk = FALSE;
    BOOL dupDefaultOk = FALSE;
    BOOL getDefaultOk = FALSE;
    BOOL regFeatureOk = FALSE;
    BOOL dupFeatureOk = FALSE;
    BOOL getFeatureOk = FALSE;
    BOOL queryOk = FALSE;
    BOOL refOk = FALSE;

    if (samgr != NULL) {
        regDefaultOk = (samgr->RegisterDefaultFeatureApi("S22Service", &g_ohos_s24_default_api) == TRUE);
        dupDefaultOk = (samgr->RegisterDefaultFeatureApi("S22Service", &g_ohos_s24_default_api) == FALSE);
        getDefaultOk = (samgr->GetDefaultFeatureApi("S22Service") == &g_ohos_s24_default_api);

        regFeatureOk = (samgr->RegisterFeatureApi("S22Service", "S23Feature", &g_ohos_s24_feature_api) == TRUE);
        dupFeatureOk = (samgr->RegisterFeatureApi("S22Service", "S23Feature", &g_ohos_s24_feature_api) == FALSE);
        getFeatureOk = (samgr->GetFeatureApi("S22Service", "S23Feature") == &g_ohos_s24_feature_api);

        void *target = NULL;
        int queryRet = g_ohos_s24_default_api.QueryInterface(&g_ohos_s24_default_api, 0, &target);
        queryOk = (queryRet == EC_SUCCESS) && (target == &g_ohos_s24_default_api);

        int addRef = g_ohos_s24_default_api.AddRef(&g_ohos_s24_default_api);
        int release = g_ohos_s24_default_api.Release(&g_ohos_s24_default_api);
        refOk = (addRef >= 2) && (release == addRef - 1);
    }

    BOOL ok = instOk && regDefaultOk && dupDefaultOk && getDefaultOk &&
              regFeatureOk && dupFeatureOk && getFeatureOk && queryOk && refOk;

    esp_rom_printf("[OHOS-S24] samgr register api smoke inst=%u regDef=%u dupDef=%u getDef=%u regFeat=%u dupFeat=%u getFeat=%u query=%u ref=%u ok=%u\n",
                   instOk,
                   regDefaultOk,
                   dupDefaultOk,
                   getDefaultOk,
                   regFeatureOk,
                   dupFeatureOk,
                   getFeatureOk,
                   queryOk,
                   refOk,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "samgr register api smoke ok=%u regDef=%u regFeat=%u",
               ok, regDefaultOk, regFeatureOk);
}

static VOID OhosLiteosSamgrRegisterFeatureSmoke(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL regOk = FALSE;
    BOOL dupOk = FALSE;
    BOOL missingSvcOk = FALSE;
    BOOL getFeatureApiOk = FALSE;
    BOOL invalidApiOk = FALSE;
    BOOL invalidFeatureOk = FALSE;

    if (samgr != NULL) {
        regOk = (samgr->RegisterFeature("S22Service", &g_ohos_s23_feature) == TRUE);

        /* Duplicate feature registration should fail. */
        dupOk = (samgr->RegisterFeature("S22Service", &g_ohos_s23_feature) == FALSE);

        /* Registering a feature to a missing service should fail. */
        missingSvcOk = (samgr->RegisterFeature("NoSuchService", &g_ohos_s23_feature) == FALSE);

        /* Feature exists, but no public API registered yet. */
        getFeatureApiOk = (samgr->GetFeatureApi("S22Service", "S23Feature") == NULL);

        /* Invalid API / feature paths should fail safely. */
        invalidApiOk = (samgr->RegisterFeatureApi("S22Service", "S23Feature", NULL) == FALSE);
        invalidFeatureOk = (samgr->RegisterFeature("S22Service", NULL) == FALSE);
    }

    BOOL ok = instOk && regOk && dupOk && missingSvcOk &&
              getFeatureApiOk && invalidApiOk && invalidFeatureOk;

    esp_rom_printf("[OHOS-S23] samgr register feature smoke inst=%u reg=%u dup=%u missingSvc=%u getApi=%u invalidApi=%u invalidFeature=%u ok=%u\n",
                   instOk,
                   regOk,
                   dupOk,
                   missingSvcOk,
                   getFeatureApiOk,
                   invalidApiOk,
                   invalidFeatureOk,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "samgr register feature smoke ok=%u reg=%u dup=%u",
               ok, regOk, dupOk);
}

static VOID OhosLiteosSamgrRegisterServiceSmoke(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL regOk = FALSE;
    BOOL dupOk = FALSE;
    BOOL getDefaultOk = FALSE;
    BOOL getFeatureOk = FALSE;
    BOOL invalidApiOk = FALSE;
    BOOL invalidFeatureOk = FALSE;

    if (samgr != NULL) {
        regOk = (samgr->RegisterService(&g_ohos_s22_service) == TRUE);

        /* Duplicate registration should fail. */
        dupOk = (samgr->RegisterService(&g_ohos_s22_service) == FALSE);

        /* Service exists, but no default API / feature API registered yet. */
        getDefaultOk = (samgr->GetDefaultFeatureApi("S22Service") == NULL);
        getFeatureOk = (samgr->GetFeatureApi("S22Service", "NoFeature") == NULL);

        /* Invalid API / feature registration paths should fail safely. */
        invalidApiOk = (samgr->RegisterDefaultFeatureApi("S22Service", NULL) == FALSE);
        invalidFeatureOk = (samgr->RegisterFeature("S22Service", NULL) == FALSE);
    }

    BOOL ok = instOk && regOk && dupOk && getDefaultOk && getFeatureOk &&
              invalidApiOk && invalidFeatureOk;

    esp_rom_printf("[OHOS-S22] samgr register service smoke inst=%u reg=%u dup=%u getDef=%u getFeat=%u invalidApi=%u invalidFeature=%u ok=%u\n",
                   instOk,
                   regOk,
                   dupOk,
                   getDefaultOk,
                   getFeatureOk,
                   invalidApiOk,
                   invalidFeatureOk,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "samgr register service smoke ok=%u reg=%u dup=%u",
               ok, regOk, dupOk);
}

static VOID OhosLiteosSamgrLiteCoreSmoke(void)
{
    SamgrLite *samgr = SAMGR_GetInstance();

    BOOL instOk = (samgr != NULL);
    BOOL vtblOk = FALSE;
    BOOL invalidSvcOk = FALSE;
    BOOL invalidFeatureOk = FALSE;
    BOOL sysCapInvalidOk = FALSE;
    BOOL sysCapWeakOk = FALSE;

    if (samgr != NULL) {
        vtblOk = (samgr->RegisterService != NULL) &&
                 (samgr->UnregisterService != NULL) &&
                 (samgr->RegisterFeature != NULL) &&
                 (samgr->UnregisterFeature != NULL) &&
                 (samgr->RegisterDefaultFeatureApi != NULL) &&
                 (samgr->GetDefaultFeatureApi != NULL) &&
                 (samgr->AddSystemCapability != NULL) &&
                 (samgr->HasSystemCapability != NULL);

        invalidSvcOk = (samgr->RegisterService(NULL) == FALSE);
        invalidFeatureOk = (samgr->RegisterFeature("no_service", NULL) == FALSE);

        sysCapInvalidOk = (samgr->AddSystemCapability(NULL) == EC_INVALID) &&
                          (samgr->HasSystemCapability(NULL) == FALSE);

        /* service_registry.c currently provides weak stub APIs,
         * so valid syscap registration should still return EC_FAILURE.
         */
        sysCapWeakOk = (samgr->AddSystemCapability("SystemCapability.S21.Test") == EC_FAILURE) &&
                       (samgr->HasSystemCapability("SystemCapability.S21.Test") == FALSE);
    }

    BOOL ok = instOk && vtblOk && invalidSvcOk && invalidFeatureOk && sysCapInvalidOk && sysCapWeakOk;

    esp_rom_printf("[OHOS-S21A] samgr_lite core smoke inst=%u vtbl=%u invalidSvc=%u invalidFeature=%u sysInvalid=%u sysWeak=%u ok=%u\n",
                   instOk,
                   vtblOk,
                   invalidSvcOk,
                   invalidFeatureOk,
                   sysCapInvalidOk,
                   sysCapWeakOk,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "samgr_lite core smoke ok=%u inst=%u vtbl=%u",
               ok, instOk, vtblOk);
}

static VOID OhosLiteosServiceRegistrySmoke(void)
{
    Identity id = {
        .serviceId = 20,
        .featureId = -1,
        .queueId = NULL,
    };

    /* Do not allocate MAX_SYSCAP_NUM * MAX_SYSCAP_NAME_LEN on stack.
     * service_registry.c currently provides weak stub APIs and ignores this buffer.
     */
    char (*sysCaps)[MAX_SYSCAP_NAME_LEN] = NULL;
    int32 size = 0;

    int regSvc = SAMGR_RegisterServiceApi("s20_service", NULL, &id, NULL);
    IUnknown *found = SAMGR_FindServiceApi("s20_service", NULL);
    int32 regCap = SAMGR_RegisterSystemCapabilityApi("SystemCapability.S20.Test", TRUE);
    BOOL queryCap = SAMGR_QuerySystemCapabilityApi("SystemCapability.S20.Test");
    int32 getCaps = SAMGR_GetSystemCapabilitiesApi(sysCaps, &size);

    BOOL ok = (regSvc == EC_FAILURE) &&
              (found == NULL) &&
              (regCap == EC_FAILURE) &&
              (queryCap == FALSE) &&
              (getCaps == EC_FAILURE);

    esp_rom_printf("[OHOS-S20] service_registry smoke regSvc=%d found=%p regCap=%d query=%u getCaps=%d size=%d ok=%u\n",
                   regSvc,
                   found,
                   regCap,
                   queryCap,
                   getCaps,
                   size,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "service_registry smoke ok=%u regSvc=%d regCap=%d",
               ok, regSvc, regCap);
}

static VOID OhosLiteosSamgrTaskManagerSafeSmoke(void)
{
    TaskConfig config = {
        .level = LEVEL_HIGH,
        .priority = PRI_NORMAL,
        .stackSize = 0x800,
        .queueSize = 2,
        .taskFlags = SHARED_TASK,
    };

    TaskPool *pool = SAMGR_CreateFixedTaskPool(&config, "s19a_safe_pool", 1);

    BOOL createOk = (pool != NULL);
    BOOL fieldOk = FALSE;
    BOOL refOk = FALSE;

    if (pool != NULL) {
        fieldOk = (pool->queueId != NULL) &&
                  (pool->stackSize == config.stackSize) &&
                  (pool->priority == config.priority) &&
                  (pool->size == 1) &&
                  (pool->top == 0) &&
                  (pool->ref == 1);

        TaskPool *refPool = SAMGR_ReferenceTaskPool(pool);
        refOk = (refPool == pool) && (pool->ref == 2);

        /* S19-A-safe:
         * Do not call SAMGR_StartTaskPool.
         * Do not destroy queue.
         * Do not free TaskPool.
         * Keep this object alive to avoid disturbing later LiteOS scheduler tests.
         */
    }

    BOOL ok = createOk && fieldOk && refOk;

    esp_rom_printf("[OHOS-S19A-SAFE] task_manager smoke create=%u field=%u ref=%u queue=%p ok=%u\n",
                   createOk,
                   fieldOk,
                   refOk,
                   (pool != NULL) ? pool->queueId : NULL,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "task_manager safe smoke ok=%u create=%u field=%u ref=%u",
               ok, createOk, fieldOk, refOk);
}

static VOID OhosLiteosSamgrMessageSmoke(void)
{
    MQueueId q = QUEUE_Create("samgr_msg_q", sizeof(Exchange), 2);

    Request req = {
        .msgId = 18,
        .len = 0,
        .data = NULL,
        .msgValue = 0x18181818U,
    };

    Identity id = {
        .serviceId = 8,
        .featureId = -1,
        .queueId = q,
    };

    int32 sendRet = (q != NULL) ? SAMGR_SendRequest(&id, &req, NULL) : EC_FAILURE;

    Exchange rx = {0};
    int32 recvRet = (q != NULL) ? SAMGR_MsgRecv(q, (uint8 *)&rx, sizeof(rx)) : EC_FAILURE;

    BOOL recvOk = (recvRet == EC_SUCCESS) &&
                  (rx.id.serviceId == 8) &&
                  (rx.id.featureId == -1) &&
                  (rx.request.msgId == 18) &&
                  (rx.request.msgValue == 0x18181818U) &&
                  (rx.type == MSG_NON);

    int32 freeRet = SAMGR_FreeMsg(&rx);

    int32 nullRespRet = SAMGR_SendResponse(NULL, NULL);

    int destroyRet = (q != NULL) ? QUEUE_Destroy(q) : EC_FAILURE;

    BOOL ok = (q != NULL) &&
              (sendRet == EC_SUCCESS) &&
              recvOk &&
              (freeRet == EC_SUCCESS) &&
              (nullRespRet == EC_INVALID) &&
              (destroyRet == EC_SUCCESS);

    esp_rom_printf("[OHOS-S18] samgr_message smoke q=%p send=%d recv=%d recvOk=%u free=%d nullResp=%d destroy=%d msgId=%d msgValue=0x%x type=%d ok=%u\n",
                   q,
                   sendRet,
                   recvRet,
                   recvOk,
                   freeRet,
                   nullRespRet,
                   destroyRet,
                   rx.request.msgId,
                   rx.request.msgValue,
                   rx.type,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "samgr_message smoke ok=%u send=%d recv=%d",
               ok, sendRet, recvRet);
}


static VOID OhosLiteosSamgrServiceSmoke(void)
{
    static Service service = {
        .GetName = OhosLiteosServiceGetName,
        .Initialize = OhosLiteosServiceInitialize,
        .MessageHandle = OhosLiteosServiceMessageHandle,
        .GetTaskConfig = OhosLiteosServiceGetTaskConfig,
    };

    static Feature feature = {
        .GetName = OhosLiteosS17FeatureGetName,
        .OnInitialize = OhosLiteosS17FeatureOnInitialize,
        .OnStop = OhosLiteosS17FeatureOnStop,
        .OnMessage = OhosLiteosS17FeatureOnMessage,
    };

    ServiceImpl *impl = SAMGR_CreateServiceImpl(&service, SVC_INIT);
    BOOL createOk = (impl != NULL);

    int16 featureId = createOk ? DEFAULT_AddFeature(impl, &feature) : INVALID_INDEX;
    BOOL addOk = (featureId >= 0);

    FeatureImpl *found = createOk ? DEFAULT_GetFeature(impl, "S17Feature") : NULL;
    BOOL findOk = (found != NULL) && (found->feature == &feature);

    if (createOk) {
        impl->serviceId = 7;
    }

    Identity id = createOk ? DEFAULT_GetFeatureId(impl, "S17Feature") : (Identity){INVALID_INDEX, INVALID_INDEX, NULL};
    BOOL idOk = (id.serviceId == 7) && (id.featureId == featureId);

    Feature *deleted = createOk ? DEFAULT_DeleteFeature(impl, "S17Feature") : NULL;
    BOOL delOk = (deleted == &feature);

    BOOL ok = createOk && addOk && findOk && idOk && delOk;

    esp_rom_printf("[OHOS-S17] samgr_service smoke create=%u featureId=%d add=%u find=%u idSvc=%d idFeature=%d idOk=%u del=%u ok=%u\n",
                   createOk,
                   featureId,
                   addOk,
                   findOk,
                   id.serviceId,
                   id.featureId,
                   idOk,
                   delOk,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "samgr_service smoke ok=%u fid=%d idOk=%u del=%u",
               ok, featureId, idOk, delOk);

    if (impl != NULL) {
        VECTOR_Clear(&impl->features);
        SAMGR_Free(impl);
    }
}


static VOID OhosLiteosSamgrFeatureSmoke(void)
{
    static Feature feature = {
        .GetName = OhosLiteosFeatureGetName,
        .OnInitialize = OhosLiteosFeatureOnInitialize,
        .OnStop = OhosLiteosFeatureOnStop,
        .OnMessage = OhosLiteosFeatureOnMessage,
    };

    static OhosLiteosFeatureIUnknownObj apiObj = {
        DEFAULT_IUNKNOWN_ENTRY_BEGIN,
        DEFAULT_IUNKNOWN_ENTRY_END,
        .magic = 0x46545245U,
    };

    FeatureImpl *impl = FEATURE_CreateInstance(&feature);
    BOOL createOk = (impl != NULL);

    BOOL noInterfaceBefore = createOk ? SAMGR_IsNoInterface(impl) : FALSE;

    IUnknown *api = GET_IUNKNOWN(apiObj);
    BOOL addOk = createOk ? SAMGR_AddInterface(impl, api) : FALSE;

    IUnknown *gotApi = createOk ? SAMGR_GetInterface(impl) : NULL;
    BOOL getOk = (gotApi == api);

    IUnknown *delApi = createOk ? SAMGR_DelInterface(impl) : NULL;
    BOOL delOk = (delApi == api);

    BOOL noInterfaceAfter = createOk ? SAMGR_IsNoInterface(impl) : FALSE;

    BOOL ok = createOk &&
              noInterfaceBefore &&
              addOk &&
              getOk &&
              delOk &&
              noInterfaceAfter &&
              (apiObj.magic == 0x46545245U);

    esp_rom_printf("[OHOS-S16] samgr_feature smoke create=%u noBefore=%u add=%u get=%u del=%u noAfter=%u magic=0x%x ok=%u\n",
                   createOk,
                   noInterfaceBefore,
                   addOk,
                   getOk,
                   delOk,
                   noInterfaceAfter,
                   apiObj.magic,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "samgr_feature smoke ok=%u add=%u get=%u del=%u",
               ok, addOk, getOk, delOk);

    if (impl != NULL) {
        SAMGR_Free(impl);
    }
}


static VOID OhosLiteosSamgrIUnknownSmoke(void)
{
    static OhosLiteosIUnknownSmokeObj obj = {
        DEFAULT_IUNKNOWN_ENTRY_BEGIN,
        DEFAULT_IUNKNOWN_ENTRY_END,
        .magic = 0x494B4E57U,
    };

    IUnknown *iUnknown = GET_IUNKNOWN(obj);

    int addRefRet = iUnknown->AddRef(iUnknown);

    void *target = NULL;
    int queryRet = iUnknown->QueryInterface(iUnknown, DEFAULT_VERSION, &target);

    int releaseRet1 = iUnknown->Release(iUnknown);
    int releaseRet2 = iUnknown->Release(iUnknown);

    BOOL ok = (addRefRet >= 2) &&
              (queryRet == EC_SUCCESS) &&
              (target == iUnknown) &&
              (releaseRet1 >= 1) &&
              (releaseRet2 >= 1) &&
              (obj.magic == 0x494B4E57U);

    esp_rom_printf("[OHOS-S15] samgr_iunknown smoke addRef=%d query=%d targetOk=%u release=%d/%d magic=0x%x ok=%u\n",
                   addRefRet,
                   queryRet,
                   target == iUnknown,
                   releaseRet1,
                   releaseRet2,
                   obj.magic,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "samgr_iunknown smoke ok=%u addRef=%d query=%d",
               ok, addRefRet, queryRet);
}


static VOID OhosLiteosSamgrVectorSmoke(void)
{
    Vector vec = VECTOR_Make(NULL, NULL);

    uint32 a = 0x11111111U;
    uint32 b = 0x22222222U;

    int16 idxA = VECTOR_Add(&vec, &a);
    int16 idxB = VECTOR_Add(&vec, &b);

    uint32 *pa = (uint32 *)VECTOR_At(&vec, idxA);
    uint32 *pb = (uint32 *)VECTOR_At(&vec, idxB);

    int16 size = VECTOR_Size(&vec);
    int16 num = VECTOR_Num(&vec);

    BOOL ok = (idxA == 0) &&
              (idxB == 1) &&
              (pa != NULL) &&
              (pb != NULL) &&
              (*pa == 0x11111111U) &&
              (*pb == 0x22222222U) &&
              (size == 2) &&
              (num == 2);

    esp_rom_printf("[OHOS-S14] samgr_vector smoke idxA=%d idxB=%d size=%d num=%d pa=0x%x pb=0x%x ok=%u\n",
                   idxA,
                   idxB,
                   size,
                   num,
                   pa ? *pa : 0,
                   pb ? *pb : 0,
                   ok);

    HILOG_INFO(HILOG_MODULE_INIT, "samgr_vector smoke ok=%u size=%d num=%d", ok, size, num);

    VECTOR_Clear(&vec);
}


static VOID OhosLiteosSamgrAdapterSmoke(void)
{
    void *mem = SAMGR_Malloc(32);
    BOOL memOk = FALSE;
    if (mem != NULL) {
        UINT8 *buf = (UINT8 *)mem;
        for (UINT32 i = 0; i < 32; i++) {
            buf[i] = (UINT8)(0xA0U + i);
        }
        SAMGR_Free(mem);
        memOk = TRUE;
    }

    typedef struct {
        uint32 seq;
        uint32 magic;
    } SamgrAdapterMsg;

    MQueueId q = QUEUE_Create("samgr_adp_q", sizeof(SamgrAdapterMsg), 2);
    SamgrAdapterMsg tx = {
        .seq = 1,
        .magic = 0x53414D47U,
    };
    SamgrAdapterMsg rx = {0};
    uint8 pri = 0;

    int putRet = (q != NULL) ? QUEUE_Put(q, &tx, 0, 0) : EC_FAILURE;
    int popRet = (q != NULL) ? QUEUE_Pop(q, &rx, &pri, 0) : EC_FAILURE;
    int destroyRet = (q != NULL) ? QUEUE_Destroy(q) : EC_FAILURE;

    MutexId mux = MUTEX_InitValue();
    BOOL muxOk = FALSE;
    if (mux != NULL) {
        MUTEX_Lock(mux);
        MUTEX_Unlock(mux);
        muxOk = TRUE;
    }

    uint64 nowMs = SAMGR_GetProcessTime();

    esp_rom_printf("[OHOS-S13] samgr_adapter smoke memOk=%u q=%p put=%d pop=%d destroy=%d rxSeq=%u rxMagic=0x%x muxOk=%u timeMs=%llu\n",
                   memOk,
                   q,
                   putRet,
                   popRet,
                   destroyRet,
                   rx.seq,
                   rx.magic,
                   muxOk,
                   nowMs);

    HILOG_INFO(HILOG_MODULE_INIT, "samgr_adapter smoke mem=%u qret=%d/%d mux=%u",
               memOk, putRet, popRet, muxOk);
}


unsigned int ohos_liteos_create_test_task(void)
{
    UINT32 ret;
    unsigned int consumerTask = 0;
    unsigned int producerTask = 0;
    TSK_INIT_PARAM_S initParam = {0};

#if OHOS_ENABLE_BRINGUP_SMOKE
    OhosLiteosUtilsLiteSmoke();
    OhosLiteosHilogLiteSmoke();
    OhosLiteosStartupLiteSmoke();
    OhosLiteosSamgrLiteHeaderSmoke();
    OhosLiteosSamgrAdapterSmoke();
    OhosLiteosSamgrVectorSmoke();
    OhosLiteosSamgrIUnknownSmoke();
    OhosLiteosSamgrFeatureSmoke();
    OhosLiteosSamgrServiceSmoke();
    OhosLiteosSamgrMessageSmoke();
    OhosLiteosSamgrTaskManagerSafeSmoke();
    OhosLiteosServiceRegistrySmoke();
    OhosLiteosSamgrLiteCoreSmoke();
    OhosLiteosSamgrRegisterServiceSmoke();
    OhosLiteosSamgrRegisterFeatureSmoke();
    OhosLiteosSamgrRegisterApiSmoke();
    OhosLiteosSamgrUnregisterApiFeatureSmoke();
    OhosLiteosSamgrUnregisterServiceSmoke();
    OhosLiteosSamgrBootstrapPrecheckSmoke();
    OhosLiteosSamgrRegisterBootstrapServiceSmoke();
    OhosLiteosSamgrRegisterSingleTaskServiceSmoke();
    OhosLiteosSamgrRegisterAckCallerServiceSmoke();
    OhosLiteosSamgrBootstrapNoTaskSmoke();
#endif

#if OHOS_ENABLE_DEMO_SERVICE
    ret = OhosDemoServiceStart();
    esp_rom_printf("[OHOS-S36C] OhosDemoServiceStart ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#if OHOS_ENABLE_LED_SERVICE && !OHOS_ENABLE_MULTI_SERVICE_RUNTIME
    ret = OhosLedServiceStart();
    esp_rom_printf("[OHOS-S41D] OhosLedServiceStart ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#if OHOS_ENABLE_KEY_SERVICE && !OHOS_ENABLE_MULTI_SERVICE_RUNTIME
    ret = OhosKeyServiceStart();
    esp_rom_printf("[OHOS-S41C] OhosKeyServiceStart ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#if OHOS_ENABLE_UART_LINK_SERVICE && !OHOS_ENABLE_MULTI_SERVICE_RUNTIME
    ret = OhosUartLinkServiceStart();
    esp_rom_printf("[OHOS-S42A] OhosUartLinkServiceStart ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif


#if OHOS_ENABLE_MULTI_SERVICE_RUNTIME
    esp_rom_printf("[OHOS-S41E] multi-service runtime register begin\n");

#if OHOS_ENABLE_LED_SERVICE
    ret = OhosLedServiceRegister();
    esp_rom_printf("[OHOS-S41E] OhosLedServiceRegister ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#if OHOS_ENABLE_KEY_SERVICE
    ret = OhosKeyServiceRegister();
    esp_rom_printf("[OHOS-S41E] OhosKeyServiceRegister ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#if OHOS_ENABLE_UART_LINK_SERVICE
    ret = OhosUartLinkServiceRegister();
    esp_rom_printf("[OHOS-S41E] OhosUartLinkServiceRegister ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif


#if OHOS_ENABLE_DISPLAY_SERVICE
    ret = OhosDisplayServiceRegister();
    esp_rom_printf("[OHOS-S41E] OhosDisplayServiceRegister ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif


#if OHOS_ENABLE_AUDIO_SERVICE
    ret = OhosAudioServiceRegister();
    esp_rom_printf("[OHOS-S41E] OhosAudioServiceRegister ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif


#if OHOS_ENABLE_CAMERA_SERVICE
    ret = OhosCameraServiceRegister();
    esp_rom_printf("[OHOS-S41E] OhosCameraServiceRegister ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif

    esp_rom_printf("[OHOS-S41E] SAMGR_Bootstrap once begin\n");
    SAMGR_Bootstrap();
    esp_rom_printf("[OHOS-S41E] SAMGR_Bootstrap once done\n");

#if OHOS_ENABLE_LED_SERVICE
    ret = OhosLedServiceStartTasks();
    esp_rom_printf("[OHOS-S41E] OhosLedServiceStartTasks ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#if OHOS_ENABLE_KEY_SERVICE
    ret = OhosKeyServiceStartTasks();
    esp_rom_printf("[OHOS-S41E] OhosKeyServiceStartTasks ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#if OHOS_ENABLE_UART_LINK_SERVICE
    ret = OhosUartLinkServiceStartTasks();
    esp_rom_printf("[OHOS-S41E] OhosUartLinkServiceStartTasks ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif


#if OHOS_ENABLE_DISPLAY_SERVICE
    ret = OhosDisplayServiceStartTasks();
    esp_rom_printf("[OHOS-S41E] OhosDisplayServiceStartTasks ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif


#if OHOS_ENABLE_AUDIO_SERVICE
    ret = OhosAudioServiceStartTasks();
    esp_rom_printf("[OHOS-S41E] OhosAudioServiceStartTasks ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif


#if OHOS_ENABLE_CAMERA_SERVICE
    ret = OhosCameraServiceStartTasks();
    esp_rom_printf("[OHOS-S41E] OhosCameraServiceStartTasks ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#if OHOS_ENABLE_CONFIG_SERVICE
    ret = ConfigServiceStartTask();
    esp_rom_printf("[OHOS-S41E] ConfigServiceStartTask ret=%u\n", ret);
#endif

#if OHOS_ENABLE_FONT_SERVICE
    ret = FontServiceStartTask();
    esp_rom_printf("[OHOS-S41E] FontServiceStartTask ret=%u\n", ret);
#endif

#if OHOS_ENABLE_K2_VERIFY_TASK
    ret = OhosKeyServiceStartK2VerifyTask();
    esp_rom_printf("[OHOS-S41E] OhosKeyServiceStartK2VerifyTask ret=%u action=voice_user_action\n", ret);
#endif

#if OHOS_ENABLE_MULTI_SERVICE_SELFTEST
    ret = OhosMultiServiceSelfTestStart();
    esp_rom_printf("[OHOS-S46A] OhosMultiServiceSelfTestStart ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif

    esp_rom_printf("[OHOS-S41E] multi-service runtime start done\n");
#endif


#if OHOS_ENABLE_DISPLAY_SERVICE && !OHOS_ENABLE_MULTI_SERVICE_RUNTIME
    ret = OhosDisplayServiceStart();
    esp_rom_printf("[OHOS-S43A] OhosDisplayServiceStart ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif


#if OHOS_ENABLE_AUDIO_SERVICE && !OHOS_ENABLE_MULTI_SERVICE_RUNTIME
    ret = OhosAudioServiceStart();
    esp_rom_printf("[OHOS-S44A] OhosAudioServiceStart ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif


#if OHOS_ENABLE_CAMERA_SERVICE && !OHOS_ENABLE_MULTI_SERVICE_RUNTIME
    ret = OhosCameraServiceStart();
    esp_rom_printf("[OHOS-S45A] OhosCameraServiceStart ret=%u\n", ret);
    if (ret != LOS_OK) {
        return ret;
    }
#endif

#if OHOS_ENABLE_KERNEL_CONTINUOUS_SMOKE
    ret = LOS_QueueCreate("ohos_s7_q", 4, &g_ohos_s7_queue, 0, sizeof(OhosLiteosQueueMsg));
    esp_rom_printf("[OHOS-S7] LOS_QueueCreate ret=%u queue=%u msgSize=%u\n",
                   ret, g_ohos_s7_queue, (UINT32)sizeof(OhosLiteosQueueMsg));
    if (ret != LOS_OK) {
        return ret;
    }

    initParam.pfnTaskEntry = OhosLiteosQueueConsumerTask;
    initParam.usTaskPrio = 10;
    initParam.uwStackSize = OHOS_SMOKE_TASK_STACK_SIZE;
    initParam.pcName = "ohos_s7_cons";
    initParam.uwArg = 0;

    ret = LOS_TaskCreate(&consumerTask, &initParam);
    esp_rom_printf("[OHOS-S7] LOS_TaskCreate consumer ret=%u taskId=%u prio=%u\n",
                   ret, consumerTask, initParam.usTaskPrio);
    if (ret != LOS_OK) {
        return ret;
    }

    initParam = (TSK_INIT_PARAM_S){0};
    initParam.pfnTaskEntry = OhosLiteosQueueProducerTask;
    initParam.usTaskPrio = 11;
    initParam.uwStackSize = OHOS_SMOKE_TASK_STACK_SIZE;
    initParam.pcName = "ohos_s7_prod";
    initParam.uwArg = 0;

    ret = LOS_TaskCreate(&producerTask, &initParam);
    esp_rom_printf("[OHOS-S7] LOS_TaskCreate producer ret=%u taskId=%u prio=%u\n",
               ret, producerTask, initParam.usTaskPrio);
    if (ret != LOS_OK) {
        return ret;
    }
    /* S8: semaphore smoke test */
    ret = LOS_SemCreate(0, &g_ohos_s8_sem);
    esp_rom_printf("[OHOS-S8] LOS_SemCreate ret=%u sem=%u\n", ret, g_ohos_s8_sem);
    if (ret != LOS_OK) {
        return ret;
    }
    unsigned int semTakerTask = 0;
    unsigned int semGiverTask = 0;
    initParam = (TSK_INIT_PARAM_S){0};
    initParam.pfnTaskEntry = OhosLiteosSemTakerTask;
    initParam.usTaskPrio = 9;
    initParam.uwStackSize = OHOS_SMOKE_TASK_STACK_SIZE;
    initParam.pcName = "ohos_s8_sem_taker";
    initParam.uwArg = 0;
    
    ret = LOS_TaskCreate(&semTakerTask, &initParam);
    esp_rom_printf("[OHOS-S8] LOS_TaskCreate sem_taker ret=%u taskId=%u prio=%u\n",
               ret, semTakerTask, initParam.usTaskPrio);
    if (ret != LOS_OK) {
        return ret;
    }
    
    initParam = (TSK_INIT_PARAM_S){0};
    initParam.pfnTaskEntry = OhosLiteosSemGiverTask;
    initParam.usTaskPrio = 12;
    initParam.uwStackSize = OHOS_SMOKE_TASK_STACK_SIZE;
    initParam.pcName = "ohos_s8_sem_giver";
    initParam.uwArg = 0;
    
    ret = LOS_TaskCreate(&semGiverTask, &initParam);
    esp_rom_printf("[OHOS-S8] LOS_TaskCreate sem_giver ret=%u taskId=%u prio=%u\n",
               ret, semGiverTask, initParam.usTaskPrio);
    if (ret != LOS_OK) {
        return ret;
    }
    /* S8: mutex smoke test */
    ret = LOS_MuxCreate(&g_ohos_s8_mux);
    esp_rom_printf("[OHOS-S8] LOS_MuxCreate ret=%u mux=%u\n", ret, g_ohos_s8_mux);
    if (ret != LOS_OK) {
        return ret;
    }
    unsigned int muxTask1 = 0;
    unsigned int muxTask2 = 0;

    initParam = (TSK_INIT_PARAM_S){0};
    initParam.pfnTaskEntry = OhosLiteosMuxTask;
    initParam.usTaskPrio = 13;
    initParam.uwStackSize = OHOS_SMOKE_TASK_STACK_SIZE;
    initParam.pcName = "ohos_s8_mux1";
    initParam.uwArg = 1;

    ret = LOS_TaskCreate(&muxTask1, &initParam);
    esp_rom_printf("[OHOS-S8] LOS_TaskCreate mux1 ret=%u taskId=%u prio=%u\n",
               ret, muxTask1, initParam.usTaskPrio);
    if (ret != LOS_OK) {
        return ret;
    }
    
    initParam = (TSK_INIT_PARAM_S){0};
    initParam.pfnTaskEntry = OhosLiteosMuxTask;
    initParam.usTaskPrio = 13;
    initParam.uwStackSize = OHOS_SMOKE_TASK_STACK_SIZE;
    initParam.pcName = "ohos_s8_mux2";
    initParam.uwArg = 2;
    
    ret = LOS_TaskCreate(&muxTask2, &initParam);
    esp_rom_printf("[OHOS-S8] LOS_TaskCreate mux2 ret=%u taskId=%u prio=%u\n",
               ret, muxTask2, initParam.usTaskPrio);
    if (ret != LOS_OK) {
        return ret;
    }

    /* S8: event smoke test, reuse sem tasks to avoid exceeding task limit */
    ret = LOS_EventInit(&g_ohos_s8_event);
    esp_rom_printf("[OHOS-S8] LOS_EventInit ret=%u\n", ret);

    return ret;
#else
    esp_rom_printf("[OHOS-S35C] kernel continuous smoke disabled\n");
    return LOS_OK;
#endif
}


#if OHOS_ENABLE_MULTI_SERVICE_SELFTEST
static void OhosMultiServiceSelfTestTask(void *arg)
{
    (void)arg;

    LOS_TaskDelay(30);

    UINT32 ret = LOS_OK;
    UINT32 finalOk = 1U;

    esp_rom_printf("[OHOS-S46A] unified selftest begin\n");

#if OHOS_ENABLE_LED_SERVICE
    ret = OhosLedServiceSelfTest();
    esp_rom_printf("[OHOS-S46A] unified LED selftest ret=%u\n", ret);
    if (ret != LOS_OK) {
        finalOk = 0U;
    }
#endif

#if OHOS_ENABLE_KEY_SERVICE
    ret = OhosKeyServiceSelfTest();
    esp_rom_printf("[OHOS-S46A] unified Key selftest ret=%u\n", ret);
    if (ret != LOS_OK) {
        finalOk = 0U;
    }
#endif

#if OHOS_ENABLE_UART_LINK_SERVICE
    ret = OhosUartLinkServiceSelfTest();
    esp_rom_printf("[OHOS-S46A] unified UART selftest ret=%u\n", ret);
    if (ret != LOS_OK) {
        finalOk = 0U;
    }
#endif

#if OHOS_ENABLE_DISPLAY_SERVICE
    ret = OhosDisplayServiceSelfTest();
    esp_rom_printf("[OHOS-S46A] unified Display selftest ret=%u\n", ret);
    if (ret != LOS_OK) {
        finalOk = 0U;
    }
#endif

#if OHOS_ENABLE_AUDIO_SERVICE
    ret = OhosAudioServiceSelfTest();
    esp_rom_printf("[OHOS-S46A] unified Audio selftest ret=%u\n", ret);
    if (ret != LOS_OK) {
        finalOk = 0U;
    }
#endif

#if OHOS_ENABLE_CAMERA_SERVICE
    ret = OhosCameraServiceSelfTest();
    esp_rom_printf("[OHOS-S46A] unified Camera selftest ret=%u\n", ret);
    if (ret != LOS_OK) {
        finalOk = 0U;
    }
#endif

    esp_rom_printf("[OHOS-S46A] unified selftest final ok=%u\n", finalOk);
}

static UINT32 OhosMultiServiceSelfTestStart(void)
{
    UINT32 taskId = 0;

    UINT32 ret = OhosLiteosCreateTask("ohos_unified_selftest",
                                      OhosMultiServiceSelfTestTask,
                                      NULL,
                                      25,
                                      0x1400,
                                      &taskId);

    esp_rom_printf("[OHOS-S46A] unified selftest task create ret=%u taskId=%u prio=%u stack=0x%x\n",
                   ret,
                   taskId,
                   25U,
                   0x1400U);

    return ret;
}
#endif


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
