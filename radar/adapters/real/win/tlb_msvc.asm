; tlb_msvc.asm — MSVC MASM wrappers for INVEPT and INVVPID (x64)
;
; Intel SDM INVEPT/INVVPID set CF/ZF on failure. These wrappers expose the
; instruction with a C-friendly int return (0 = success, 1 = failure) and the
; descriptor passed by pointer, mirroring how syscall_msvc.asm wraps syscalls.
;
; Signature (extern "C"):
;   int invept_asm(unsigned int type, const void* descriptor);
;   int invvpid_asm(unsigned int type, const void* descriptor);
;
; Parameter layout at entry (x64 C calling convention):
;   RCX = type, RDX = descriptor pointer

.code

;------------------------------------------------------------------------------
; int invept_asm(unsigned int type, const void* descriptor)
;------------------------------------------------------------------------------
invept_asm PROC
    mov     rax, rdx            ; descriptor -> RAX (address operand source)
    ; x64 INVEPT/INVVPID require a 64-bit type register (ML64 rejects ECX).
    mov     ecx, ecx            ; zero-extend type into RCX
    invept  rcx, oword ptr [rax]
    setna   al                  ; AL = 1 on failure (CF or ZF set)
    movzx   eax, al             ; zero-extend to int return
    ret
invept_asm ENDP

;------------------------------------------------------------------------------
; int invvpid_asm(unsigned int type, const void* descriptor)
;------------------------------------------------------------------------------
invvpid_asm PROC
    mov     rax, rdx            ; descriptor -> RAX
    mov     ecx, ecx            ; zero-extend type into RCX
    invvpid rcx, oword ptr [rax]
    setna   al                  ; AL = 1 on failure (CF or ZF set)
    movzx   eax, al             ; zero-extend to int return
    ret
invvpid_asm ENDP

END
