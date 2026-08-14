# Simulation Model

## World
`sim::World` is the lab's educational OS and game-state simulation. It holds
simulated processes, handles, memory regions, modules, and overlay windows.
All lab operations mutate this model only; the lab makes no real OS or game
calls.

## Red Strategies
Red strategies operate on `sim::World` and produce signals that represent the
observable effects of their simulated actions. Their reports describe the
signals left in the shared model.

## Blue Strategies
Blue strategies scan `sim::World` for anomalies through independent observation
views, such as handle, memory, or behavior views. A detection requires two or
more independent views to observe corroborating anomalies. Blue reports record
the risk score and the observations supporting it.

## Duel Flow
1. **Setup**: Create `sim::World` and initialize the simulated game state.
2. **Red**: Run a red strategy and collect its simulated signals.
3. **Blue**: Run blue strategies against the shared world state.
4. **Score**: Compare red signals with blue detections.
5. **Report**: Record the outcome and the observation views that contributed.

## Tiers
- T0: Direct RPM (simplest, most detectable)
- T1: Syscall (bypasses ntdll, handle still visible)
- T2: Kernel driver (medium complexity)
- T3: Hypervisor (low detectability)
- T4: DMA physical (hardware-level)
