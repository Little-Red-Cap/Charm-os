import argparse
import json
from datetime import datetime
from pathlib import Path


RUNTIME_EVIDENCE_SCHEMA = "minimal_kernel.runtime_evidence_bundle.summary/v1"
WITNESS_BUNDLE_SCHEMA = "system_compiler.witness_bundle/v0"
WORLD_COMPARE_SCHEMA = "system_compiler.world_compare/v0"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_text(path: Path, text: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def resolve_output_path(explicit: str, output_root: Path, default_name: str) -> Path:
    if explicit:
        return Path(explicit).resolve()
    return (output_root / default_name).resolve()


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


def require_schema(path: Path, data: dict, expected_schema: str, expected_kind: str | None = None):
    if data.get("schema") != expected_schema:
        raise ValueError(f"unsupported schema for {path}: {data.get('schema')}")
    if expected_kind is not None and data.get("kind") != expected_kind:
        raise ValueError(f"unsupported kind for {path}: {data.get('kind')}")


def build_world_view(witness_bundle: dict):
    world = witness_bundle["world"]
    subject = world.get("subject", {})
    return {
        "name": str(world["name"]),
        "title": str(world["title"]),
        "summary": str(world["summary"]),
        "subject": {
            "profile": nullable_text(subject.get("profile")),
            "board": nullable_text(subject.get("board")),
            "active_facets": string_list(subject.get("active_facets", [])),
        },
        "first_class_terms": string_list(world.get("first_class_terms", [])),
        "core_questions": string_list(world.get("core_questions", [])),
        "compare_questions": string_list(world.get("compare_questions", [])),
        "contract_refs": string_list(world.get("contract_refs", [])),
    }


def build_runtime_view(runtime_summary: dict, runtime_path: Path):
    host = runtime_summary.get("host", {})
    cold = host.get("cold", {})
    warm = host.get("warm", {})
    warm_compare = warm.get("comparison", {})

    qemu_lower_half = runtime_summary.get("qemu", {}).get("lower_half")
    if qemu_lower_half:
        qemu_view = {
            "available": True,
            "case_count": int(qemu_lower_half.get("case_count", 0)),
            "completed_case_count": int(qemu_lower_half.get("completed_case_count", 0)),
            "ok_count": int(qemu_lower_half.get("status", {}).get("ok", 0)),
            "fail_count": int(qemu_lower_half.get("status", {}).get("fail", 0)),
            "other_count": int(qemu_lower_half.get("status", {}).get("other", 0)),
            "biography_identity": nullable_text(qemu_lower_half.get("biography", {}).get("identity")),
            "witness_conclusion": nullable_text(qemu_lower_half.get("witness_bundle", {}).get("conclusion")),
        }
    else:
        qemu_view = {
            "available": False,
            "case_count": 0,
            "completed_case_count": 0,
            "ok_count": 0,
            "fail_count": 0,
            "other_count": 0,
            "biography_identity": None,
            "witness_conclusion": None,
        }

    return {
        "result": str(runtime_summary.get("result", "fail")),
        "summary_path": str(runtime_path),
        "report_markdown_path": str(Path(runtime_summary["report_markdown_path"]).resolve()),
        "check_text_path": str(Path(runtime_summary["check_text_path"]).resolve()),
        "host": {
            "cold_ok_count": int(cold.get("status", {}).get("ok", 0)),
            "cold_fail_count": int(cold.get("status", {}).get("fail", 0)),
            "cold_other_count": int(cold.get("status", {}).get("other", 0)),
            "warm_ok_count": int(warm.get("status", {}).get("ok", 0)),
            "warm_fail_count": int(warm.get("status", {}).get("fail", 0)),
            "warm_other_count": int(warm.get("status", {}).get("other", 0)),
            "warm_regressions": int(warm_compare.get("regressions", 0)),
            "warm_improvements": int(warm_compare.get("improvements", 0)),
        },
        "qemu": qemu_view,
    }


def build_witness_view(witness_bundle: dict, witness_path: Path):
    summary = witness_bundle.get("witness_summary", {})
    artifact_context = witness_bundle.get("artifact_context", {})
    return {
        "result": str(witness_bundle.get("result", "fail")),
        "summary_path": str(witness_path),
        "report_markdown_path": str(Path(artifact_context["report_markdown_path"]).resolve()),
        "check_text_path": str(Path(artifact_context["check_text_path"]).resolve()),
        "entry_count": int(summary.get("entry_count", 0)),
        "ok_count": int(summary.get("ok_count", 0)),
        "missing_count": int(summary.get("missing_count", 0)),
        "fail_count": int(summary.get("fail_count", 0)),
        "required_missing_count": int(summary.get("required_missing_count", 0)),
    }


def build_compare_view(world_compare: dict, compare_path: Path):
    artifact_context = world_compare.get("artifact_context", {})
    collapse_surface = world_compare.get("collapse_surface", {})
    witness_summary = world_compare.get("witness_summary", {})
    return {
        "result": str(world_compare.get("result", "fail")),
        "summary_path": str(compare_path),
        "report_markdown_path": str(Path(artifact_context["report_markdown_path"]).resolve()),
        "check_text_path": str(Path(artifact_context["check_text_path"]).resolve()),
        "world_verdict": str(world_compare["world_verdict"]),
        "regression_count": int(witness_summary.get("regression_count", 0)),
        "improvement_count": int(witness_summary.get("improvement_count", 0)),
        "required_regression_count": int(witness_summary.get("required_regression_count", 0)),
        "collapse_surface_changed": bool(collapse_surface.get("changed", False)),
        "affected_layers": string_list(collapse_surface.get("affected_layers", [])),
        "affected_focus": string_list(collapse_surface.get("affected_focus", [])),
    }


def build_next_questions(runtime_summary: dict, world_view: dict, world_compare: dict | None):
    questions = []
    if world_compare is not None:
        questions.extend(world_compare.get("questions", {}).get("next_questions", []))

    qemu_bio = runtime_summary.get("qemu", {}).get("lower_half", {}).get("biography", {})
    questions.extend(qemu_bio.get("next_questions", []))

    if not questions:
        questions.extend(world_view.get("compare_questions", [])[:1])

    return string_list(questions)


def build_identity(world_view: dict):
    board = world_view["subject"]["board"] or "unknown-board"
    return (
        "The `{0}` world is Charm's witness-bearing runtime biography that joins host runtime glue, "
        "lower-half ingress evidence, and counterfactual system-compiler verdicts on `{1}`."
    ).format(world_view["name"], board)


def build_thesis(runtime_summary: dict, witness_bundle: dict, world_compare: dict | None):
    runtime_ok = str(runtime_summary.get("result", "fail")) == "ok"
    witness_ok = str(witness_bundle.get("result", "fail")) == "ok"

    if world_compare is None:
        if runtime_ok and witness_ok:
            return (
                "This world is currently backed by a valid runtime evidence bundle and witness bundle; "
                "counterfactual compare has not yet been attached."
            )
        return (
            "This world is already explainable as an artifact, but runtime evidence or witness testimony "
            "still carries failures before a baseline compare is even applied."
        )

    verdict = str(world_compare["world_verdict"])
    if verdict == "standing":
        return (
            "This world stands because runtime evidence is green, the witness bundle has no required gaps, "
            "and world compare reports no witness regressions or added missing contracts."
        )
    if verdict == "improved":
        return (
            "This world improves on its baseline without opening required regressions, so the current evidence "
            "story is stronger than before."
        )
    if verdict == "drifted":
        return (
            "This world still materializes, but witness or contract drift means the baseline story no longer "
            "matches the candidate cleanly."
        )
    return (
        "This world has collapsed as a baseline-preserving object because witness regressions, missing contracts, "
        "or candidate failure prevent the prior story from holding."
    )


def build_report(summary: dict):
    report_lines = [
        "# System Compiler Biography",
        "",
        f"- Result: `{summary['result']}`",
        f"- Profile: `{summary['profile']}`",
        f"- World verdict: `{summary['world_verdict']}`" if summary["world_verdict"] else "- World verdict: `not-attached`",
        f"- World: `{summary['world']['name']}` (`{summary['world']['title']}`)",
        f"- Summary JSON: `{summary['delivery']['summary_path']}`",
        "",
        "## Identity",
        f"- Identity: {summary['biography']['identity']}",
        f"- Thesis: {summary['biography']['thesis']}",
        "",
        "## Delivery",
        f"- Output root: `{summary['delivery']['output_root']}`",
        f"- Report: `{summary['delivery']['report_markdown_path']}`",
        f"- Check: `{summary['delivery']['check_text_path']}`",
        "",
        "## Evidence Chain",
        f"- Runtime evidence: `{summary['runtime_evidence']['summary_path']}` (`{summary['runtime_evidence']['result']}`)",
        f"- Witness bundle: `{summary['witness_bundle']['summary_path']}` (`{summary['witness_bundle']['result']}`)",
    ]
    if summary["world_compare"] is not None:
        report_lines.append(
            f"- World compare: `{summary['world_compare']['summary_path']}` (`{summary['world_compare']['world_verdict']}`)"
        )
    else:
        report_lines.append("- World compare: `not-attached`")

    report_lines.extend(
        [
            "",
            "## Runtime Surface",
            "- Host cold: `ok={0} fail={1} other={2}`".format(
                summary["runtime_evidence"]["host"]["cold_ok_count"],
                summary["runtime_evidence"]["host"]["cold_fail_count"],
                summary["runtime_evidence"]["host"]["cold_other_count"],
            ),
            "- Host warm: `ok={0} fail={1} other={2} regressions={3} improvements={4}`".format(
                summary["runtime_evidence"]["host"]["warm_ok_count"],
                summary["runtime_evidence"]["host"]["warm_fail_count"],
                summary["runtime_evidence"]["host"]["warm_other_count"],
                summary["runtime_evidence"]["host"]["warm_regressions"],
                summary["runtime_evidence"]["host"]["warm_improvements"],
            ),
            "- QEMU lower-half: `available={0} cases={1}/{2} ok={3} fail={4} other={5}`".format(
                summary["runtime_evidence"]["qemu"]["available"],
                summary["runtime_evidence"]["qemu"]["completed_case_count"],
                summary["runtime_evidence"]["qemu"]["case_count"],
                summary["runtime_evidence"]["qemu"]["ok_count"],
                summary["runtime_evidence"]["qemu"]["fail_count"],
                summary["runtime_evidence"]["qemu"]["other_count"],
            ),
        ]
    )
    if summary["runtime_evidence"]["qemu"]["witness_conclusion"]:
        report_lines.append(
            f"- QEMU witness conclusion: `{summary['runtime_evidence']['qemu']['witness_conclusion']}`"
        )
    if summary["runtime_evidence"]["qemu"]["biography_identity"]:
        report_lines.append(
            f"- QEMU biography identity: {summary['runtime_evidence']['qemu']['biography_identity']}"
        )

    report_lines.extend(
        [
            "",
            "## Witness Surface",
            "- Witness entries: `total={0} ok={1} missing={2} fail={3} required_missing={4}`".format(
                summary["witness_bundle"]["entry_count"],
                summary["witness_bundle"]["ok_count"],
                summary["witness_bundle"]["missing_count"],
                summary["witness_bundle"]["fail_count"],
                summary["witness_bundle"]["required_missing_count"],
            ),
            f"- Witness report: `{summary['witness_bundle']['report_markdown_path']}`",
        ]
    )

    if summary["world_compare"] is not None:
        report_lines.extend(
            [
                "",
                "## Counterfactual Verdict",
                "- Verdict: `{0}`".format(summary["world_compare"]["world_verdict"]),
                "- Witness drift: `regressions={0} improvements={1} required_regressions={2}`".format(
                    summary["world_compare"]["regression_count"],
                    summary["world_compare"]["improvement_count"],
                    summary["world_compare"]["required_regression_count"],
                ),
                "- Collapse surface: `changed={0}`".format(summary["world_compare"]["collapse_surface_changed"]),
                f"- Compare report: `{summary['world_compare']['report_markdown_path']}`",
            ]
        )
        if summary["world_compare"]["affected_layers"]:
            report_lines.append(
                "- Affected layers: `{0}`".format("`, `".join(summary["world_compare"]["affected_layers"]))
            )
        if summary["world_compare"]["affected_focus"]:
            report_lines.append(
                "- Affected focus: `{0}`".format("`, `".join(summary["world_compare"]["affected_focus"]))
            )

    report_lines.extend(["", "## Evidence Path"])
    for step in summary["biography"]["evidence_path"]:
        report_lines.append(f"- {step}")

    report_lines.extend(["", "## Questions"])
    for question in summary["questions"]["core_questions"]:
        report_lines.append(f"- core: {question}")
    for question in summary["questions"]["compare_questions"]:
        report_lines.append(f"- compare: {question}")
    for question in summary["biography"]["next_questions"]:
        report_lines.append(f"- next: {question}")

    return "\n".join(report_lines) + "\n"


def build_check(summary: dict):
    lines = [
        f"summary: {summary['delivery']['summary_path']}",
        f"result: {summary['result']}",
        f"profile: {summary['profile']}",
        "world_verdict: {0}".format(summary["world_verdict"] if summary["world_verdict"] else "not-attached"),
        "runtime_evidence: result={0}".format(summary["runtime_evidence"]["result"]),
        "witness_bundle: result={0} entries={1} required_missing={2}".format(
            summary["witness_bundle"]["result"],
            summary["witness_bundle"]["entry_count"],
            summary["witness_bundle"]["required_missing_count"],
        ),
    ]
    if summary["world_compare"] is not None:
        lines.append(
            "world_compare: result={0} verdict={1} regressions={2} required_regressions={3}".format(
                summary["world_compare"]["result"],
                summary["world_compare"]["world_verdict"],
                summary["world_compare"]["regression_count"],
                summary["world_compare"]["required_regression_count"],
            )
        )
    else:
        lines.append("world_compare: not-attached")
    lines.append("next_questions: {0}".format(len(summary["biography"]["next_questions"])))
    return "\n".join(lines) + "\n"


def build_summary(args):
    runtime_path = Path(args.runtime_evidence).resolve()
    witness_path = Path(args.witness_bundle).resolve()
    compare_path = Path(args.world_compare).resolve() if args.world_compare else None

    runtime_summary = load_json(runtime_path)
    witness_bundle = load_json(witness_path)
    require_schema(runtime_path, runtime_summary, RUNTIME_EVIDENCE_SCHEMA)
    require_schema(witness_path, witness_bundle, WITNESS_BUNDLE_SCHEMA, "system_compiler.witness_bundle")

    world_compare = None
    if compare_path is not None:
        world_compare = load_json(compare_path)
        require_schema(compare_path, world_compare, WORLD_COMPARE_SCHEMA, "system_compiler.world_compare")

    output_root = Path(args.output_root or witness_path.parent).resolve()
    summary_path = resolve_output_path(args.summary, output_root, "biography.summary.json")
    report_path = resolve_output_path(args.report_markdown, output_root, "biography.report.md")
    check_path = resolve_output_path(args.check_text, output_root, "biography.check.txt")

    world_view = build_world_view(witness_bundle)
    next_questions = build_next_questions(runtime_summary, world_view, world_compare)
    evidence_path = ["runtime_evidence_bundle", "system_compiler_witness_bundle"]
    if world_compare is not None:
        evidence_path.append("system_compiler_world_compare")

    witness_artifact_context = witness_bundle.get("artifact_context", {})
    compare_artifact_context = world_compare.get("artifact_context", {}) if world_compare else {}

    summary = {
        "schema": "system_compiler.biography/v0",
        "kind": "system_compiler.biography",
        "generated_at_utc": datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ"),
        "generator": "scripts/export_system_compiler_biography.py",
        "result": "ok",
        "profile": str(args.profile),
        "world_verdict": str(world_compare["world_verdict"]) if world_compare is not None else None,
        "world": world_view,
        "biography": {
            "identity": build_identity(world_view),
            "thesis": build_thesis(runtime_summary, witness_bundle, world_compare),
            "evidence_path": evidence_path,
            "next_questions": next_questions,
        },
        "delivery": {
            "output_root": str(output_root),
            "summary_path": str(summary_path),
            "report_markdown_path": str(report_path),
            "check_text_path": str(check_path),
        },
        "artifact_context": {
            "canonical_world_path": nullable_text(witness_artifact_context.get("canonical_world")),
            "runtime_evidence_summary": str(runtime_path),
            "runtime_evidence_report_markdown_path": str(Path(runtime_summary["report_markdown_path"]).resolve()),
            "runtime_evidence_check_text_path": str(Path(runtime_summary["check_text_path"]).resolve()),
            "witness_bundle_summary": str(witness_path),
            "witness_bundle_report_markdown_path": str(Path(witness_artifact_context["report_markdown_path"]).resolve()),
            "witness_bundle_check_text_path": str(Path(witness_artifact_context["check_text_path"]).resolve()),
            "baseline_witness_summary": nullable_text(compare_artifact_context.get("baseline_witness_bundle")),
            "world_compare_summary": str(compare_path) if compare_path is not None else None,
            "world_compare_report_markdown_path": (
                str(Path(compare_artifact_context["report_markdown_path"]).resolve())
                if world_compare is not None
                else None
            ),
            "world_compare_check_text_path": (
                str(Path(compare_artifact_context["check_text_path"]).resolve())
                if world_compare is not None
                else None
            ),
        },
        "runtime_evidence": build_runtime_view(runtime_summary, runtime_path),
        "witness_bundle": build_witness_view(witness_bundle, witness_path),
        "world_compare": build_compare_view(world_compare, compare_path) if world_compare is not None else None,
        "questions": {
            "core_questions": world_view["core_questions"],
            "compare_questions": world_view["compare_questions"],
            "next_questions": next_questions,
        },
        "violations": [],
    }

    write_text(summary_path, json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
    write_text(report_path, build_report(summary))
    write_text(check_path, build_check(summary))
    return summary_path, summary


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Export one top-level system compiler biography from runtime evidence, witness bundle, and optional world compare."
    )
    parser.add_argument("--runtime-evidence", required=True, help="Runtime evidence bundle summary path.")
    parser.add_argument("--witness-bundle", required=True, help="Witness bundle summary path.")
    parser.add_argument("--world-compare", default="", help="Optional world compare summary path.")
    parser.add_argument("--output-root", default="", help="Output root for biography artifacts.")
    parser.add_argument("--summary", default="", help="Explicit biography summary path.")
    parser.add_argument("--report-markdown", default="", help="Explicit biography report path.")
    parser.add_argument("--check-text", default="", help="Explicit biography check path.")
    parser.add_argument(
        "--profile",
        default="minimal-kernel-runtime-system-compiler-witness",
        help="Profile label recorded inside the biography summary.",
    )
    args = parser.parse_args()

    try:
        summary_path, summary = build_summary(args)
    except Exception as exc:
        print(f"[ERROR] {exc}")
        return 1

    print(f"[BIOGRAPHY] summary={summary_path}")
    print(f"[BIOGRAPHY] result={summary['result']}")
    print(f"[BIOGRAPHY] world_verdict={summary['world_verdict']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
