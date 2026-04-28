import argparse
import json
import sys
from collections import Counter
from pathlib import Path


INDEX_SCHEMA_PATH = "schemas/system_compiler.biography_index.v0.schema.json"
BIOGRAPHY_SCHEMA_PATH = "schemas/system_compiler.biography.v0.schema.json"
BIOGRAPHY_SCHEMA = "system_compiler.biography/v0"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def normalize_path(value: str | None):
    if value is None:
        return None
    text = str(value).strip()
    if not text:
        return None
    return str(Path(text).resolve())


def ensure_exists(path_value: str | None, label: str, errors: list[str]):
    normalized = normalize_path(path_value)
    if normalized is None:
        errors.append(f"{label}: missing path")
        return None
    if not Path(normalized).exists():
        errors.append(f"{label}: not found -> {normalized}")
    return normalized


def string_list(values):
    seen = set()
    result = []
    for value in values or []:
        if value is None:
            continue
        text = str(value).strip()
        if not text or text in seen:
            continue
        seen.add(text)
        result.append(text)
    return result


def nullable_text(value):
    if value is None:
        return None
    text = str(value)
    return text if text else None


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


def expected_summary_route(
    biography: dict,
    artifact_context: dict,
    *,
    surface_id: str,
    artifact_key: str,
    fallback_value=None,
):
    surface = get_front_page_surface(biography, surface_id)
    if surface is not None:
        normalized_surface_path = normalize_path(surface.get("summary_path"))
        if normalized_surface_path is not None:
            return normalized_surface_path

    normalized_artifact_path = normalize_path(artifact_context.get(artifact_key))
    if normalized_artifact_path is not None:
        return normalized_artifact_path

    return normalize_path(fallback_value)


def validate_summary_counts(summary: dict, errors: list[str]):
    entries = summary.get("entries", [])
    expected = {
        "biography_count": len(entries),
        "unique_world_count": len({str(entry.get("world_name", "")) for entry in entries}),
        "ok_count": sum(1 for entry in entries if entry.get("result") == "ok"),
        "fail_count": sum(1 for entry in entries if entry.get("result") != "ok"),
        "compare_attached_count": sum(1 for entry in entries if bool(entry.get("compare_attached"))),
        "not_attached_count": sum(1 for entry in entries if not bool(entry.get("compare_attached"))),
        "standing_count": sum(1 for entry in entries if entry.get("world_verdict") == "standing"),
        "improved_count": sum(1 for entry in entries if entry.get("world_verdict") == "improved"),
        "drifted_count": sum(1 for entry in entries if entry.get("world_verdict") == "drifted"),
        "collapsed_count": sum(1 for entry in entries if entry.get("world_verdict") == "collapsed"),
    }

    summary_block = summary.get("summary", {})
    for key, expected_value in expected.items():
        actual_value = summary_block.get(key)
        if actual_value != expected_value:
            errors.append(f"summary.{key}: expected {expected_value} but got {actual_value}")

    expected_result = "ok" if expected["fail_count"] == 0 else "fail"
    if summary.get("result") != expected_result:
        errors.append(f"result: expected {expected_result} but got {summary.get('result')}")


def build_expected_questions(biographies: list[dict]):
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


def compare_list_field(actual, expected, label: str, errors: list[str]):
    actual_list = string_list(actual)
    expected_list = string_list(expected)
    if actual_list != expected_list:
        errors.append(f"{label}: expected {expected_list} but got {actual_list}")


def compare_scalar_field(actual, expected, label: str, errors: list[str]):
    if actual != expected:
        errors.append(f"{label}: expected {expected!r} but got {actual!r}")


def validate_front_page(front_page: dict, label: str, errors: list[str]):
    ensure_exists(front_page.get("summary_path"), f"{label}.summary_path", errors)
    ensure_exists(front_page.get("report_markdown_path"), f"{label}.report_markdown_path", errors)
    ensure_exists(front_page.get("check_text_path"), f"{label}.check_text_path", errors)

    for index, surface in enumerate(front_page.get("supporting_surfaces", [])):
        if not isinstance(surface, dict):
            errors.append(f"{label}.supporting_surfaces[{index}]: invalid surface")
            continue

        ensure_exists(surface.get("summary_path"), f"{label}.supporting_surfaces[{index}].summary_path", errors)
        ensure_exists(
            surface.get("report_markdown_path"),
            f"{label}.supporting_surfaces[{index}].report_markdown_path",
            errors,
        )
        ensure_exists(
            surface.get("check_text_path"),
            f"{label}.supporting_surfaces[{index}].check_text_path",
            errors,
        )


