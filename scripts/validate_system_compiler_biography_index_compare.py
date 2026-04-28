import argparse
import json
import sys
from pathlib import Path


COMPARE_SCHEMA_PATH = "schemas/system_compiler.biography_index_compare.v0.schema.json"
INDEX_SCHEMA_PATH = "schemas/system_compiler.biography_index.v0.schema.json"


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


def validate_summary_counts(summary: dict, errors: list[str]):
    entry_changes = summary.get("entry_changes", [])
    change_count = len(entry_changes)
    added_count = sum(1 for entry in entry_changes if entry.get("change_kind") == "added")
    removed_count = sum(1 for entry in entry_changes if entry.get("change_kind") == "removed")
    regression_count = sum(1 for entry in entry_changes if entry.get("impact") == "regression")
    improvement_count = sum(1 for entry in entry_changes if entry.get("impact") == "improvement")
    neutral_change_count = sum(1 for entry in entry_changes if entry.get("impact") == "neutral")

    entry_summary = summary.get("entry_summary", {})
    expected = {
        "changed_entry_count": change_count,
        "added_entry_count": added_count,
        "removed_entry_count": removed_count,
        "regression_count": regression_count,
        "improvement_count": improvement_count,
        "neutral_change_count": neutral_change_count,
    }

    for key, expected_value in expected.items():
        actual_value = entry_summary.get(key)
        if actual_value != expected_value:
            errors.append(f"entry_summary.{key}: expected {expected_value} but got {actual_value}")

    collapse_surface = summary.get("collapse_surface", {})
    if len(collapse_surface.get("regressed_entries", [])) != regression_count:
        errors.append(
            "collapse_surface.regressed_entries: expected {0} entries but got {1}".format(
                regression_count,
                len(collapse_surface.get("regressed_entries", [])),
            )
        )

    expected_added_failed = sum(
        1
        for entry in entry_changes
        if entry.get("change_kind") == "added" and entry.get("right_result") != "ok"
    )
    if len(collapse_surface.get("added_failed_entries", [])) != expected_added_failed:
        errors.append(
            "collapse_surface.added_failed_entries: expected {0} entries but got {1}".format(
                expected_added_failed,
                len(collapse_surface.get("added_failed_entries", [])),
            )
        )


