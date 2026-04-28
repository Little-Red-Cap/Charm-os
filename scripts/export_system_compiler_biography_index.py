import argparse
import json
from datetime import datetime
from pathlib import Path


BIOGRAPHY_SCHEMA = "system_compiler.biography/v0"
BIOGRAPHY_KIND = "system_compiler.biography"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_text(path: Path, text: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def resolve_output_path(explicit: str, output_root: Path, default_name: str) -> Path:
    if explicit:
        return Path(explicit).resolve()
    return (output_root / default_name).resolve()


def build_surface_ref(
    surface_id: str,
    summary_schema: str,
    label: str,
    role: str,
    summary_path: str,
    report_markdown_path: str,
    check_text_path: str,
):
    return {
        "id": surface_id,
        "label": label,
        "role": role,
        "summary_schema": summary_schema,
        "summary_path": str(Path(summary_path).resolve()),
        "report_markdown_path": str(Path(report_markdown_path).resolve()),
        "check_text_path": str(Path(check_text_path).resolve()),
    }


def build_front_page(summary_path: Path, report_path: Path, check_path: Path, supporting_surfaces: list[dict]):
    return {
        "summary_path": str(summary_path.resolve()),
        "report_markdown_path": str(report_path.resolve()),
        "check_text_path": str(check_path.resolve()),
        "supporting_surfaces": supporting_surfaces,
    }


def ordered_unique(values):
    seen = set()
    result = []
    for value in values:
        if value in seen:
            continue
        seen.add(value)
        result.append(value)
    return result


def string_list(values):
    return ordered_unique(
        str(value)
        for value in (values or [])
        if value is not None and str(value).strip()
    )


def nullable_text(value):
    if value is None:
        return None
    text = str(value)
    return text if text else None


def verdict_label(value):
    return nullable_text(value) or "not-attached"


def load_biography(path: Path):
    data = load_json(path)
    if data.get("schema") != BIOGRAPHY_SCHEMA:
        raise ValueError(f"unsupported biography schema: {path}")
    if data.get("kind") != BIOGRAPHY_KIND:
        raise ValueError(f"unsupported biography kind: {path}")
    return data


def get_front_page_surface(biography: dict, surface_id: str):
    front_page = biography.get("front_page", {}) or {}
    supporting_surfaces = front_page.get("supporting_surfaces", []) or []
    for surface in supporting_surfaces:
        if not isinstance(surface, dict):
            continue
        if str(surface.get("id", "")).strip() != surface_id:
            continue
        return surface
    return None


def resolve_summary_route(
    biography: dict,
    artifact_context: dict,
    *,
    surface_id: str,
    artifact_key: str,
    fallback_value=None,
):
    surface = get_front_page_surface(biography, surface_id)
    if surface is not None and surface.get("summary_path"):
        return str(Path(surface["summary_path"]).resolve())

    artifact_value = artifact_context.get(artifact_key)
    if artifact_value:
        return str(Path(artifact_value).resolve())

    if fallback_value:
        return str(Path(fallback_value).resolve())

    return None


def make_entry_id(world_name: str, verdict: str, compare_attached: bool, seen: dict[str, int]) -> str:
    base = f"{world_name}:{verdict}:{'compare' if compare_attached else 'witness'}"
    seen[base] = seen.get(base, 0) + 1
    if seen[base] == 1:
        return base
    return f"{base}#{seen[base]}"


def build_entry(biography: dict, biography_path: Path, seen: dict[str, int]):
    world = biography.get("world", {})
    world_subject = world.get("subject", {})
    biography_block = biography.get("biography", {})
    artifact_context = biography.get("artifact_context", {})
    world_compare_block = biography.get("world_compare") or {}
    compare_attached = biography.get("world_compare") is not None
    verdict = nullable_text(biography.get("world_verdict"))

    runtime_evidence_summary = resolve_summary_route(
        biography,
        artifact_context,
        surface_id="runtime_evidence",
        artifact_key="runtime_evidence_summary",
    )
    witness_bundle_summary = resolve_summary_route(
        biography,
        artifact_context,
        surface_id="witness_bundle",
        artifact_key="witness_bundle_summary",
    )
    world_compare_summary = resolve_summary_route(
        biography,
        artifact_context,
        surface_id="world_compare",
        artifact_key="world_compare_summary",
        fallback_value=world_compare_block.get("summary_path"),
    )

    if runtime_evidence_summary is None:
        raise ValueError(f"biography is missing runtime evidence route: {biography_path}")
    if witness_bundle_summary is None:
        raise ValueError(f"biography is missing witness bundle route: {biography_path}")
    if compare_attached and world_compare_summary is None:
        raise ValueError(f"compare-attached biography is missing world compare route: {biography_path}")

    return {
        "id": make_entry_id(
            str(world.get("name", "")),
            verdict_label(verdict),
            compare_attached,
            seen,
        ),
        "profile": str(biography.get("profile", "")),
        "world_name": str(world.get("name", "")),
        "world_title": str(world.get("title", "")),
        "board": nullable_text(world_subject.get("board")),
        "active_facets": string_list(world_subject.get("active_facets", [])),
        "result": str(biography.get("result", "fail")),
        "world_verdict": verdict,
        "compare_attached": compare_attached,
        "identity": str(biography_block.get("identity", "")),
        "thesis": str(biography_block.get("thesis", "")),
        "evidence_path": string_list(biography_block.get("evidence_path", [])),
        "next_questions": string_list(biography_block.get("next_questions", [])),
        "summary_path": str(biography_path),
        "report_markdown_path": str(Path(biography["delivery"]["report_markdown_path"]).resolve()),
        "check_text_path": str(Path(biography["delivery"]["check_text_path"]).resolve()),
        "runtime_evidence_summary": runtime_evidence_summary,
        "witness_bundle_summary": witness_bundle_summary,
        "world_compare_summary": world_compare_summary,
    }


def build_biography_surface(entry: dict, biography: dict, biography_path: Path):
    front_page = biography.get("front_page", {})
    delivery = biography.get("delivery", {})
    world = biography.get("world", {})
    world_name = str(world.get("name", "")).strip()
    world_title = str(world.get("title", "")).strip()
    label_anchor = world_name or world_title or entry["id"]
    return build_surface_ref(
        surface_id=entry["id"],
        summary_schema=BIOGRAPHY_SCHEMA,
        label=f"world biography: {label_anchor}",
        role="shelf_entry",
        summary_path=front_page.get("summary_path") or delivery.get("summary_path") or str(biography_path),
        report_markdown_path=front_page.get("report_markdown_path") or delivery["report_markdown_path"],
        check_text_path=front_page.get("check_text_path") or delivery["check_text_path"],
    )


def build_index_summary(entries):
    return {
        "biography_count": len(entries),
        "unique_world_count": len({entry["world_name"] for entry in entries}),
        "ok_count": sum(1 for entry in entries if entry["result"] == "ok"),
        "fail_count": sum(1 for entry in entries if entry["result"] != "ok"),
        "compare_attached_count": sum(1 for entry in entries if entry["compare_attached"]),
        "not_attached_count": sum(1 for entry in entries if not entry["compare_attached"]),
        "standing_count": sum(1 for entry in entries if entry["world_verdict"] == "standing"),
        "improved_count": sum(1 for entry in entries if entry["world_verdict"] == "improved"),
        "drifted_count": sum(1 for entry in entries if entry["world_verdict"] == "drifted"),
        "collapsed_count": sum(1 for entry in entries if entry["world_verdict"] == "collapsed"),
    }


def build_questions(biographies):
    core_questions = []
    compare_questions = []
    next_questions = []
    for biography in biographies:
        questions = biography.get("questions", {})
        core_questions.extend(questions.get("core_questions", []))
        compare_questions.extend(questions.get("compare_questions", []))
        next_questions.extend(questions.get("next_questions", []))
    return {
        "core_questions": string_list(core_questions),
        "compare_questions": string_list(compare_questions),
        "next_questions": string_list(next_questions),
    }


def extend_question_lines(report_lines: list[str], prefix: str, values):
    for question in values:
        report_lines.append(f"- {prefix}: {question}")


def build_report(summary: dict):
    report_lines = [
        "# System Compiler Biography Index",
        "",
        f"- Result: `{summary['result']}`",
        f"- Profile: `{summary['profile']}`",
        f"- Shelf: `{summary['shelf']['title']}`",
        f"- Summary JSON: `{summary['delivery']['summary_path']}`",
        "",
        "## Shelf",
        f"- Summary: {summary['shelf']['summary']}",
        "- Counts: `biographies={0} worlds={1} compare_attached={2} not_attached={3}`".format(
            summary["summary"]["biography_count"],
            summary["summary"]["unique_world_count"],
            summary["summary"]["compare_attached_count"],
            summary["summary"]["not_attached_count"],
        ),
        "- Verdicts: `standing={0} improved={1} drifted={2} collapsed={3}`".format(
            summary["summary"]["standing_count"],
            summary["summary"]["improved_count"],
            summary["summary"]["drifted_count"],
            summary["summary"]["collapsed_count"],
        ),
        "- Results: `ok={0} fail={1}`".format(
            summary["summary"]["ok_count"],
            summary["summary"]["fail_count"],
        ),
        "",
        "## Entries",
        "Id | Verdict | Result | World | Board | Compare",
        "--- | --- | --- | --- | --- | ---",
    ]

    for entry in summary["entries"]:
        report_lines.append(
            "{0} | {1} | {2} | {3} | {4} | {5}".format(
                entry["id"],
                verdict_label(entry["world_verdict"]),
                entry["result"],
                entry["world_name"],
                entry["board"] or "",
                "yes" if entry["compare_attached"] else "no",
            )
        )

    for entry in summary["entries"]:
        report_lines.extend(
            [
                "",
                f"## Entry `{entry['id']}`",
                f"- World: `{entry['world_name']}` (`{entry['world_title']}`)",
                f"- Verdict: `{verdict_label(entry['world_verdict'])}`",
                f"- Identity: {entry['identity']}",
                f"- Thesis: {entry['thesis']}",
                f"- Biography summary: `{entry['summary_path']}`",
                f"- Biography report: `{entry['report_markdown_path']}`",
                f"- Biography check: `{entry['check_text_path']}`",
                f"- Runtime evidence summary: `{entry['runtime_evidence_summary']}`",
                f"- Witness bundle summary: `{entry['witness_bundle_summary']}`",
            ]
        )
        if entry["active_facets"]:
            report_lines.append("- Active facets: `{0}`".format("`, `".join(entry["active_facets"])))
        if entry["world_compare_summary"]:
            report_lines.append(f"- World compare summary: `{entry['world_compare_summary']}`")
        if entry["evidence_path"]:
            report_lines.append("- Evidence path: `{0}`".format(" -> ".join(entry["evidence_path"])))
        if entry["next_questions"]:
            report_lines.append("- Next questions:")
            for question in entry["next_questions"]:
                report_lines.append(f"  - {question}")
        else:
            report_lines.append("- Next questions: none")

    report_lines.extend(["", "## Shelf Questions"])
    if (
        not summary["questions"]["core_questions"]
        and not summary["questions"]["compare_questions"]
        and not summary["questions"]["next_questions"]
    ):
        report_lines.append("- none")
    else:
        extend_question_lines(report_lines, "core", summary["questions"]["core_questions"])
        extend_question_lines(report_lines, "compare", summary["questions"]["compare_questions"])
        extend_question_lines(report_lines, "next", summary["questions"]["next_questions"])

    return "\n".join(report_lines) + "\n"


def build_check(summary: dict):
    lines = [
        f"summary: {summary['delivery']['summary_path']}",
        f"result: {summary['result']}",
        f"profile: {summary['profile']}",
        "biography_count: {0}".format(summary["summary"]["biography_count"]),
        "unique_world_count: {0}".format(summary["summary"]["unique_world_count"]),
        "ok_count: {0}".format(summary["summary"]["ok_count"]),
        "fail_count: {0}".format(summary["summary"]["fail_count"]),
        "compare_attached_count: {0}".format(summary["summary"]["compare_attached_count"]),
        "not_attached_count: {0}".format(summary["summary"]["not_attached_count"]),
        "standing_count: {0}".format(summary["summary"]["standing_count"]),
        "improved_count: {0}".format(summary["summary"]["improved_count"]),
        "drifted_count: {0}".format(summary["summary"]["drifted_count"]),
        "collapsed_count: {0}".format(summary["summary"]["collapsed_count"]),
    ]
    return "\n".join(lines) + "\n"


def build_summary(args):
    biography_paths = [Path(path).resolve() for path in args.biography]
    if not biography_paths:
        raise ValueError("at least one --biography path is required")

    biographies = [load_biography(path) for path in biography_paths]
    output_root = Path(args.output_root or biography_paths[0].parent).resolve()
    summary_path = resolve_output_path(args.summary, output_root, "biography.index.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "biography.index.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "biography.index.check.txt")

    seen_ids: dict[str, int] = {}
    entries = [build_entry(biography, path, seen_ids) for biography, path in zip(biographies, biography_paths)]
    supporting_surfaces = [
        build_biography_surface(entry, biography, biography_path)
        for entry, biography, biography_path in zip(entries, biographies, biography_paths)
    ]
    index_summary = build_index_summary(entries)
    questions = build_questions(biographies)

    summary = {
        "schema": "system_compiler.biography_index/v0",
        "kind": "system_compiler.biography_index",
        "generated_at_utc": datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ"),
        "generator": "scripts/export_system_compiler_biography_index.py",
        "result": "ok" if index_summary["fail_count"] == 0 else "fail",
        "profile": str(args.profile),
        "shelf": {
            "title": "System Compiler World Shelf",
            "summary": (
                "A directory view that gathers one or more system compiler biographies into a single "
                "world shelf for browsing, compare, and review."
            ),
        },
        "front_page": build_front_page(
            summary_path=summary_path,
            report_path=report_path,
            check_path=check_path,
            supporting_surfaces=supporting_surfaces,
        ),
        "delivery": {
            "output_root": str(output_root),
            "summary_path": str(summary_path),
            "report_markdown_path": str(report_path),
            "check_text_path": str(check_path),
        },
        "artifact_context": {
            "biography_summaries": [str(path) for path in biography_paths],
            "output_root": str(output_root),
            "report_markdown_path": str(report_path),
            "check_text_path": str(check_path),
        },
        "summary": index_summary,
        "entries": entries,
        "questions": questions,
        "violations": [],
    }

    write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
    write_text(report_path, build_report(summary))
    write_text(check_path, build_check(summary))
    return summary_path, summary


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export a world shelf from one or more system compiler biography summaries."
    )
    parser.add_argument(
        "--biography",
        action="append",
        default=[],
        help="Biography summary path. Pass multiple times to build one shelf.",
    )
    parser.add_argument("--output-root", default="", help="Output root for shelf artifacts.")
    parser.add_argument("--summary", default="", help="Explicit shelf summary path.")
    parser.add_argument("--report-markdown", default="", help="Explicit shelf report path.")
    parser.add_argument("--check-text", default="", help="Explicit shelf check path.")
    parser.add_argument(
        "--profile",
        default="system-compiler-world-shelf",
        help="Profile label recorded inside the shelf summary.",
    )
    args = parser.parse_args()

    try:
        summary_path, summary = build_summary(args)
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[BIOGRAPHY-INDEX] summary={summary_path}")
    print(f"[BIOGRAPHY-INDEX] result={summary['result']}")
    print(f"[BIOGRAPHY-INDEX] biographies={summary['summary']['biography_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
