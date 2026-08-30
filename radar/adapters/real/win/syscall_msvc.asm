; syscall_msvc.asm — MSVC MASM indirect syscall wrappers for x64
;
; Each function sets up the x64 syscall convention from the C calling-convention
; parameters, then JMPs to the runtime-resolved syscall gadget in ntdll.
;
; Stack layout at function entry (x64 C calling convention):
;   [rsp]       = return address
;   [rsp+08h]   = shadow/home space for RCX
;   [rsp+10h]   = shadow/home space for RDX
;   [rsp+18h]   = shadow/home space for R8
;   [rsp+20h]   = shadow/home space for R9
;   [rsp+28h]   = first stack parameter  (arg4 for 4-arg wrapper)
;   [rsp+30h]   = second stack parameter (arg5 for 5-arg wrapper)
;   [rsp+38h]   = third stack parameter  (arg6 for 6-arg wrapper)
;   [rsp+40h]   = fourth stack parameter (arg7 for 7-arg wrapper)
;
; Parameter mapping (C caller → syscall convention):
;   RCX = ssn        → EAX = syscall number
;   RDX = arg1       → RCX = 1st NT function arg, R10 = arg1 (syscall convention)
;   R8  = arg2       → RDX = 2nd NT function arg
;   R9  = arg3       → R8  = 3rd NT function arg
;   [rsp+28h] = arg4 → R9  = 4th NT function arg
;   [rsp+30h] = arg5 → [rsp+28h] = 5th NT function arg (shifted down by 8)
;   [rsp+38h] = arg6 → [rsp+30h] = 6th NT function arg
;   [rsp+40h] = arg7 → [rsp+38h] = 7th NT function arg
;
; The stack shift occurs because our function's C caller allocated shadow space
; for 4 register params above the return address.  The kernel looks for stack
; params at [rsp+28h] relative to the syscall instruction, so we shift each
; stack-based argument down by one slot (8 bytes) when moving from our
; parameter layout to the kernel-expected layout.

.code

EXTERN g_syscall_gadget_ptr : qword

;------------------------------------------------------------------------------
; NTSTATUS syscall_asm_4(int ssn, uint64_t arg1, uint64_t arg2,
;                        uint64_t arg3, uint64_t arg4)
;------------------------------------------------------------------------------
syscall_asm_4 PROC
    mov     r11, rcx            ; save ssn in r11
    mov     rcx, rdx            ; arg1 → rcx  (1st NT arg)
    mov     rdx, r8             ; arg2 → rdx  (2nd NT arg)
    mov     r8, r9              ; arg3 → r8   (3rd NT arg)
    mov     r9, [rsp+28h]       ; arg4 → r9   (4th NT arg)
    mov     eax, r11d           ; ssn → eax   (syscall number)
    mov     r10, rcx            ; r10 = arg1  (syscall convention)
    jmp     qword ptr [g_syscall_gadget_ptr]
syscall_asm_4 ENDP

;------------------------------------------------------------------------------
; NTSTATUS syscall_asm_5(int ssn, uint64_t arg1, uint64_t arg2,
;                        uint64_t arg3, uint64_t arg4, uint64_t arg5)
;------------------------------------------------------------------------------
syscall_asm_5 PROC
    mov     r11, rcx            ; save ssn in r11
    mov     rcx, rdx            ; arg1 → rcx
    mov     rdx, r8             ; arg2 → rdx
    mov     r8, r9              ; arg3 → r8
    mov     r9, [rsp+28h]       ; arg4 → r9
    mov     r10, [rsp+30h]      ; arg5 → r10 (temp)
    mov     [rsp+28h], r10      ; arg5 → stack (kernel expects at rsp+28h)
    mov     eax, r11d           ; ssn → eax
    mov     r10, rcx            ; r10 = arg1
    jmp     qword ptr [g_syscall_gadget_ptr]
syscall_asm_5 ENDP

;------------------------------------------------------------------------------
; NTSTATUS syscall_asm_6(int ssn, uint64_t arg1, uint64_t arg2,
;                        uint64_t arg3, uint64_t arg4, uint64_t arg5,
;                        uint64_t arg6)
;------------------------------------------------------------------------------
syscall_asm_6 PROC
    mov     r11, rcx            ; save ssn in r11
    mov     rcx, rdx            ; arg1 → rcx
    mov     rdx, r8             ; arg2 → rdx
    mov     r8, r9              ; arg3 → r8
    mov     r9, [rsp+28h]       ; arg4 → r9
    mov     r10, [rsp+30h]      ; arg5 → r10 (temp)
    mov     [rsp+28h], r10      ; arg5 → stack (kernel expects at rsp+28h)
    mov     r10, [rsp+38h]      ; arg6 → r10 (temp)
    mov     [rsp+30h], r10      ; arg6 → stack (kernel expects at rsp+30h)
    mov     eax, r11d           ; ssn → eax
    mov     r10, rcx            ; r10 = arg1
    jmp     qword ptr [g_syscall_gadget_ptr]
syscall_asm_6 ENDP

;------------------------------------------------------------------------------
; NTSTATUS syscall_asm_7(int ssn, uint64_t arg1, uint64_t arg2,
;                        uint64_t arg3, uint64_t arg4, uint64_t arg5,
;                        uint64_t arg6, uint64_t arg7)
;------------------------------------------------------------------------------
syscall_asm_7 PROC
    mov     r11, rcx            ; save ssn in r11
    mov     rcx, rdx            ; arg1 → rcx
    mov     rdx, r8             ; arg2 → rdx
    mov     r8, r9              ; arg3 → r8
    mov     r9, [rsp+28h]       ; arg4 → r9
    mov     r10, [rsp+30h]      ; arg5 → r10 (temp)
    mov     [rsp+28h], r10      ; arg5 → stack (kernel expects at rsp+28h)
    mov     r10, [rsp+38h]      ; arg6 → r10 (temp)
    mov     [rsp+30h], r10      ; arg6 → stack (kernel expects at rsp+30h)
    mov     r10, [rsp+40h]      ; arg7 → r10 (temp)
    mov     [rsp+38h], r10      ; arg7 → stack (kernel expects at rsp+38h)
    mov     eax, r11d           ; ssn → eax
    mov     r10, rcx            ; r10 = arg1
    jmp     qword ptr [g_syscall_gadget_ptr]
syscall_asm_7 ENDP

END
