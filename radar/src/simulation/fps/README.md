# Dummy FPS — Counter-Strike-inspired bomb defuse lab

Educational stand-in game for the anti-cheat-legit-radar lab. Headless C++
scenario on a simple map (`dusty_yard`): two teams (attackers plant, defenders
defuse), freeze/buy economy, bomb drop and pickup, and full plant → fuse →
defuse / explode / eliminate / time outcomes.

This is sim-only. It is not Valve netcode, not a multiplayer client, and not a
3D engine. Rules are CS-flavored enough that entity positions, teams, alive
flags, and round story matter for legit-radar pedagogy.

## Why it exists

Red/blue strategies need a concrete game-shaped surface: players with origins,
bomb state that moves across the map, and a round that produces unfair-info
value when someone reads more entity data than the server should send. This
package is that surface.

| Piece | Role |
|-------|------|
| `fps::Map` | Named areas + bomb sites A/B with plant radii |
| `fps::Scenario` | Phases, plant/defuse channels, drop/pickup, economy, outcomes |
| `fps::lab_bridge` | Players → `ac::EntitySnapshot` and `sim::World` game memory |

## Quick start

```bash
cmake -S radar -B radar/build/dev
cmake --build radar/build/dev --target fps_demo
./radar/build/dev/fps_demo all
```

## Demo modes

```text
./radar/build/dev/fps_demo plant_defuse   # T plant A → CT defuse
./radar/build/dev/fps_demo plant_explode  # T plant B → fuse out
./radar/build/dev/fps_demo lab_bridge     # entities into sim::World
./radar/build/dev/fps_demo all
```

## Rules (CS-like, lab implementation)

### Phases

A round starts in Buy (freeze). During freeze, players hold money, may buy a
defuse kit (defenders) or weapons, and the live clock does not run. When freeze
ends (timer or explicit end), the phase becomes Live. After a successful plant,
the phase is BombPlanted and the fuse owns the critical timer. RoundEnd freezes
actions and applies money for the next round.

### Bomb carrier, drop, pickup

Exactly one living attacker may carry the bomb before plant. If that carrier
dies, the bomb drops on the ground at their origin. Attackers in pickup range
may pick it up; defenders cannot. Plant still requires a living carrier standing
inside a site plant radius (hold-to-plant or demo instant helpers that still
enforce team/site/carrier rules).

### Plant and defuse

Plant: living attacker, carries bomb, inside site plant radius, Live phase.
Defuse: living defender, bomb already planted, within range of bomb origin,
BombPlanted phase. Leaving range or dying cancels the in-progress channel.

Defuse kit: optional defender item (buy cost from RoundConfig). With a kit, the
effective hold-to-defuse time is shorter (kit time vs bare time). Instant defuse
helpers still require a valid defuser; kit primarily affects the timed channel.

### Weapon class and damage (optional flavor)

Players have a WeaponClass (none / pistol / rifle / AWP) and money. Buy rifle
or kit spends money when rules allow. Damage can use weapon class defaults or a
raw override. This is enough for elimination demos; it is not a ballistics model.

### Round-end money

On RoundEnd, money updates by outcome: win reward, loss reward, and a small
plant bonus for attackers when the plant succeeded (even if the round continues
to fuse resolution). Start money seeds a new roster. Numbers live in RoundConfig
and stay lab-tunable, not competitive-match exact.

### Win paths

| Outcome | When |
|---------|------|
| Attackers win explode | Fuse reaches zero with bomb still planted and not defused |
| Attackers win eliminate | All defenders dead while rules still allow T elim win |
| Defenders win defuse | Defuse completes on planted bomb |
| Defenders win eliminate | All attackers dead and bomb is not planted |
| Defenders win time | Live round clock expires with no plant |

Planted fuse exception (CS-like): if the bomb is already planted and all
attackers die, the round does not end as a defender eliminate win. Defenders
must still defuse, or the fuse explodes for an attacker win. Time win only
applies when the bomb was never planted.

### lab_bridge

`to_entity_snapshots` maps scenario players to `ac::EntitySnapshot` (team,
origin, alive) for interest management and info-advantage demos.
`sync_entities_to_sim` / `make_lab_world_from_scenario` write those players into
`sim::World` game-process memory in the same layout as `plant_lab_entities`, so
T0-style RPM readers and higher-tier radar scars can consume a bomb-round story
instead of anonymous fixtures. The bridge does not reimplement netcode or 3D.

## Honesty

- Sim-only educational rules. Not CS:GO/CS2, not Source, not Valve networking.
- The demo keeps wrong-team, off-site plant, pre-plant defuse, invalid pickup,
  and related guards in the shipped scenario code.

## Link to anti-cheat lab

After `make_lab_world_from_scenario`, a T0-style reader can OpenProcess +
ReadProcessMemory the lab game process and pull entity XY — same path as
`01_external_rpm`, with a real bomb-round narrative.

See also: [LESSON.md](LESSON.md).