def validate_references(summary: dict, index_schema: dict, errors: list[str]):
    delivery_context = summary.get("artifact_context", {})
    baseline_path = None
    candidate_path = None
    if isinstance(delivery_context, dict):
        baseline_path = ensure_exists(delivery_context.get("baseline_biography_index"), "artifact_context.baseline_biography_index", errors)
        candidate_path = ensure_exists(delivery_context.get("candidate_biography_index"), "artifact_context.candidate_biography_index", errors)
        ensure_exists(delivery_context.get("output_root"), "artifact_context.output_root", errors)
        ensure_exists(delivery_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
        ensure_exists(delivery_context.get("check_text_path"), "artifact_context.check_text_path", errors)

    if baseline_path is None or candidate_path is None:
        return

    try:
        import jsonschema
    except ImportError:
        errors.append("jsonschema is required")
        return

    baseline_summary = load_json(Path(baseline_path))
    candidate_summary = load_json(Path(candidate_path))
    try:
        jsonschema.validate(baseline_summary, index_schema)
    except Exception as exc:
        errors.append(f"baseline biography index schema validation failed: {exc}")
    try:
        jsonschema.validate(candidate_summary, index_schema)
    except Exception as exc:
        errors.append(f"candidate biography index schema validation failed: {exc}")

    shelf_status = summary.get("shelf_status", {})
    baseline_index_summary = baseline_summary.get("summary", {})
    candidate_index_summary = candidate_summary.get("summary", {})

    expected_status = {
        "baseline_result": baseline_summary.get("result"),
        "candidate_result": candidate_summary.get("result"),
        "baseline_profile": baseline_summary.get("profile"),
        "candidate_profile": candidate_summary.get("profile"),
        "baseline_biography_count": baseline_index_summary.get("biography_count"),
        "candidate_biography_count": candidate_index_summary.get("biography_count"),
        "baseline_unique_world_count": baseline_index_summary.get("unique_world_count"),
        "candidate_unique_world_count": candidate_index_summary.get("unique_world_count"),
        "baseline_compare_attached_count": baseline_index_summary.get("compare_attached_count"),
        "candidate_compare_attached_count": candidate_index_summary.get("compare_attached_count"),
        "baseline_not_attached_count": baseline_index_summary.get("not_attached_count"),
        "candidate_not_attached_count": candidate_index_summary.get("not_attached_count"),
    }
    for key, expected_value in expected_status.items():
        actual_value = shelf_status.get(key)
        if actual_value != expected_value:
            errors.append(f"shelf_status.{key}: expected {expected_value!r} but got {actual_value!r}")

    baseline_entries_by_path = {
        normalize_path(entry.get("summary_path")): entry
        for entry in baseline_summary.get("entries", [])
        if normalize_path(entry.get("summary_path")) is not None
    }
    candidate_entries_by_path = {
        normalize_path(entry.get("summary_path")): entry
        for entry in candidate_summary.get("entries", [])
        if normalize_path(entry.get("summary_path")) is not None
    }

    entry_summary = summary.get("entry_summary", {})
    if entry_summary.get("baseline_entry_count") != len(baseline_entries_by_path):
        errors.append(
            "entry_summary.baseline_entry_count: expected {0} but got {1}".format(
                len(baseline_entries_by_path),
                entry_summary.get("baseline_entry_count"),
            )
        )
    if entry_summary.get("candidate_entry_count") != len(candidate_entries_by_path):
        errors.append(
            "entry_summary.candidate_entry_count: expected {0} but got {1}".format(
                len(candidate_entries_by_path),
                entry_summary.get("candidate_entry_count"),
            )
        )

    for index, change in enumerate(summary.get("entry_changes", [])):
        prefix = f"entry_changes[{index}]"
        left_summary_path = ensure_exists(change.get("left_summary_path"), f"{prefix}.left_summary_path", errors) if change.get("left_summary_path") is not None else None
        right_summary_path = ensure_exists(change.get("right_summary_path"), f"{prefix}.right_summary_path", errors) if change.get("right_summary_path") is not None else None

        if left_summary_path is not None:
            baseline_entry = baseline_entries_by_path.get(left_summary_path)
            if baseline_entry is None:
                errors.append(f"{prefix}.left_summary_path: not found in baseline biography index -> {left_summary_path}")
            else:
                if change.get("baseline_entry_id") != baseline_entry.get("id"):
                    errors.append(f"{prefix}.baseline_entry_id: expected {baseline_entry.get('id')!r} but got {change.get('baseline_entry_id')!r}")
                if change.get("world_name") != baseline_entry.get("world_name"):
                    errors.append(f"{prefix}.world_name: expected {baseline_entry.get('world_name')!r} but got {change.get('world_name')!r}")
                if change.get("profile") != baseline_entry.get("profile"):
                    errors.append(f"{prefix}.profile: expected {baseline_entry.get('profile')!r} but got {change.get('profile')!r}")

        if right_summary_path is not None:
            candidate_entry = candidate_entries_by_path.get(right_summary_path)
            if candidate_entry is None:
                errors.append(f"{prefix}.right_summary_path: not found in candidate biography index -> {right_summary_path}")
            else:
                if change.get("candidate_entry_id") != candidate_entry.get("id"):
                    errors.append(f"{prefix}.candidate_entry_id: expected {candidate_entry.get('id')!r} but got {change.get('candidate_entry_id')!r}")
                if change.get("world_name") != candidate_entry.get("world_name"):
                    errors.append(f"{prefix}.world_name: expected {candidate_entry.get('world_name')!r} but got {change.get('world_name')!r}")
                if change.get("profile") != candidate_entry.get("profile"):
                    errors.append(f"{prefix}.profile: expected {candidate_entry.get('profile')!r} but got {change.get('profile')!r}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler biography index compare summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to biography index compare summary JSON. If omitted, --bundle-root/summary.json is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-biography-index-compare").resolve()
        summary_path = bundle_root / "summary.json"

    compare_schema_path = (repo_root / COMPARE_SCHEMA_PATH).resolve()
    index_schema_path = (repo_root / INDEX_SCHEMA_PATH).resolve()

    try:
        summary = load_json(summary_path)
        compare_schema = load_json(compare_schema_path)
        index_schema = load_json(index_schema_path)
        jsonschema.validate(summary, compare_schema)
        errors: list[str] = []
        validate_summary_counts(summary, errors)
        validate_references(summary, index_schema, errors)
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] shelf verdict -> {summary.get('shelf_verdict', '')}")
    print(f"[OK] changed entries -> {summary.get('entry_summary', {}).get('changed_entry_count', 0)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
