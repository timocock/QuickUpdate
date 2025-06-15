#include <exec/exec.h>
#include <proto/exec.h>
#include <string.h>

/* SAS/C specific memory layout information */
extern UBYTE *__base;
extern ULONG __stack;

/* Structure to hold child process context */
struct ChildContext {
    APTR user_stack;
    APTR system_stack;
    APTR data_seg;
    ULONG stack_size;
};

/* Assembly helper to switch stacks */
void __asm SwitchStack(register __a0 APTR newStack, register __d0 ULONG size) {
    move.l a0, sp      // Set new stack pointer
    move.l #0, d0      // Child returns 0
    rts
}

/* Main fork implementation */
int fork(void) {
    struct Process *parent = (struct Process *)FindTask(NULL);
    ULONG stack_size = (ULONG)parent->pr_Task.tc_SPUpper - (ULONG)parent->pr_Task.tc_SPLower;
    
    // Allocate child memory segments
    APTR child_data = AllocMem((ULONG)__base, MEMF_CLEAR);
    APTR child_stack = AllocMem(stack_size, MEMF_CLEAR);
    
    if (!child_data || !child_stack) {
        FreeMem(child_data, (ULONG)__base);
        FreeMem(child_stack, stack_size);
        return -1;
    }

    // Copy parent's data and stack
    memcpy(child_data, __base, (ULONG)__base);
    memcpy(child_stack, (APTR)parent->pr_Task.tc_SPLower, stack_size);

    // Set up child context
    struct ChildContext ctx = {
        .user_stack = child_stack + stack_size,
        .system_stack = child_stack,
        .data_seg = child_data,
        .stack_size = stack_size
    };

    // Create new process
    struct Process *child = (struct Process *)CreateProc(
        "forked-child",
        0,
        (void (*)())SwitchStack,
        stack_size,
        (IPTR)&ctx
    );

    if (!child) {
        FreeMem(child_data, (ULONG)__base);
        FreeMem(child_stack, stack_size);
        return -1;
    }

    // Return child PID to parent
    return child->pr_ProcessID;
}
