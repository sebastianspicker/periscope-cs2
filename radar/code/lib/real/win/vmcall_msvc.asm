; vmcall_msvc.asm — MSVC MASM Hyper-V VMCALL wrapper for x64
;
; Executes the VMCALL instruction with the Hyper-V fast-hypercall convention:
;   RCX = input value
;   RDX = param1
;   R8  = param2
;   RAX = result
;
; The x64 C calling convention already passes the first three arguments in
; RCX, RDX, and R8, so no register shuffling is required. VMCALL must be
; executed at CPL=0 (or with a valid VMX context and guest-privileged access).

.code

;------------------------------------------------------------------------------
; uint64_t vmcall_asm(uint64_t input, uint64_t param1, uint64_t param2)
;------------------------------------------------------------------------------
vmcall_asm PROC
    vmcall
    ret
vmcall_asm ENDP

END
