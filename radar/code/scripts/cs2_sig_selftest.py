"""Self-test suite for CS2 signature update pure transforms."""
from __future__ import annotations

import sys
from pathlib import Path

from cs2_sig_common import FIELD_MAP, GLOBAL_KEYS
from cs2_sig_emit import emit_hpp
from cs2_sig_fetch import (
    build_snapshot,
    diff_snapshots,
    field_lookup,
    load_json_bytes,
    load_json_path,
)

def _assert(cond: bool, msg: str) -> None:
    if not cond:
        raise AssertionError(msg)


def run_self_test(script_dir: Path, code_dir: Path) -> int:
    """
    Exercise build_snapshot / field_lookup / diff_snapshots / emit_hpp on real
    fixture inputs and (when present) on-disk lab dumps. Returns 0/1.
    """
    print("=== update_cs2_signatures --self-test ===")
    failures = 0
    fixtures = script_dir / "fixtures"

    def check(name: str, fn) -> None:
        nonlocal failures
        try:
            fn()
            print(f"  OK  {name}")
        except Exception as exc:  # noqa: BLE001 — collect all self-test failures
            failures += 1
            print(f"  FAIL  {name}: {exc}", file=sys.stderr)

    # 1) Minimal fixture builds a complete snapshot with critical globals.
    def t_minimal_build() -> None:
        offsets = load_json_path(fixtures / "minimal_offsets.json")
        client = load_json_path(fixtures / "minimal_client_dll.json")
        snap = build_snapshot(
            offsets,
            client,
            "fixture://minimal",
            generated_at="2000-01-01T00:00:00Z",
        )
        _assert(snap["globals"]["dwEntityList"] == 39124592, "dwEntityList value")
        _assert(snap["globals"]["dwLocalPlayerPawn"] == 37376568, "dwLocalPlayerPawn")
        _assert(snap["fields"]["m_iHealth"] == 844, "m_iHealth")
        _assert(snap["fields"]["m_angEyeAngles"] == 13120, "m_angEyeAngles")
        _assert(snap["fields"]["m_vecAbsOrigin"] == 200, "m_vecAbsOrigin")
        # Extended FIELD_MAP set (entities.cpp / live_radar_stack.cpp)
        _assert(snap["fields"]["m_bDormant"] == 259, "m_bDormant")
        _assert(snap["fields"]["m_ArmorValue"] == 7324, "m_ArmorValue")
        _assert(snap["fields"]["m_entitySpottedState"] == 7256, "m_entitySpottedState")
        _assert(snap["fields"]["m_bSpotted"] == 8, "m_bSpotted")
        _assert(snap["fields"]["m_pObserverServices"] == 4640, "m_pObserverServices")
        _assert(snap["fields"]["m_iObserverMode"] == 72, "m_iObserverMode")
        _assert(snap["fields"]["m_bBombTicking"] == 4512, "m_bBombTicking")
        _assert(snap["fields"]["m_nBombSite"] == 4516, "m_nBombSite")
        _assert(snap["fields"]["m_flC4Blow"] == 4560, "m_flC4Blow")
        _assert(snap["fields"]["m_bBeingDefused"] == 4572, "m_bBeingDefused")
        _assert(
            snap["constants"]["entity_identity_stride"] == 0x70,
            "entity_identity_stride",
        )
        for key in GLOBAL_KEYS:
            _assert(key in snap["globals"], f"global key present: {key}")
        for _cls, name in FIELD_MAP:
            _assert(name in snap["fields"], f"field key present: {name}")
        _assert(
            len(snap["fields"]) >= len({n for _c, n in FIELD_MAP}),
            "snapshot carries full FIELD_MAP field count",
        )

    check("fixture build_snapshot critical globals+fields", t_minimal_build)

    # 2) Missing critical schema field raises from shipped build_snapshot.
    def t_missing_field() -> None:
        offsets = load_json_path(fixtures / "minimal_offsets.json")
        broken = load_json_path(fixtures / "broken_client_dll.json")
        raised = False
        try:
            build_snapshot(offsets, broken, "fixture://broken")
        except RuntimeError as exc:
            raised = True
            _assert(
                "Missing schema field" in str(exc),
                f"error should name missing field, got: {exc}",
            )
        _assert(raised, "expected RuntimeError for missing schema field")

    check("missing critical field raises", t_missing_field)

    # 3) Missing critical global raises.
    def t_missing_global() -> None:
        offsets = {"client.dll": {"dwViewMatrix": 1}}  # no entity list / pawn
        client = load_json_path(fixtures / "minimal_client_dll.json")
        raised = False
        build_snapshot._quiet_missing = True  # type: ignore[attr-defined]
        try:
            try:
                build_snapshot(offsets, client, "fixture://no-crit")
            except RuntimeError as exc:
                raised = True
                _assert(
                    "critical global" in str(exc).lower()
                    or "dwEntityList" in str(exc)
                    or "dwLocalPlayerPawn" in str(exc),
                    f"error should mention critical global, got: {exc}",
                )
        finally:
            build_snapshot._quiet_missing = False  # type: ignore[attr-defined]
        _assert(raised, "expected RuntimeError for missing critical global")

    check("missing critical global raises", t_missing_global)

    # 4) field_lookup preferred class then fallback.
    def t_field_lookup() -> None:
        classes = {
            "C_BaseEntity": {"fields": {"m_iHealth": 100}},
            "Other": {"fields": {"m_iTeamNum": 7}},
        }
        _assert(field_lookup(classes, "C_BaseEntity", "m_iHealth") == 100, "preferred")
        _assert(field_lookup(classes, "Missing", "m_iTeamNum") == 7, "fallback")
        _assert(field_lookup(classes, "X", "nope") is None, "absent")

    check("field_lookup preferred + fallback", t_field_lookup)

    # 5) diff_snapshots lists real section.key changes.
    def t_diff() -> None:
        base_off = load_json_path(fixtures / "minimal_offsets.json")
        alt_off = load_json_path(fixtures / "alt_offsets.json")
        client = load_json_path(fixtures / "minimal_client_dll.json")
        a = build_snapshot(base_off, client, "a", generated_at="t1")
        b = build_snapshot(alt_off, client, "b", generated_at="t2")
        changes = diff_snapshots(a, b)
        _assert(changes, "expected non-empty diff")
        joined = "\n".join(changes)
        _assert("globals.dwEntityList" in joined, "diff names dwEntityList")
        _assert("0x254FE70" in joined or "39124592" in joined or "->" in joined, "diff values")
        # equal snapshots → empty change list
        same = diff_snapshots(a, dict(a, generated_at_utc="other-ts"))
        _assert(same == [], f"timestamp-only change should not appear: {same}")
        first = diff_snapshots(None, a)
        _assert(first == ["(no previous snapshot)"], "no previous")

    check("diff_snapshots lists section.key changes", t_diff)

    # 6) emit_hpp embeds ALL FIELD_MAP keys (core + extended) from snapshot.
    def t_emit() -> None:
        offsets = load_json_path(fixtures / "minimal_offsets.json")
        client = load_json_path(fixtures / "minimal_client_dll.json")
        snap = build_snapshot(
            offsets, client, "fixture://hpp", generated_at="2000-01-01T00:00:00Z"
        )
        hpp = emit_hpp(snap)
        _assert("#pragma once" in hpp, "pragma")
        _assert("namespace real::cs2::snapshot" in hpp, "namespace")
        _assert("dwEntityList = 0x254FE70" in hpp, "entity list hex")
        _assert("m_iHealth = 0x34C" in hpp, "health hex")
        _assert("entity_identity_stride = 0x70" in hpp, "stride")
        _assert('kGeneratedAt = "2000-01-01T00:00:00Z"' in hpp, "timestamp")
        # Extended symbols must appear — incomplete 8-field emit cannot pass.
        _assert("m_bDormant = 0x103" in hpp, "extended m_bDormant in header")
        _assert("m_ArmorValue = 0x1C9C" in hpp, "extended m_ArmorValue in header")
        _assert("m_entitySpottedState = 0x1C58" in hpp, "extended m_entitySpottedState")
        _assert("m_bSpotted = 0x8" in hpp, "extended m_bSpotted in header")
        _assert("m_pObserverServices = 0x1220" in hpp, "extended m_pObserverServices")
        _assert("m_iObserverMode = 0x48" in hpp, "extended m_iObserverMode")
        _assert("m_bBombTicking = 0x11A0" in hpp, "extended m_bBombTicking")
        _assert("m_nBombSite = 0x11A4" in hpp, "extended m_nBombSite")
        _assert("m_flC4Blow = 0x11D0" in hpp, "extended m_flC4Blow")
        _assert("m_bBeingDefused = 0x11DC" in hpp, "extended m_bBeingDefused")
        for _cls, name in FIELD_MAP:
            _assert(
                f"inline constexpr std::uint64_t {name} =" in hpp,
                f"emit_hpp must include FIELD_MAP key {name}",
            )
        # Regression: never ship a header that only has the original 8 core fields.
        core_only = {
            "m_iHealth", "m_iTeamNum", "m_lifeState", "m_pGameSceneNode",
            "m_vOldOrigin", "m_angEyeAngles", "m_hPlayerPawn", "m_vecAbsOrigin",
        }
        field_count = sum(
            1 for line in hpp.splitlines()
            if line.strip().startswith("inline constexpr std::uint64_t m_")
        )
        _assert(
            field_count >= len(core_only) + 5,
            f"header field count {field_count} too small (must include extended set)",
        )

    check("emit_hpp embeds snapshot values", t_emit)

    # 7) On-disk lab dumps (if present) produce critical globals via shipped path.
    def t_ondisk() -> None:
        data = code_dir / "data" / "cs2"
        off_p = data / "offsets.json"
        cli_p = data / "client_dll.json"
        if not off_p.is_file() or not cli_p.is_file():
            print("  SKIP  on-disk data/cs2 dumps not present")
            return
        offsets = load_json_path(off_p)
        client = load_json_path(cli_p)
        snap = build_snapshot(offsets, client, "on-disk://data/cs2")
        _assert(snap["globals"]["dwEntityList"] > 0, "on-disk dwEntityList")
        _assert(snap["globals"]["dwLocalPlayerPawn"] > 0, "on-disk dwLocalPlayerPawn")
        for _cls, name in FIELD_MAP:
            _assert(snap["fields"][name] > 0, f"on-disk field {name}")
        # Round-trip: diff against existing snapshot should be comparable
        snap_p = data / "offsets_snapshot.json"
        if snap_p.is_file():
            prev = load_json_path(snap_p)
            changes = diff_snapshots(prev, snap)
            # Not asserting empty — dumps may lag snapshot — just that diff runs.
            _assert(isinstance(changes, list), "diff returns list")
            print(f"       on-disk vs installed snapshot: {len(changes)} value change(s)")

    check("on-disk data/cs2 dumps via build_snapshot", t_ondisk)

    # 8) load_json_bytes rejects garbage.
    def t_bad_json() -> None:
        raised = False
        try:
            load_json_bytes(b"not-json{")
        except ValueError:
            raised = True
        _assert(raised, "bad JSON must raise ValueError")

    check("load_json_bytes rejects garbage", t_bad_json)

    print()
    if failures:
        print(f"SELF-TEST FAILED: {failures} case(s)", file=sys.stderr)
        return 1
    print("SELF-TEST PASSED")
    return 0