def expected_biography_surface(entry: dict, biography: dict, biography_path: str):
    front_page = biography.get("front_page", {})
    delivery = biography.get("delivery", {})
    world = biography.get("world", {})
    world_name = str(world.get("name", "")).strip()
    world_title = str(world.get("title", "")).strip()
    label_anchor = world_name or world_title or str(entry.get("id", "")).strip()
    return {
        "id": entry.get("id"),
        "label": f"world biography: {label_anchor}",
        "role": "shelf_entry",
        "summary_schema": BIOGRAPHY_SCHEMA,
        "summary_path": normalize_path(front_page.get("summary_path"))
        or normalize_path(delivery.get("summary_path"))
        or normalize_path(biography_path),
        "report_markdown_path": normalize_path(front_page.get("report_markdown_path"))
        or normalize_path(delivery.get("report_markdown_path")),
        "check_text_path": normalize_path(front_page.get("check_text_path"))
        or normalize_path(delivery.get("check_text_path")),
    }


def validate_entry(entry: dict, biography: dict, errors: list[str]):
    world = biography.get("world", {})
    world_subject = world.get("subject", {})
    biography_block = biography.get("biography", {})
    biography_delivery = biography.get("delivery", {})
    artifact_context = biography.get("artifact_context", {})
    world_compare = biography.get("world_compare")
    compare_attached = world_compare is not None
    expected_runtime_evidence_summary = expected_summary_route(
        biography,
        artifact_context,
        surface_id="runtime_evidence",
        artifact_key="runtime_evidence_summary",
    )
    expected_witness_bundle_summary = expected_summary_route(
        biography,
        artifact_context,
        surface_id="witness_bundle",
        artifact_key="witness_bundle_summary",
    )
    expected_world_compare_summary = expected_summary_route(
        biography,
        artifact_context,
        surface_id="world_compare",
        artifact_key="world_compare_summary",
        fallback_value=(world_compare or {}).get("summary_path"),
    )

    compare_scalar_field(entry.get("profile"), str(biography.get("profile", "")), "entry.profile", errors)
    compare_scalar_field(entry.get("world_name"), str(world.get("name", "")), "entry.world_name", errors)
    compare_scalar_field(entry.get("world_title"), str(world.get("title", "")), "entry.world_title", errors)
    compare_scalar_field(entry.get("board"), nullable_text(world_subject.get("board")), "entry.board", errors)
    compare_list_field(entry.get("active_facets"), world_subject.get("active_facets", []), "entry.active_facets", errors)
    compare_scalar_field(entry.get("result"), str(biography.get("result", "fail")), "entry.result", errors)
    compare_scalar_field(entry.get("world_verdict"), biography.get("world_verdict"), "entry.world_verdict", errors)
    compare_scalar_field(entry.get("compare_attached"), compare_attached, "entry.compare_attached", errors)
    compare_scalar_field(entry.get("identity"), str(biography_block.get("identity", "")), "entry.identity", errors)
    compare_scalar_field(entry.get("thesis"), str(biography_block.get("thesis", "")), "entry.thesis", errors)
    compare_list_field(entry.get("evidence_path"), biography_block.get("evidence_path", []), "entry.evidence_path", errors)
    compare_list_field(entry.get("next_questions"), biography_block.get("next_questions", []), "entry.next_questions", errors)
    compare_scalar_field(
        normalize_path(entry.get("report_markdown_path")),
        normalize_path(biography_delivery.get("report_markdown_path")),
        "entry.report_markdown_path",
        errors,
    )
    compare_scalar_field(
        normalize_path(entry.get("check_text_path")),
        normalize_path(biography_delivery.get("check_text_path")),
        "entry.check_text_path",
        errors,
    )
    compare_scalar_field(
        normalize_path(entry.get("runtime_evidence_summary")),
        expected_runtime_evidence_summary,
        "entry.runtime_evidence_summary",
        errors,
    )
    compare_scalar_field(
        normalize_path(entry.get("witness_bundle_summary")),
        expected_witness_bundle_summary,
        "entry.witness_bundle_summary",
        errors,
    )
    compare_scalar_field(
        normalize_path(entry.get("world_compare_summary")),
        expected_world_compare_summary,
        "entry.world_compare_summary",
        errors,
    )

    if compare_attached and entry.get("world_compare_summary") is None:
        errors.append("entry.world_compare_summary: compare-attached entry is missing world compare summary")
    if not compare_attached and entry.get("world_compare_summary") is not None:
        errors.append("entry.world_compare_summary: witness-only entry unexpectedly points to world compare summary")


