import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SCHEMA = "charm.compiler_lifecycle.summary/v0"
KIND = "charm.compiler_lifecycle.summary"
GENERATOR = "scripts/export_compiler_lifecycle_summary.py"

LIFECYCLE_STATES = [
    "declared",
    "materialized",
    "proven",
    "frozen",
    "lowered",
    "witnessed",
    "observed",
    "archived",
    "compared",
]


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def as_dict(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def as_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def source_status(kind: str, data: dict[str, Any]) -> str:
    if kind == "kernel_runtime_session":
        return str(as_dict(data.get("verdict")).get("session_status") or "unknown")
    if kind == "witness_bundle":
        return str(data.get("result") or "unknown")
    if kind == "world_compare":
        verdict = str(data.get("world_verdict") or "").strip()
        result = str(data.get("result") or "unknown")
        return f"{result}/{verdict}" if verdict else result
    if kind == "runtime_ledger":
        events = as_list(data.get("events"))
        return f"events={len(events)}"
    if kind == "artifact_report_index":
        headline = as_dict(data.get("compiler_headline"))
        return str(headline.get("status") or data.get("mode") or "present")
    if kind == "artifact_report":
        formation = as_dict(data.get("system_formation"))
        return str(formation.get("status") or data.get("mode") or "present")
    return "present"


def make_surface(kind: str, path: Path, data: dict[str, Any]) -> dict[str, Any]:
    return {
        "kind": kind,
        "path": str(path),
        "schema": data.get("schema"),
        "status": source_status(kind, data),
    }


def load_surface(path_value: str, kind: str, violations: list[str]) -> tuple[Path | None, dict[str, Any] | None]:
    if not path_value.strip():
        return None, None

    path = Path(path_value).resolve()
    if not path.exists():
        violations.append(f"{kind}: input not found -> {path}")
        return path, None

    try:
        data = load_json(path)
    except Exception as exc:
        violations.append(f"{kind}: failed to read JSON -> {path}: {exc}")
        return path, None

    return path, data


def make_state(
    status: str,
    coverage_strength: str,
    projection_kind: str,
    sidecar_gap: str,
    source_surfaces: list[dict[str, Any]],
    notes: list[str],
) -> dict[str, Any]:
    return {
        "status": status,
        "coverage_strength": coverage_strength,
        "projection_kind": projection_kind,
        "sidecar_gap": sidecar_gap,
        "source_surfaces": source_surfaces,
        "notes": notes,
    }


def present_or_missing(sources: list[dict[str, Any]]) -> str:
    return "present" if sources else "missing"


def render_report(summary: dict[str, Any]) -> str:
    lines = [
        "# Compiler Lifecycle Summary",
        "",
        f"- Schema: `{summary['schema']}`",
        f"- Result: `{summary['result']}`",
        f"- Generated at: `{summary['generated_at_utc']}`",
        f"- State count: `{summary['state_count']}`",
        "",
        "## States",
        "",
        "State | Status | Coverage | Projection | Sidecar gap | Sources",
        "--- | --- | --- | --- | --- | ---",
    ]

    states = as_dict(summary.get("states"))
    for name in LIFECYCLE_STATES:
        state = as_dict(states.get(name))
        source_count = len(as_list(state.get("source_surfaces")))
        lines.append(
            " | ".join(
                [
                    name,
                    str(state.get("status")),
                    str(state.get("coverage_strength")),
                    str(state.get("projection_kind")),
                    str(state.get("sidecar_gap")),
                    str(source_count),
                ]
            )
        )

    violations = as_list(summary.get("violations"))
    lines += ["", "## Violations", ""]
    if not violations:
        lines.append("- none")
    else:
        lines.extend(f"- {item}" for item in violations)

    return "\n".join(lines) + "\n"


def render_check(summary: dict[str, Any]) -> str:
    states = as_dict(summary.get("states"))
    lines = [
        f"summary: {summary.get('artifact_paths', {}).get('summary')}",
        f"result: {summary.get('result')}",
        f"state_count: {summary.get('state_count')}",
        f"frozen_status: {as_dict(states.get('frozen')).get('status')}",
        f"lowered_projection_kind: {as_dict(states.get('lowered')).get('projection_kind')}",
        f"archived_coverage_strength: {as_dict(states.get('archived')).get('coverage_strength')}",
        f"observed_status: {as_dict(states.get('observed')).get('status')}",
        f"violation_count: {len(as_list(summary.get('violations')))}",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export a read-only compiler lifecycle summary sidecar from existing exported surfaces."
    )
    parser.add_argument("--artifact-report-index", action="append", default=[])
    parser.add_argument("--artifact-report", action="append", default=[])
    parser.add_argument("--kernel-runtime-session", default="")
    parser.add_argument("--runtime-ledger", default="")
    parser.add_argument("--witness-bundle", default="")
    parser.add_argument("--world-compare", default="")
    parser.add_argument(
        "--output",
        default="out/compiler-lifecycle-summary/compiler_lifecycle.summary.json",
    )
    parser.add_argument("--report-markdown", default="")
    parser.add_argument("--check-text", default="")
    args = parser.parse_args()

    output_path = Path(args.output).resolve()
    report_path = Path(args.report_markdown).resolve() if args.report_markdown else output_path.with_suffix(".md")
    check_path = Path(args.check_text).resolve() if args.check_text else output_path.with_suffix(".check.txt")

    violations: list[str] = []
    artifact_surfaces: list[dict[str, Any]] = []

    for value in args.artifact_report_index:
        path, data = load_surface(value, "artifact_report_index", violations)
        if path is not None and data is not None:
            artifact_surfaces.append(make_surface("artifact_report_index", path, data))

    for value in args.artifact_report:
        path, data = load_surface(value, "artifact_report", violations)
        if path is not None and data is not None:
            artifact_surfaces.append(make_surface("artifact_report", path, data))

    session_surface: list[dict[str, Any]] = []
    session_data: dict[str, Any] = {}
    session_path, loaded_session = load_surface(args.kernel_runtime_session, "kernel_runtime_session", violations)
    if session_path is not None and loaded_session is not None:
        session_data = loaded_session
        session_surface.append(make_surface("kernel_runtime_session", session_path, loaded_session))

    ledger_surface: list[dict[str, Any]] = []
    ledger_path, ledger_data = load_surface(args.runtime_ledger, "runtime_ledger", violations)
    if ledger_path is not None and ledger_data is not None:
        ledger_surface.append(make_surface("runtime_ledger", ledger_path, ledger_data))

    witness_surface: list[dict[str, Any]] = []
    witness_path, witness_data = load_surface(args.witness_bundle, "witness_bundle", violations)
    if witness_path is not None and witness_data is not None:
        witness_surface.append(make_surface("witness_bundle", witness_path, witness_data))

    compare_surface: list[dict[str, Any]] = []
    compare_path, compare_data = load_surface(args.world_compare, "world_compare", violations)
    if compare_path is not None and compare_data is not None:
        compare_surface.append(make_surface("world_compare", compare_path, compare_data))

    session_ledger_notes: list[str] = []
    ledger_projection = as_dict(session_data.get("ledger"))
    if ledger_projection:
        event_count = ledger_projection.get("event_count")
        runtime_ledger = ledger_projection.get("runtime_ledger")
        session_ledger_notes.append(
            f"kernel_runtime_session ledger projection present: event_count={event_count} runtime_ledger={runtime_ledger}"
        )

    observed_sources = ledger_surface[:]
    if session_ledger_notes and session_surface:
        observed_sources.extend(session_surface)
    if not observed_sources and artifact_surfaces:
        observed_sources.extend(artifact_surfaces)

    states = {
        "declared": make_state(
            present_or_missing(artifact_surfaces),
            "strong" if artifact_surfaces else "missing",
            "mixed" if artifact_surfaces else "missing",
            "not_required",
            artifact_surfaces,
            ["artifact report/index projects declared input facts"] if artifact_surfaces else ["no artifact report/index input"],
        ),
        "materialized": make_state(
            present_or_missing(artifact_surfaces),
            "strong" if artifact_surfaces else "missing",
            "mixed" if artifact_surfaces else "missing",
            "not_required",
            artifact_surfaces,
            ["artifact report/index projects materialized report surfaces"] if artifact_surfaces else ["no artifact report/index input"],
        ),
        "proven": make_state(
            present_or_missing(session_surface),
            "strong" if session_surface else "missing",
            "direct" if session_surface else "missing",
            "optional",
            session_surface,
            ["kernel runtime session verdict is consumed as an exported proof surface"] if session_surface else ["no kernel runtime session input"],
        ),
        "frozen": make_state(
            "missing",
            "missing",
            "interpretive",
            "recommended",
            [],
            ["no freeze receipt input is supported in v0; lifecycle summary must not fabricate frozen coverage"],
        ),
        "lowered": make_state(
            present_or_missing(artifact_surfaces),
            "medium" if artifact_surfaces else "missing",
            "interpretive" if artifact_surfaces else "missing",
            "optional",
            artifact_surfaces,
            ["artifact report/index is read as a lowered explanation surface, not as world truth"] if artifact_surfaces else ["no lowered surface input"],
        ),
        "witnessed": make_state(
            present_or_missing(session_surface + witness_surface),
            "strong" if (session_surface or witness_surface) else "missing",
            "mixed" if (session_surface or witness_surface) else "missing",
            "not_required",
            session_surface + witness_surface,
            ["session summary and/or witness bundle project witnessed state"] if (session_surface or witness_surface) else ["no witness surface input"],
        ),
        "observed": make_state(
            present_or_missing(observed_sources),
            "strong" if observed_sources else "missing",
            "direct" if observed_sources else "missing",
            "not_required",
            observed_sources,
            (session_ledger_notes or ["runtime ledger or session ledger projection projects observed state"])
            if observed_sources
            else ["no runtime ledger, session ledger projection, or artifact evidence input"],
        ),
        "archived": make_state(
            present_or_missing(witness_surface),
            "weak_to_medium" if witness_surface else "missing",
            "interpretive" if witness_surface else "missing",
            "optional",
            witness_surface,
            ["witness bundle is read as a weak-to-medium archive projection, not an archive manifest"] if witness_surface else ["no witness bundle input"],
        ),
        "compared": make_state(
            present_or_missing(compare_surface),
            "strong" if compare_surface else "missing",
            "direct" if compare_surface else "missing",
            "optional",
            compare_surface,
            ["world compare is consumed as exported compare surface"] if compare_surface else ["no world compare input"],
        ),
    }

    state_names = list(states.keys())
    if state_names != LIFECYCLE_STATES:
        violations.append("internal state order does not match lifecycle state vocabulary")

    summary = {
        "schema": SCHEMA,
        "kind": KIND,
        "generated_at_utc": utc_now(),
        "generator": GENERATOR,
        "result": "ok" if not violations else "fail",
        "state_count": len(states),
        "states": states,
        "source_context": {
            "artifact_report_index": args.artifact_report_index,
            "artifact_report": args.artifact_report,
            "kernel_runtime_session": args.kernel_runtime_session or None,
            "runtime_ledger": args.runtime_ledger or None,
            "witness_bundle": args.witness_bundle or None,
            "world_compare": args.world_compare or None,
        },
        "artifact_paths": {
            "summary": str(output_path),
            "report_markdown": str(report_path),
            "check_text": str(check_path),
        },
        "violations": violations,
    }

    try:
        write_json(output_path, summary)
        write_text(report_path, render_report(summary))
        write_text(check_path, render_check(summary))
    except Exception as exc:
        print(f"[ERROR] failed to write lifecycle summary sidecar: {exc}", file=sys.stderr)
        return 1

    print(f"[OK] compiler lifecycle summary -> {output_path}")
    print(f"[OK] result -> {summary['result']}")
    return 0 if summary["result"] == "ok" else 1


if __name__ == "__main__":
    raise SystemExit(main())
