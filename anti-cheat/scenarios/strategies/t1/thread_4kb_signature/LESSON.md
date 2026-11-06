# thread_4kb_signature — Thread 4KB Signature Buffer Evasion

Family: Evasion. Tiers: T1.

## Battlefield
Red: Creates a thread whose start address points to clean code.
Blue: CS2's MonitorThreadContext copies 4KB of memory at the thread start
      address and sends it to the server for analysis.

## Real AC Context
CS2's CDllVerificationMonitor::MonitorThreadContext does:
1. On DLL_THREAD_ATTACH, captures thread start address
2. Copies 4KB of memory at that address (signature_buffer)
3. Collects: memory protection, module handle, return address chain
4. Sends this data to the server (Message 159)

If the 4KB buffer contains:
- Known cheat signatures (byte patterns)
- Calls to OpenProcess/ReadProcessMemory
- Suspicious strings ("cheat", "radar", "esp")
- RWX memory (PAGE_EXECUTE_READWRITE = 0x40)

Then the server flags it.

## Technique
Red must start threads at an address where the first 4KB is CLEAN:
- Inside a legitimate loaded module (e.g., kernel32.dll)
- Via a proxy function in the game's own code
- With a short jump to the real code (the 4KB must look legitimate)

## Lesson
Thread start address must be a legitimate module. The 4KB at that
address must not contain suspicious patterns. This is why thread
hijacking (starting in kernel32) still fails — the 4KB of kernel32
code is clean, but the RETURN ADDRESS back to red's code is also
captured.
