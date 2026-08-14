# fps — Lab FPS demo surface

Sim-only mini FPS world used by demos/fps_demo.cpp and residual demos. Not a strategy
pair. It exercises shared sim + residual AC wiring with a Counter-Strike-inspired
bomb-defuse round so radar advantage has a game-shaped story.

Explicitly not Valve netcode and not a 3D client. Positions, teams, alive flags,
and bomb phase are enough for fog and entity-read pedagogy.

## What lives here

- `fps::Map` — dusty_yard areas and sites A/B with plant radii.
- `fps::Scenario` — freeze/buy, live, plant/defuse channels, ground bomb, economy,
  elimination and timer outcomes.
- `fps::lab_bridge` — living (or all) players to `ac::EntitySnapshot`, and sync
  into `sim::World` game memory for RPM/IOCTL/DMA-shaped readers.

## CS-like rules (what the lab models)

### Freeze / buy and money

Rounds open in Buy (freeze). Money is per player. Defenders may buy a defuse kit;
either side may buy a rifle when funds allow. Freeze clock counts down into Live
(or demos skip freeze). RoundEnd applies win/loss money and optional plant bonus
so the next round’s buy phase has a CS-ish economy slice without claiming
competitive parity.

### Bomb drop and pickup

Before plant, one attacker carries the bomb. On carrier death the bomb becomes a
ground object at their feet. Living attackers in pickup range can reclaim it;
defenders cannot. Plant still needs a carrier inside a site radius.

### Defuse kit and weapons

Kit shortens the hold-to-defuse duration (configured bare vs kit times). Weapon
class is optional damage flavor for the damage/kill path; it is not a full gun
game. Instant plant/defuse helpers still enforce team, site, planted state, and
carrier rules so stubs cannot skip the story.

### Win paths (read carefully)

1. Explode — fuse hits zero; attackers win.
2. Defuse — defenders finish the defuse channel; defenders win.
3. Eliminate attackers — all Ts dead and bomb not planted; defenders win.
4. Eliminate defenders — all CTs dead under elim rules; attackers win.
5. Time — live clock out with no plant; defenders win.

If the bomb is planted and all attackers die, the fuse continues. Defenders must
defuse or lose to explode. That planted-fuse exception is the main CS rule that
keeps entity and bomb-state reads meaningful after a wipe.

### lab_bridge still maps players to World

`lab_bridge` does not simulate bullets or netcode. It maps Scenario players into
EntitySnapshot lists and into sim::World entity bytes so red strategies that need
entities (T0 CheatClient, T2 KernelRadar, T4 DmaRadar) plant or consume lab
entity bytes via World.plant_lab_entities and RPM/IOCTL/DMA backends. fps_demo
visualizes the advantage of reading more than interest fog allows.

## RED interest

Red strategies that need entities (T0 CheatClient, T2 KernelRadar, T4 DmaRadar)
plant or consume lab entity bytes via World.plant_lab_entities and
RPM/IOCTL/DMA backends. fps_demo visualizes the advantage: bomb carrier and
site positions matter more when the round story is real.

## BLUE interest

Blue structural fog (server_sends_full_enemy_origin) and info-advantage scorers
shrink what a radar can see even when client scars are weak. A CS-like bomb round
gives fog something concrete to cull (enemy origins off interest, post-death
noise) without inventing a second game engine.

## Run

```bash
./build/fps_demo
./build/fps_demo all
./build/fps_tests
./build/strategy_lab stats
```

## Related

- code/include/ + code/src/sim — World scars
- code/include/ + code/src/server — InterestManager, InfoAdvantageScorer, BanCorrelator
- code/demos/fps_demo.cpp — interactive residual demo
- `code/strategies/` + `./build/strategy_lab`
- [README.md](README.md) — package surface and rule tables
