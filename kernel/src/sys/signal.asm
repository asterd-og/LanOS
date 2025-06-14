[section .text]
[global sched_sighandle]
[global sched_sighandle_end]

sched_sighandle: ; (int, void*)
    mov ax, cs
    and ax, 3
    cmp ax, 0
    je .sighandle_sys ; if (!(cs & 3)) sched_sighandle_sys()
    jmp .sighandle_call
.sighandle_sys:
    ; It got interrupted mid-syscall, let's swapgs
    swapgs
.sighandle_call:
    call rsi ; sa_handler, sig number is in rdi already
    ret ; Calls sa_restorer (it was pushed onto the stack)

sched_sighandle_end:
    ; Label for the kernel to know the size of sched_sighandle