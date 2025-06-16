[section .text]
[global sched_get_rip]
sched_get_rip:
    pop rax ; sched_get_rip was called, returning address was put into the stack
    push rax ; push it again so we can ret
    ret