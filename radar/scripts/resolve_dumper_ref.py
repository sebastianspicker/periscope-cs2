#!/usr/bin/env python3
"""resolve_dumper_ref.py — pick the newest trusted a2x/cs2-dumper output ref.

R8 research item, adapted from the cs2-dma release offset-sync
workflow (Apache-2.0, NOTICE recorded). Hardening preserved:

- Only PRs whose author GitHub considers trusted for the upstream repository
  (OWNER / MEMBER / COLLABORATOR) may become an automatic source.
- A future timestamp beyond a small skew guard is rejected, so a PR cannot
  win timestamp comparison permanently with a year-2099 date.
- The main branch's info.json is always a candidate; open trusted PRs win
  only when strictly newer.

Pure decision logic is separated from network I/O and covered by --self-test
(inline fixtures, no network), mirroring scripts/update_cs2_signatures.py.

Usage:
  python3 radar/scripts/resolve_dumper_ref.py --repo a2x/cs2-dumper
  python3 radar/scripts/resolve_dumper_ref.py --self-test
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import sys
import urllib.parse
import urllib.request
from datetime import datetime, timedelta, timezone
from typing import Any

TRUSTED_ASSOCIATIONS = {"OWNER", "MEMBER", "COLLABORATOR"}
MAX_FUTURE_SKEW = timedelta(hours=6)
DEFAULT_REPO = "a2x/cs2-dumper"


class Selection:
    """The chosen upstream ref for an offset sync run."""

    __slots__ = ("kind", "ref", "sha", "timestamp", "label", "url_base")

    def __init__(
        self,
        kind: str,
        ref: str,
        sha: str,
        timestamp: str,
        label: str,
    ) -> None:
        self.kind = kind  # "main" or "pull"
        self.ref = ref
        self.sha = sha
        self.timestamp = timestamp
        self.label = label

    def url_base_for(self, repo: str) -> str:
        encoded_ref = urllib.parse.quote(self.ref, safe="")
        return (
            f"https://raw.githubusercontent.com/{repo}/{encoded_ref}/output/"
        )


def parse_timestamp(value: object) -> datetime | None:
    """Parses an ISO-8601 info.json timestamp; None when unusable."""
    if not isinstance(value, str) or not value.strip():
        return None
    normalized = value.strip().replace("Z", "+00:00")
    try:
        parsed = datetime.fromisoformat(normalized)
    except (TypeError, ValueError):
        return None
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


def is_newer(candidate: datetime, current: datetime | None) -> bool:
    """Strictly newer than the current best (None current always loses to a
    parsed candidate)."""
    if candidate is None:
        return False
    return current is None or candidate > current


def future_skew_rejected(candidate: datetime, now: datetime) -> bool:
    """A timestamp more than MAX_FUTURE_SKEW ahead of `now` is rejected."""
    return candidate > now + MAX_FUTURE_SKEW


def _selection_from(info: dict[str, Any], kind: str, ref: str, sha: str,
                    label: str) -> Selection | None:
    timestamp = info.get("timestamp") if isinstance(info, dict) else None
    if parse_timestamp(timestamp) is None:
        return None
    if not sha:
        return None
    return Selection(kind, ref, sha, str(timestamp), label)


def select_ref(
    main_info: dict[str, Any],
    pulls: list[dict[str, Any]],
    now: datetime,
    head_info_for_pull: dict[int, dict[str, Any]] | None = None,
) -> tuple[Selection | None, list[str]]:
    """Picks the newest trusted ref.

    `pulls` is the GitHub REST pulls list (state=open). `head_info_for_pull`
    maps PR number -> that head's parsed info.json (injected by the caller or
    by tests); a PR without a usable head info.json is skipped.

    Returns (selection | None, notes). selection is None only when main has
    no usable info.json at all.
    """
    notes: list[str] = []
    best = _selection_from(main_info, "main", "main", "main", "main")
    best_dt = parse_timestamp(best.timestamp) if best else None
    if best is None:
        notes.append("main info.json unusable")

    head_info_for_pull = head_info_for_pull or {}
    for pull in pulls:
        number = pull.get("number")
        label = f"PR #{number}"
        if not isinstance(number, int):
            continue
        if pull.get("draft"):
            notes.append(f"{label}: skipped (draft)")
            continue
        if pull.get("author_association") not in TRUSTED_ASSOCIATIONS:
            notes.append(f"{label}: skipped (untrusted association)")
            continue
        head = pull.get("head") or {}
        head_repo = (head.get("repo") or {}).get("full_name")
        head_sha = head.get("sha")
        if not head_repo or not head_sha:
            notes.append(f"{label}: skipped (missing head repo or sha)")
            continue
        info = head_info_for_pull.get(number)
        if info is None:
            notes.append(f"{label}: skipped (no usable head info.json)")
            continue
        candidate = _selection_from(
            info, "pull", str(head_sha), head_sha, label
        )
        if candidate is None:
            notes.append(f"{label}: skipped (head info.json timestamp unusable)")
            continue
        candidate_dt = parse_timestamp(candidate.timestamp)
        assert candidate_dt is not None  # guaranteed by _selection_from
        if future_skew_rejected(candidate_dt, now):
            notes.append(f"{label}: skipped (future timestamp beyond skew)")
            continue
        if is_newer(candidate_dt, best_dt):
            best = candidate
            best_dt = candidate_dt
            notes.append(f"{label}: becomes candidate ({candidate.timestamp})")
        else:
            notes.append(f"{label}: not newer than current best")

    return best, notes


def decode_contents(payload: Any) -> Any:
    """Unwraps a GitHub contents-API response into the parsed file body."""
    if isinstance(payload, dict) and "content" in payload:
        return json.loads(base64.b64decode(payload["content"]).decode("utf-8"))
    return payload


def fetch_json(url: str, token: str, max_bytes: int = 2 * 1024 * 1024) -> Any:
    headers = {
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "periscope-radar-offset-sync",
    }
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)  # noqa: S310
    with urllib.request.urlopen(request, timeout=20) as response:  # noqa: S310
        data = response.read(max_bytes + 1)
    if len(data) > max_bytes:
        raise RuntimeError(f"response from {url} exceeds {max_bytes} bytes")
    return json.loads(data.decode("utf-8"))


def fetch_all_pulls(repo: str, token: str) -> list[dict[str, Any]]:
    pulls: list[dict[str, Any]] = []
    page = 1
    while True:
        url = (
            f"https://api.github.com/repos/{repo}/pulls"
            f"?state=open&per_page=100&page={page}"
        )
        batch = fetch_json(url, token)
        if not isinstance(batch, list) or not batch:
            break
        pulls.extend(item for item in batch if isinstance(item, dict))
        if len(batch) < 100:
            break
        page += 1
        if page > 10:  # bounded: 1000 open PRs is far beyond realistic
            break
    return pulls


def self_test() -> int:
    """Exercises the pure selection logic against inline fixtures."""
    failures = 0

    def check(ok: bool, what: str) -> None:
        nonlocal failures
        if not ok:
            print(f"FAIL {what}")
            failures += 1

    now = datetime(2026, 11, 5, 12, 0, 0, tzinfo=timezone.utc)
    main_info = {"timestamp": "2026-11-05T10:00:00Z"}

    def pr(number: int, association: str, ts: str, draft: bool = False,
           head_repo: str = "someone/cs2-dumper", head_sha: str = "abc") -> dict:
        return {
            "number": number,
            "draft": draft,
            "author_association": association,
            "head": {"repo": {"full_name": head_repo}, "sha": head_sha},
            "_ts": ts,
        }

    def heads(*prs: dict) -> dict[int, dict[str, Any]]:
        return {p["number"]: {"timestamp": p["_ts"]} for p in prs}

    # Trusted newer PR wins.
    selection, notes = select_ref(
        main_info,
        [pr(1, "MEMBER", "2026-11-05T11:00:00Z")],
        now,
        heads(pr(1, "MEMBER", "2026-11-05T11:00:00Z")),
    )
    check(selection is not None and selection.kind == "pull", "trusted PR wins")
    check(selection is not None and selection.sha == "abc", "PR sha carried")

    # Untrusted association never wins.
    selection, _ = select_ref(
        main_info,
        [pr(2, "FIRST_TIME_CONTRIBUTOR", "2026-11-05T11:30:00Z")],
        now,
        heads(pr(2, "FIRST_TIME_CONTRIBUTOR", "2026-11-05T11:30:00Z")),
    )
    check(selection is not None and selection.kind == "main",
          "untrusted PR ignored")

    # Draft ignored.
    selection, _ = select_ref(
        main_info,
        [pr(3, "OWNER", "2026-11-05T11:30:00Z", draft=True)],
        now,
        heads(pr(3, "OWNER", "2026-11-05T11:30:00Z", draft=True)),
    )
    check(selection is not None and selection.kind == "main", "draft ignored")

    # Future skew rejected (year-2099 attack).
    selection, _ = select_ref(
        main_info,
        [pr(4, "OWNER", "2099-01-01T00:00:00Z")],
        now,
        heads(pr(4, "OWNER", "2099-01-01T00:00:00Z")),
    )
    check(selection is not None and selection.kind == "main", "skew guarded")

    # Older PR stays below main.
    selection, _ = select_ref(
        main_info,
        [pr(5, "COLLABORATOR", "2026-11-05T09:00:00Z")],
        now,
        heads(pr(5, "COLLABORATOR", "2026-11-05T09:00:00Z")),
    )
    check(selection is not None and selection.kind == "main", "older PR ignored")

    # Newest of several trusted PRs wins.
    selection, _ = select_ref(
        main_info,
        [
            pr(6, "MEMBER", "2026-11-05T10:30:00Z"),
            pr(7, "MEMBER", "2026-11-05T11:45:00Z"),
        ],
        now,
        heads(
            pr(6, "MEMBER", "2026-11-05T10:30:00Z"),
            pr(7, "MEMBER", "2026-11-05T11:45:00Z"),
        ),
    )
    check(selection is not None and selection.label == "PR #7",
          "newest trusted PR wins")

    # Missing head info.json skips the PR.
    selection, _ = select_ref(
        main_info, [pr(8, "OWNER", "2026-11-05T11:50:00Z")], now, {}
    )
    check(selection is not None and selection.kind == "main",
          "PR without head info skipped")

    # URL base is pinned to the resolved SHA for pulls, to the ref for main.
    pull_sel, _ = select_ref(
        main_info,
        [pr(1, "MEMBER", "2026-11-05T11:00:00Z", head_sha="abc123")],
        now,
        heads(pr(1, "MEMBER", "2026-11-05T11:00:00Z", head_sha="abc123")),
    )
    check(pull_sel is not None and
          pull_sel.url_base_for(DEFAULT_REPO) ==
          "https://raw.githubusercontent.com/a2x/cs2-dumper/abc123/output/",
          "pull url base pins the sha")
    main_sel, _ = select_ref(main_info, [], now)
    check(main_sel is not None and
          main_sel.url_base_for(DEFAULT_REPO).endswith("/main/output/"),
          "main url base")

    # Broken main info yields None selection with a note.
    empty, notes = select_ref({}, [], now)
    check(empty is None, "unusable main yields None")
    check(any("unusable" in note for note in notes), "unusable noted")

    if failures != 0:
        print(f"{failures} resolver check(s) failed")
        return 1
    print("resolve_dumper_ref: all checks passed")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Pick the newest trusted a2x/cs2-dumper output ref.",
    )
    parser.add_argument("--repo", default=DEFAULT_REPO)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument(
        "--json-out",
        help="write the selection as JSON to this path (default: stdout)",
    )
    args = parser.parse_args(argv)

    if args.self_test:
        return self_test()

    token = os.environ.get("GH_TOKEN", "")
    now = datetime.now(timezone.utc)
    try:
        main_info = decode_contents(
            fetch_json(
                f"https://api.github.com/repos/{args.repo}/contents/"
                f"output/info.json?ref=main",
                token,
            )
        )
        pulls = fetch_all_pulls(args.repo, token)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"resolve error: {error}", file=sys.stderr)
        return 1

    head_infos: dict[int, dict[str, Any]] = {}
    for pull in pulls:
        head = pull.get("head") or {}
        head_repo = (head.get("repo") or {}).get("full_name")
        head_sha = head.get("sha")
        if not pull.get("draft") and head_repo and head_sha:
            if pull.get("author_association") in TRUSTED_ASSOCIATIONS:
                try:
                    head_infos[pull["number"]] = decode_contents(
                        fetch_json(
                            f"https://api.github.com/repos/{head_repo}/"
                            f"contents/output/info.json?ref={head_sha}",
                            token,
                        )
                    )
                except (OSError, RuntimeError, ValueError):
                    head_infos.pop(pull["number"], None)

    selection, notes = select_ref(main_info, pulls, now, head_infos)
    for note in notes:
        print(note)
    if selection is None:
        print("no usable upstream ref", file=sys.stderr)
        return 1

    result = {
        "kind": selection.kind,
        "ref": selection.ref,
        "sha": selection.sha,
        "label": selection.label,
        "timestamp": selection.timestamp,
        "url_base": selection.url_base_for(args.repo),
    }
    rendered = json.dumps(result, indent=2, sort_keys=True)
    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as handle:
            handle.write(rendered + "\n")
    else:
        print(rendered)
    return 0


if __name__ == "__main__":
    sys.exit(main())
