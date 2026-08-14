# ret_addr_spoof — Return Address Chain Spoofing

Family: Evasion. Tiers: T1.

## Battlefield
Red: Starts a thread with a legitimate address (kernel32, game DLL), but the
     RETURN ADDRESS chain still leads back to red's allocated memory.
Blue: CS2's MonitorThreadContext doesn't just check the start address — it
     also captures the return address chain via RtlWalkFrameChain / stack walking.
     Even if the start address is clean, the return addresses on the stack
     reveal red's true code location.

## Real AC Context
CS2 captures:
1. Thread start address (via NtQueryInformationThread)
2. Return address chain (via stack walking / RtlWalkFrameChain)
3. Module handle for each return address
4. Memory protection of each return address

If a return address points to:
- RWX memory (PAGE_EXECUTE_READWRITE)
- Memory with no backing module
- A known cheat module

Then the thread is flagged even if the start address is clean.

## Technique
Red must spoof the return address chain:
- Use a ROP chain (Return-Oriented Programming) to redirect execution
- Ensure EVERY return address leads to a legitimate module
- No return addresses to RWX pages or unmapped memory

## Lesson
Start address is not enough. The ENTIRE return address chain must be
legitimate. This is why simple thread hijacking always fails.