def validate_references(summary: dict, biography_schema: dict, errors: list[str]):
    front_page = summary.get("front_page", {})
    if isinstance(front_page, dict):
        validate_front_page(front_page, "front_page", errors)

    delivery = summary.get("delivery", {})
    delivery_summary_path = None
    delivery_report_path = None
    delivery_check_path = None
    if isinstance(delivery, dict):
        ensure_exists(delivery.get("output_root"), "delivery.output_root", errors)
        delivery_summary_path = ensure_exists(delivery.get("summary_path"), "delivery.summary_path", errors)
        delivery_report_path = ensure_exists(delivery.get("report_markdown_path"), "delivery.report_markdown_path", errors)
        delivery_check_path = ensure_exists(delivery.get("check_text_path"), "delivery.check_text_path", errors)

    if isinstance(front_page, dict):
        compare_scalar_field(
            normalize_path(front_page.get("summary_path")),
            delivery_summary_path,
            "front_page.summary_path",
            errors,
        )
        compare_scalar_field(
            normalize_path(front_page.get("report_markdown_path")),
            delivery_report_path,
            "front_page.report_markdown_path",
            errors,
        )
        compare_scalar_field(
            normalize_path(front_page.get("check_text_path")),
            delivery_check_path,
            "front_page.check_text_path",
            errors,
        )

    artifact_context = summary.get("artifact_context", {})
    biography_paths: list[str] = []
    if isinstance(artifact_context, dict):
        for index, value in enumerate(artifact_context.get("biography_summaries", [])):
            normalized = ensure_exists(value, f"artifact_context.biography_summaries[{index}]", errors)
            if normalized is not None:
                biography_paths.append(normalized)
        ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
        ensure_exists(artifact_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
        ensure_exists(artifact_context.get("check_text_path"), "artifact_context.check_text_path", errors)

    if not biography_paths:
        errors.append("artifact_context.biography_summaries: expected at least one biography summary")
        return

    try:
        import jsonschema
    except ImportError:
        errors.append("jsonschema is required")
        return

    biographies_by_path: dict[str, dict] = {}
    for biography_path in biography_paths:
        biography_data = load_json(Path(biography_path))
        try:
            jsonschema.validate(biography_data, biography_schema)
        except Exception as exc:
            errors.append(f"biography schema validation failed for {biography_path}: {exc}")
            continue
        biographies_by_path[biography_path] = biography_data

    entries = summary.get("entries", [])
    if len(entries) != len(biography_paths):
        errors.append(
            "entries: expected {0} entries from artifact_context.biography_summaries but got {1}".format(
                len(biography_paths),
                len(entries),
            )
        )

    remaining = Counter(biography_paths)
    for index, entry in enumerate(entries):
        entry_prefix = f"entries[{index}]"
        summary_path = ensure_exists(entry.get("summary_path"), f"{entry_prefix}.summary_path", errors)
        ensure_exists(entry.get("report_markdown_path"), f"{entry_prefix}.report_markdown_path", errors)
        ensure_exists(entry.get("check_text_path"), f"{entry_prefix}.check_text_path", errors)
        ensure_exists(entry.get("runtime_evidence_summary"), f"{entry_prefix}.runtime_evidence_summary", errors)
        ensure_exists(entry.get("witness_bundle_summary"), f"{entry_prefix}.witness_bundle_summary", errors)
        if entry.get("world_compare_summary") is not None:
            ensure_exists(entry.get("world_compare_summary"), f"{entry_prefix}.world_compare_summary", errors)

        if summary_path is None:
            continue
        if remaining[summary_path] <= 0:
            errors.append(f"{entry_prefix}.summary_path: unexpected biography summary -> {summary_path}")
            continue
        remaining[summary_path] -= 1

        biography = biographies_by_path.get(summary_path)
        if biography is None:
            continue
        entry_errors: list[str] = []
        validate_entry(entry, biography, entry_errors)
        for message in entry_errors:
            errors.append(f"{entry_prefix}: {message}")

    for biography_path, count in remaining.items():
        if count > 0:
            errors.append(f"artifact_context.biography_summaries: unreferenced biography summary -> {biography_path}")

    expected_questions = build_expected_questions(list(biographies_by_path.values()))
    questions = summary.get("questions", {})
    compare_list_field(questions.get("core_questions", []), expected_questions["core_questions"], "questions.core_questions", errors)
    compare_list_field(
        questions.get("compare_questions", []),
        expected_questions["compare_questions"],
        "questions.compare_questions",
        errors,
    )
    compare_list_field(questions.get("next_questions", []), expected_questions["next_questions"], "questions.next_questions", errors)

    if not isinstance(front_page, dict):
        return

    surfaces = front_page.get("supporting_surfaces", [])
    if len(surfaces) != len(entries):
        errors.append(
            "front_page.supporting_surfaces: expected {0} surfaces but got {1}".format(
                len(entries),
                len(surfaces),
            )
        )

    for index, surface in enumerate(surfaces):
        if index >= len(entries) or not isinstance(surface, dict):
            continue

        entry = entries[index]
        biography_path = biography_paths[index] if index < len(biography_paths) else None
        if biography_path is None:
            continue

        biography = biographies_by_path.get(biography_path)
        if biography is None:
            continue

        expected_surface = expected_biography_surface(entry, biography, biography_path)
        prefix = f"front_page.supporting_surfaces[{index}]"
        compare_scalar_field(surface.get("id"), expected_surface["id"], f"{prefix}.id", errors)
        compare_scalar_field(surface.get("label"), expected_surface["label"], f"{prefix}.label", errors)
        compare_scalar_field(surface.get("role"), expected_surface["role"], f"{prefix}.role", errors)
        compare_scalar_field(
            surface.get("summary_schema"),
            expected_surface["summary_schema"],
            f"{prefix}.summary_schema",
            errors,
        )
        compare_scalar_field(
            normalize_path(surface.get("summary_path")),
            expected_surface["summary_path"],
            f"{prefix}.summary_path",
            errors,
        )
        compare_scalar_field(
            normalize_path(surface.get("report_markdown_path")),
            expected_surface["report_markdown_path"],
            f"{prefix}.report_markdown_path",
            errors,
        )
        compare_scalar_field(
            normalize_path(surface.get("check_text_path")),
            expected_surface["check_text_path"],
            f"{prefix}.check_text_path",
            errors,
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler biography index summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to biography index summary JSON. If omitted, --bundle-root/biography.index.summary.json is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing biography.index.summary.json.",
    )
    args = parser.parse_args()

    try:
        import jsonschema
    except ImportError:
        print("jsonschema is required. Install it with: python -m pip install jsonschema", file=sys.stderr)
        return 2

    repo_root = Path(__file__).resolve().parent.parent
    if args.summary:
        summary_path = Path(args.summary).resolve()
    else:
        bundle_root = Path(args.bundle_root or "out/system-compiler-biography-index").resolve()
        summary_path = bundle_root / "biography.index.summary.json"

    index_schema_path = (repo_root / INDEX_SCHEMA_PATH).resolve()
    biography_schema_path = (repo_root / BIOGRAPHY_SCHEMA_PATH).resolve()

    try:
        summary = load_json(summary_path)
        index_schema = load_json(index_schema_path)
        biography_schema = load_json(biography_schema_path)
        jsonschema.validate(summary, index_schema)
        errors: list[str] = []
        validate_summary_counts(summary, errors)
        validate_references(summary, biography_schema, errors)
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] profile -> {summary.get('profile', '')}")
    print(f"[OK] biographies -> {summary.get('summary', {}).get('biography_count', 0)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
