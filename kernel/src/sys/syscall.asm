[bits 64]
[global syscall_entry]
[extern syscall_handler]
[extern idt_block]
[extern idt_unblock]

syscall_entry:
    ; Currently running on user gs, switch to the kernel gs
    swapgs ; switch
    mov [gs:0], rsp ; Save user stack in gs
    mov rsp, [gs:8] ; Kernel stack

    push rcx
    push r11

    push r9
    push r8
    push r10
    push rdx
    push rsi
    push rdi
    push rax

    mov rdi, rsp
    call syscall_handler

    add rsp, 8
    pop rdi
    pop rsi
    pop rdx
    pop r10
    pop r8
    pop r9

    pop r11
    pop rcx

    mov [gs:8], rsp
    mov rsp, [gs:0]
    swapgs ; swap again, now kernel stack is the kernel gs again
    o64 sysret