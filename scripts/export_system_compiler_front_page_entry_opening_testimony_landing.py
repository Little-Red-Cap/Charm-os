from __future__ import annotations

import argparse
import json
from collections import OrderedDict
from datetime import datetime
from pathlib import Path
from typing import Any

from system_compiler_front_page_route_lib import (
    choose_text,
    get_mapping,
    load_json,
    normalize_optional_path,
    normalize_path,
    resolve_output_path,
    write_text,
)


OPEN_EVENT_WITNESS_SCHEMA = "system_compiler.front_page_entry_opening_flow_open_event_witness/v0"
OPEN_EVENT_WITNESS_KIND = "system_compiler.front_page_entry_opening_flow_open_event_witness"
OPENING_TESTIMONY_LANDING_SCHEMA = "system_compiler.front_page_entry_opening_testimony_landing/v0"
OPENING_TESTIMONY_LANDING_KIND = "system_compiler.front_page_entry_opening_testimony_landing"


def get_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def ordered_unique(values: list[str]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for value in values:
        text = choose_text(value)
        if not text or text in seen:
            continue
        seen.add(text)
        result.append(text)
    return result


def load_open_event_witness_summary(path: Path) -> dict[str, Any]:
    summary = load_json(path)
    if choose_text(summary.get("schema")) != OPEN_EVENT_WITNESS_SCHEMA:
        raise ValueError(f"unsupported open-event witness schema: {path}")
    if choose_text(summary.get("kind")) != OPEN_EVENT_WITNESS_KIND:
        raise ValueError(f"unsupported open-event witness kind: {path}")
    return summary


def make_surface(
    surface_id: str,
    label: str,
    role: str,
    summary_schema: str,
    summary_path: str,
    report_markdown_path: str,
    check_text_path: str,
) -> OrderedDict[str, str]:
    return OrderedDict(
        [
            ("id", surface_id),
            ("label", label),
            ("role", role),
            ("summary_schema", summary_schema),
            ("summary_path", summary_path),
            ("report_markdown_path", report_markdown_path),
            ("check_text_path", check_text_path),
        ]
    )


def normalize_evidence_ref(value: Any) -> OrderedDict[str, str]:
    ref = get_mapping(value)
    return OrderedDict(
        [
            ("role", choose_text(ref.get("role"))),
            ("summary_schema", choose_text(ref.get("summary_schema"))),
            ("summary_path", normalize_optional_path(ref.get("summary_path"))),
            ("report_markdown_path", normalize_optional_path(ref.get("report_markdown_path"))),
            ("check_text_path", normalize_optional_path(ref.get("check_text_path"))),
        ]
    )


def get_source_report_path(witness_summary: dict[str, Any]) -> str:
    artifact_context = get_mapping(witness_summary.get("artifact_context"))
    front_page = get_mapping(witness_summary.get("front_page"))
    return normalize_optional_path(
        artifact_context.get("report_markdown_path") or front_page.get("report_markdown_path")
    )


def get_source_check_path(witness_summary: dict[str, Any]) -> str:
    artifact_context = get_mapping(witness_summary.get("artifact_context"))
    front_page = get_mapping(witness_summary.get("front_page"))
    return normalize_optional_path(artifact_context.get("check_text_path") or front_page.get("check_text_path"))


def build_source_witness_ref(
    witness_summary: dict[str, Any],
    witness_summary_path: Path,
) -> OrderedDict[str, str]:
    return OrderedDict(
        [
            ("summary_schema", OPEN_EVENT_WITNESS_SCHEMA),
            ("summary_kind", OPEN_EVENT_WITNESS_KIND),
            ("summary_path", normalize_path(witness_summary_path)),
            ("report_markdown_path", get_source_report_path(witness_summary)),
            ("check_text_path", get_source_check_path(witness_summary)),
        ]
    )


def build_front_page(
    summary_path: Path,
    report_path: Path,
    check_path: Path,
    witness_summary: dict[str, Any],
    witness_summary_path: Path,
) -> OrderedDict[str, Any]:
    source_ref = build_source_witness_ref(witness_summary, witness_summary_path)
    supporting_surfaces: list[OrderedDict[str, str]] = [
        make_surface(
            "source_open_event_witness",
            "source open-event witness",
            "source_open_event_witness",
            source_ref["summary_schema"],
            source_ref["summary_path"],
            source_ref["report_markdown_path"],
            source_ref["check_text_path"],
        )
    ]

    for index, ref_value in enumerate(get_list(witness_summary.get("evidence_refs"))):
        ref = normalize_evidence_ref(ref_value)
        role = choose_text(ref.get("role")) or f"evidence_ref_{index}"
        supporting_surfaces.append(
            make_surface(
                f"witness_evidence_ref_{index}_{role}",
                f"witness evidence ref: {role}",
                f"witness_evidence_ref:{role}",
                ref["summary_schema"],
                ref["summary_path"],
                ref["report_markdown_path"],
                ref["check_text_path"],
            )
        )

    return OrderedDict(
        [
            ("summary_path", normalize_path(summary_path)),
            ("report_markdown_path", normalize_path(report_path)),
            ("check_text_path", normalize_path(check_path)),
            ("supporting_surfaces", supporting_surfaces),
        ]
    )


def build_artifact_targets(witness_summary: dict[str, Any]) -> OrderedDict[str, Any]:
    witness_entry = get_mapping(witness_summary.get("witness_entry"))
    evidence_refs = [normalize_evidence_ref(ref_value) for ref_value in get_list(witness_summary.get("evidence_refs"))]
    artifact_refs = ordered_unique(
        [normalize_optional_path(path_value) for path_value in get_list(witness_entry.get("artifact_refs"))]
    )
    return OrderedDict(
        [
            ("evidence_refs", evidence_refs),
            ("witness_artifact_refs", artifact_refs),
            ("evidence_ref_count", len(evidence_refs)),
            ("witness_artifact_ref_count", len(artifact_refs)),
        ]
    )


def build_violations(witness_summary: dict[str, Any]) -> list[str]:
    violations: list[str] = []
    judgment = get_mapping(witness_summary.get("judgment"))
    witness_entry = get_mapping(witness_summary.get("witness_entry"))
    explanation = get_mapping(witness_summary.get("explanation"))
    source_path = normalize_optional_path(witness_entry.get("source_path"))
    text_lines = [choose_text(item) for item in get_list(explanation.get("text_lines")) if choose_text(item)]

    if choose_text(witness_summary.get("result")) != "ok":
        violations.append("source witness result is not ok")
    if choose_text(judgment.get("witness_status")) != "ok":
        violations.append("source witness judgment.witness_status is not ok")
    if not source_path:
        violations.append("witness_entry.source_path is missing")
    elif not Path(source_path).exists():
        violations.append(f"witness_entry.source_path is not found: {source_path}")
    if not text_lines:
        violations.append("explanation.text_lines is empty")
    return ordered_unique(violations)


def build_testimony_preview(witness_summary: dict[str, Any]) -> OrderedDict[str, Any]:
    identity = get_mapping(witness_summary.get("open_event_identity"))
    judgment = get_mapping(witness_summary.get("judgment"))
    witness_entry = get_mapping(witness_summary.get("witness_entry"))
    explanation = get_mapping(witness_summary.get("explanation"))
    source_judgment_summary = choose_text(judgment.get("source_judgment_summary"))
    explanation_lines = [choose_text(item) for item in get_list(explanation.get("text_lines")) if choose_text(item)]
    observation_lines = [choose_text(item) for item in get_list(witness_entry.get("observations")) if choose_text(item)]
    headline = "Opening testimony for {0}".format(
        choose_text(identity.get("open_event_id")) or choose_text(judgment.get("witness_id")) or "open-event witness"
    )
    summary_lines = ordered_unique(
        [
            source_judgment_summary,
            *explanation_lines[:3],
            *observation_lines[:3],
        ]
    )
    return OrderedDict(
        [
            ("headline", headline),
            ("source_judgment_summary", source_judgment_summary),
            ("summary_lines", summary_lines),
            ("explanation_text_lines", explanation_lines),
            ("observation_lines", observation_lines),
            ("summary_line_count", len(summary_lines)),
            ("explanation_line_count", len(explanation_lines)),
            ("observation_count", len(observation_lines)),
        ]
    )


def build_next_questions() -> list[OrderedDict[str, str]]:
    return [
        OrderedDict(
            [
                ("kind", "inspect_open_event"),
                ("summary", "Inspect the source open event carried by this testimony."),
                ("target_ref", "opening_identity.source_open_event_summary_path"),
            ]
        ),
        OrderedDict(
            [
                ("kind", "inspect_evidence_refs"),
                ("summary", "Inspect the evidence refs already declared by the open-event witness."),
                ("target_ref", "artifact_targets.evidence_refs"),
            ]
        ),
        OrderedDict(
            [
                ("kind", "compare_open_event_witness"),
                ("summary", "Compare this open-event witness against another testimony before promoting the landing."),
                ("target_ref", "source_witness_ref.summary_path"),
            ]
        ),
    ]


def build_summary_model(
    witness_summary_path: Path,
    output_root: Path,
    summary_path: Path,
    report_path: Path,
    check_path: Path,
) -> OrderedDict[str, Any]:
    witness_summary = load_open_event_witness_summary(witness_summary_path)
    artifact_context = get_mapping(witness_summary.get("artifact_context"))
    identity = get_mapping(witness_summary.get("open_event_identity"))
    judgment = get_mapping(witness_summary.get("judgment"))
    violations = build_violations(witness_summary)
    landing_status = "blocked" if violations else "ready"
    result = "ok" if landing_status == "ready" else "fail"
    source_ref = build_source_witness_ref(witness_summary, witness_summary_path)
    source_open_event_summary_path = normalize_optional_path(
        artifact_context.get("source_open_event_summary_path") or identity.get("source_summary_path")
    )

    return OrderedDict(
        [
            ("schema", OPENING_TESTIMONY_LANDING_SCHEMA),
            ("kind", OPENING_TESTIMONY_LANDING_KIND),
            ("generator", "scripts/export_system_compiler_front_page_entry_opening_testimony_landing.py"),
            ("result", result),
            (
                "opening_testimony_landing",
                OrderedDict(
                    [
                        ("title", "System Compiler Front Page Entry Opening Testimony Landing"),
                        ("summary", "A thin explain-entry landing projected from one open-event witness testimony."),
                    ]
                ),
            ),
            ("front_page", build_front_page(summary_path, report_path, check_path, witness_summary, witness_summary_path)),
            (
                "artifact_context",
                OrderedDict(
                    [
                        ("source_witness_summary_path", normalize_path(witness_summary_path)),
                        ("source_witness_report_markdown_path", source_ref["report_markdown_path"]),
                        ("source_witness_check_text_path", source_ref["check_text_path"]),
                        ("output_root", normalize_path(output_root)),
                        ("landing_summary_path", normalize_path(summary_path)),
                        ("report_markdown_path", normalize_path(report_path)),
                        ("check_text_path", normalize_path(check_path)),
                    ]
                ),
            ),
            ("source_witness_ref", source_ref),
            (
                "opening_identity",
                OrderedDict(
                    [
                        ("open_event_id", choose_text(identity.get("open_event_id"))),
                        ("open_event_status", choose_text(identity.get("open_event_status"))),
                        ("open_event_result", choose_text(identity.get("open_event_result"))),
                        ("reason_kind", choose_text(identity.get("reason_kind"))),
                        ("reason_summary", choose_text(identity.get("reason_summary"))),
                        ("source_judgment_status", choose_text(judgment.get("source_judgment_status"))),
                        ("source_judgment_grade", choose_text(judgment.get("source_judgment_grade"))),
                        ("source_witness_id", choose_text(judgment.get("witness_id"))),
                        ("source_witness_status", choose_text(judgment.get("witness_status"))),
                        ("source_open_event_summary_path", source_open_event_summary_path),
                    ]
                ),
            ),
            (
                "landing_decision",
                OrderedDict(
                    [
                        ("status", landing_status),
                        ("selected_entry_id", "open-event-witness"),
                        ("selected_tab_id", "opening_testimony"),
                        ("selected_role", "opening_testimony"),
                        (
                            "opening_reason",
                            OrderedDict(
                                [
                                    ("kind", "open_event_witness_testimony"),
                                    (
                                        "summary",
                                        "Open this testimony as the explain entry for its source opening judgment.",
                                    ),
                                    ("source_summary_path", normalize_path(witness_summary_path)),
                                ]
                            ),
                        ),
                    ]
                ),
            ),
            ("testimony_preview", build_testimony_preview(witness_summary)),
            ("artifact_targets", build_artifact_targets(witness_summary)),
            ("next_questions", build_next_questions()),
            ("violations", violations),
        ]
    )


def build_report(summary: dict[str, Any]) -> str:
    identity = summary["opening_identity"]
    decision = summary["landing_decision"]
    preview = summary["testimony_preview"]
    targets = summary["artifact_targets"]
    lines: list[str] = [
        "# System Compiler Front Page Entry Opening Testimony Landing",
        "",
        f"- Result: `{summary['result']}`",
        f"- Landing status: `{decision['status']}`",
        f"- Source witness: `{summary['source_witness_ref']['summary_path']}`",
        f"- Selected entry: `{decision['selected_entry_id']}` tab=`{decision['selected_tab_id']}` role=`{decision['selected_role']}`",
        "",
        "## Opening Identity",
        f"- open event: `{identity['open_event_id']}` status=`{identity['open_event_status']}` result=`{identity['open_event_result']}`",
        f"- reason: `{identity['reason_kind']}` {identity['reason_summary']}",
        f"- source judgment: status=`{identity['source_judgment_status']}` grade=`{identity['source_judgment_grade']}`",
        f"- source witness: `{identity['source_witness_id']}` status=`{identity['source_witness_status']}`",
        "",
        "## Testimony Preview",
        f"- {preview['headline']}",
    ]
    if preview["source_judgment_summary"]:
        lines.append(f"- judgment: {preview['source_judgment_summary']}")
    for line in preview["summary_lines"]:
        lines.append(f"- {line}")

    lines.extend(["", "## Artifact Targets"])
    lines.append(f"- evidence refs: `{targets['evidence_ref_count']}`")
    for ref in targets["evidence_refs"]:
        lines.append(f"- `{ref['role']}`: `{ref['summary_path']}`")
    lines.append(f"- witness artifact refs: `{targets['witness_artifact_ref_count']}`")

    lines.extend(["", "## Next Questions"])
    for question in summary["next_questions"]:
        lines.append(f"- `{question['kind']}`: {question['summary']} ({question['target_ref']})")

    if summary["violations"]:
        lines.extend(["", "## Violations"])
        for violation in summary["violations"]:
            lines.append(f"- {violation}")
    return "\n".join(lines) + "\n"


def build_check(summary: dict[str, Any]) -> str:
    identity = summary["opening_identity"]
    decision = summary["landing_decision"]
    targets = summary["artifact_targets"]
    return "\n".join(
        [
            f"source_witness_summary_path: {summary['artifact_context']['source_witness_summary_path']}",
            f"result: {summary['result']}",
            f"landing_status: {decision['status']}",
            f"selected_entry_id: {decision['selected_entry_id']}",
            f"selected_tab_id: {decision['selected_tab_id']}",
            f"selected_role: {decision['selected_role']}",
            f"open_event_id: {identity['open_event_id']}",
            f"open_event_status: {identity['open_event_status']}",
            f"source_judgment_status: {identity['source_judgment_status']}",
            f"source_judgment_grade: {identity['source_judgment_grade']}",
            f"source_witness_status: {identity['source_witness_status']}",
            f"evidence_ref_count: {targets['evidence_ref_count']}",
            f"witness_artifact_ref_count: {targets['witness_artifact_ref_count']}",
            f"violations: {'|'.join(summary['violations'])}",
        ]
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export an explain-entry landing from one opening-flow open-event witness summary."
    )
    parser.add_argument("--witness", required=True, help="Input open-event witness summary JSON.")
    parser.add_argument("--output-root", default="", help="Output root for opening testimony landing artifacts.")
    parser.add_argument("--summary", default="", help="Explicit output path for opening testimony landing summary JSON.")
    parser.add_argument("--report-markdown", default="", help="Explicit output path for opening testimony landing markdown report.")
    parser.add_argument("--check-text", default="", help="Explicit output path for opening testimony landing check text.")
    args = parser.parse_args()

    witness_path = Path(args.witness).resolve()
    output_root = Path(args.output_root or "out/system-compiler-front-page-entry-opening-testimony-landing").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    summary_path = resolve_output_path(args.summary, output_root, "front-page.entry-opening-testimony.landing.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "front-page.entry-opening-testimony.landing.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "front-page.entry-opening-testimony.landing.check.txt")

    try:
        summary = build_summary_model(witness_path, output_root, summary_path, report_path, check_path)
        summary["generated_at_utc"] = datetime.utcnow().replace(microsecond=0).isoformat() + "Z"
        write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        write_text(report_path, build_report(summary))
        write_text(check_path, build_check(summary))
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    decision = summary["landing_decision"]
    identity = summary["opening_identity"]
    print(f"[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING] summary={summary_path}")
    print(
        "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING] status={0} event_status={1} witness_status={2}".format(
            decision["status"],
            identity["source_judgment_status"],
            identity["source_witness_status"],
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
