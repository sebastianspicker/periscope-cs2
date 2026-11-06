# 32 — CS round radar + fog

## Lesson

**Myth:** Radar always has every player XY because the client holds a full entity list.  
**Truth:** On a CS-shaped round (sites, teams, plant), server interest/fog should not replicate unobservable enemies. Even after red RPM succeeds on bridged scenario memory, blue multi-reason (handle + cull + fidelity budget) starves the product.

## Pipeline

1. `fps::Scenario` on `dusty_yard`: freeze skip, plant site A  
2. `make_lab_world_from_scenario` → game process entity table  
3. Red: `OpenProcess(VmRead)` + read count/ents (scenario-derived teams/alive)  
4. Blue: InterestManager from CT spawn culls far Ts; `apply_client_fidelity_budget`  

## Sim-only

Not Valve netcode. Uses shipped Scenario + lab_bridge + interest_mgmt APIs.
