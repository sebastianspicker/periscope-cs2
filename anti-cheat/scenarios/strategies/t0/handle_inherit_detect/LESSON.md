# handle_inherit_detect — Process Handle Inheritance Detection

Family: Detection. Tiers: T0.

## Battlefield
Red: Creates a child process that inherits the game handle. The handle
     is visible in the child process — no need for the child to call OpenProcess.
Blue: Detects handles inherited from parent processes. A process with
     a VM_READ handle to cs2.exe, whose parent also has a handle, is suspicious.

## Real AC Context
When a process is created with CreateProcess and bInheritHandles=TRUE,
all inheritable handles are duplicated into the child process. Anti-cheats
check:
1. Does this process have a VM_READ handle to cs2.exe?
2. Does its parent also have such a handle?
3. Is this process a legitimate child of the parent?

Red can evade by:
- Creating the child process WITHOUT handle inheritance
- Duplicating the handle with DUPLICATE_CLOSE_SOURCE (parent loses it)
- Using a proxy process that never creates a visible child

## Lesson
Handle inheritance reveals the parent-child relationship. Both must
be considered in the handle graph analysis.
