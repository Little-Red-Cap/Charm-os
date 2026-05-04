import argparse
import json
import sys
from pathlib import Path


REVIEW_SCHEMA_PATH = "schemas/system_compiler.world_shelf_review.v0.schema.json"
INDEX_SCHEMA_PATH = "schemas/system_compiler.biography_index.v0.schema.json"
COMPARE_SCHEMA_PATH = "schemas/system_compiler.biography_index_compare.v0.schema.json"


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


def validate_json_schema(document: dict, schema: dict, label: str, errors: list[str]):
    try:
        import jsonschema
    except ImportError:
        errors.append("jsonschema is required")
        return

    try:
        jsonschema.validate(document, schema)
    except Exception as exc:
        errors.append(f"{label} schema validation failed: {exc}")


def expect_equal(actual, expected, label: str, errors: list[str]):
    if actual != expected:
        errors.append(f"{label}: expected {expected!r} but got {actual!r}")


def string_values(values) -> list[str]:
    result: list[str] = []
    for value in values or []:
        if value is None:
            continue
        text = str(value)
        if not text:
            continue
        result.append(text)
    return result


def validate_drift_digest(summary: dict, compare_summary: dict | None, errors: list[str]):
    digest = summary.get("drift_digest", {})
    if compare_summary is None:
        expected = {
            "changed": False,
            "verdict": "candidate-only",
            "entry_changed_count": 0,
            "entry_regression_count": 0,
            "entry_improvement_count": 0,
            "front_page_entry_detail_changed_count": 0,
            "front_page_entry_detail_changed_anchors": [],
            "removed_worlds": [],
            "added_failed_entries": [],
            "affected_worlds": [],
            "affected_profiles": [],
            "narratives": [],
        }
    else:
        entry_summary = compare_summary.get("entry_summary", {})
        shelf_changes = compare_summary.get("shelf_changes", {})
        collapse_surface = compare_summary.get("collapse_surface", {})
        expected = {
            "changed": bool(collapse_surface.get("changed")),
            "verdict": compare_summary.get("shelf_verdict"),
            "entry_changed_count": entry_summary.get("changed_entry_count", 0),
            "entry_regression_count": entry_summary.get("regression_count", 0),
            "entry_improvement_count": entry_summary.get("improvement_count", 0),
            "front_page_entry_detail_changed_count": len(
                shelf_changes.get("front_page_entry_detail_changes", [])
            ),
            "front_page_entry_detail_changed_anchors": string_values(
                collapse_surface.get("front_page_entry_detail_changed_anchors", [])
            ),
            "removed_worlds": string_values(collapse_surface.get("removed_worlds", [])),
            "added_failed_entries": string_values(collapse_surface.get("added_failed_entries", [])),
            "affected_worlds": string_values(collapse_surface.get("affected_worlds", [])),
            "affected_profiles": string_values(collapse_surface.get("affected_profiles", [])),
            "narratives": string_values(collapse_surface.get("narratives", [])),
        }

    for key, expected_value in expected.items():
        expect_equal(digest.get(key), expected_value, f"drift_digest.{key}", errors)


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
        "summary_path": normalize_path(summary_path),
        "report_markdown_path": normalize_path(report_markdown_path),
        "check_text_path": normalize_path(check_text_path),
    }


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


def build_front_page_surface(
    summary: dict,
    fallback_summary_path: str,
    fallback_report_markdown_path: str,
    fallback_check_text_path: str,
    surface_id: str,
    role: str,
    label_prefix: str,
    summary_schema: str,
):
    front_page = summary.get("front_page", {}) or {}
    delivery = summary.get("delivery", {}) or {}
    shelf = summary.get("shelf", {}) or {}
    shelf_title = str(shelf.get("title", "")).strip() or "System Compiler World Shelf"

    return build_surface_ref(
        surface_id=surface_id,
        summary_schema=summary_schema,
        label=f"{label_prefix}: {shelf_title}",
        role=role,
        summary_path=front_page.get("summary_path") or delivery.get("summary_path") or fallback_summary_path,
        report_markdown_path=front_page.get("report_markdown_path")
        or delivery.get("report_markdown_path")
        or fallback_report_markdown_path,
        check_text_path=front_page.get("check_text_path") or delivery.get("check_text_path") or fallback_check_text_path,
    )


def build_route_provenance_entry(
    summary: dict,
    fallback_summary_path: str,
    fallback_report_markdown_path: str,
    fallback_check_text_path: str,
    route_id: str,
    summary_schema: str,
):
    front_page = summary.get("front_page", {}) or {}
    delivery = summary.get("delivery", {}) or {}
    available_supporting_surface_ids = []
    for surface in front_page.get("supporting_surfaces", []):
        if not isinstance(surface, dict):
            continue
        surface_id = str(surface.get("id", "")).strip()
        if not surface_id or surface_id in available_supporting_surface_ids:
            continue
        available_supporting_surface_ids.append(surface_id)

    return {
        "id": route_id,
        "route_kind": "front_page_root",
        "source_summary_schema": summary_schema,
        "source_summary_path": normalize_path(fallback_summary_path),
        "source_front_page_summary_path": normalize_path(front_page.get("summary_path"))
        or normalize_path(delivery.get("summary_path"))
        or normalize_path(fallback_summary_path),
        "source_front_page_report_markdown_path": normalize_path(front_page.get("report_markdown_path"))
        or normalize_path(delivery.get("report_markdown_path"))
        or normalize_path(fallback_report_markdown_path),
        "source_front_page_check_text_path": normalize_path(front_page.get("check_text_path"))
        or normalize_path(delivery.get("check_text_path"))
        or normalize_path(fallback_check_text_path),
        "available_supporting_surface_ids": available_supporting_surface_ids,
    }


def validate_references(summary: dict, index_schema: dict, compare_schema: dict, errors: list[str]):
    front_page = summary.get("front_page", {})
    artifact_context = summary.get("artifact_context", {})
    review_status = summary.get("review_status", {})
    review_verdict = summary.get("review_verdict")
    compare_enabled = bool(review_status.get("compare_enabled"))

    if isinstance(front_page, dict):
        validate_front_page(front_page, "front_page", errors)

    ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
    ensure_exists(artifact_context.get("review_summary_path"), "artifact_context.review_summary_path", errors)
    ensure_exists(artifact_context.get("review_report_markdown_path"), "artifact_context.review_report_markdown_path", errors)
    ensure_exists(artifact_context.get("review_check_text_path"), "artifact_context.review_check_text_path", errors)
    ensure_exists(artifact_context.get("candidate_shelf_root"), "artifact_context.candidate_shelf_root", errors)
    candidate_summary_path = ensure_exists(artifact_context.get("candidate_shelf_summary"), "artifact_context.candidate_shelf_summary", errors)

    baseline_summary_path = normalize_path(artifact_context.get("baseline_shelf_summary"))
    baseline_shelf_root = normalize_path(artifact_context.get("baseline_shelf_root"))
    compare_summary_path = normalize_path(artifact_context.get("compare_summary_path"))
    compare_output_root = normalize_path(artifact_context.get("compare_output_root"))

    if compare_enabled:
        if baseline_summary_path is None:
            errors.append("artifact_context.baseline_shelf_summary: required when compare_enabled=true")
        else:
            ensure_exists(baseline_summary_path, "artifact_context.baseline_shelf_summary", errors)
        if baseline_shelf_root is None:
            errors.append("artifact_context.baseline_shelf_root: required when compare_enabled=true")
        else:
            ensure_exists(baseline_shelf_root, "artifact_context.baseline_shelf_root", errors)
        if compare_summary_path is None:
            errors.append("artifact_context.compare_summary_path: required when compare_enabled=true")
        else:
            ensure_exists(compare_summary_path, "artifact_context.compare_summary_path", errors)
        if compare_output_root is None:
            errors.append("artifact_context.compare_output_root: required when compare_enabled=true")
        else:
            ensure_exists(compare_output_root, "artifact_context.compare_output_root", errors)
    else:
        if baseline_summary_path is not None:
            errors.append("artifact_context.baseline_shelf_summary: expected null when compare_enabled=false")
        if baseline_shelf_root is not None:
            errors.append("artifact_context.baseline_shelf_root: expected null when compare_enabled=false")
        if compare_summary_path is not None:
            errors.append("artifact_context.compare_summary_path: expected null when compare_enabled=false")
        if compare_output_root is not None:
            errors.append("artifact_context.compare_output_root: expected null when compare_enabled=false")

    if candidate_summary_path is None:
        return

    candidate_summary = load_json(Path(candidate_summary_path))
    validate_json_schema(candidate_summary, index_schema, "candidate shelf summary", errors)
    expect_equal(
        normalize_path(front_page.get("summary_path")),
        normalize_path(artifact_context.get("review_summary_path")),
        "front_page.summary_path",
        errors,
    )
    expect_equal(
        normalize_path(front_page.get("report_markdown_path")),
        normalize_path(artifact_context.get("review_report_markdown_path")),
        "front_page.report_markdown_path",
        errors,
    )
    expect_equal(
        normalize_path(front_page.get("check_text_path")),
        normalize_path(artifact_context.get("review_check_text_path")),
        "front_page.check_text_path",
        errors,
    )

    expect_equal(review_status.get("candidate_result"), candidate_summary.get("result"), "review_status.candidate_result", errors)
    expect_equal(review_status.get("candidate_profile"), candidate_summary.get("profile"), "review_status.candidate_profile", errors)
    expect_equal(
        review_status.get("candidate_biography_count"),
        candidate_summary.get("summary", {}).get("biography_count"),
        "review_status.candidate_biography_count",
        errors,
    )
    expect_equal(
        review_status.get("candidate_unique_world_count"),
        candidate_summary.get("summary", {}).get("unique_world_count"),
        "review_status.candidate_unique_world_count",
        errors,
    )
    expect_equal(
        review_status.get("candidate_compare_attached_count"),
        candidate_summary.get("summary", {}).get("compare_attached_count"),
        "review_status.candidate_compare_attached_count",
        errors,
    )
    expect_equal(
        review_status.get("candidate_not_attached_count"),
        candidate_summary.get("summary", {}).get("not_attached_count"),
        "review_status.candidate_not_attached_count",
        errors,
    )

    candidate_questions = candidate_summary.get("questions", {}).get("next_questions", [])
    expect_equal(summary.get("questions", {}).get("candidate_questions"), candidate_questions, "questions.candidate_questions", errors)
    expected_supporting_surfaces = [
        build_front_page_surface(
            summary=candidate_summary,
            fallback_summary_path=candidate_summary_path,
            fallback_report_markdown_path=str(Path(artifact_context.get("candidate_shelf_root", "")) / "biography.index.report.md"),
            fallback_check_text_path=str(Path(artifact_context.get("candidate_shelf_root", "")) / "biography.index.check.txt"),
            surface_id="candidate_shelf",
            role="candidate_shelf",
            label_prefix="candidate shelf",
            summary_schema="system_compiler.biography_index/v0",
        )
    ]

    if not compare_enabled:
        expect_equal(review_status.get("compare_mode"), "candidate-only", "review_status.compare_mode", errors)
        expect_equal(review_verdict, "candidate-only", "review_verdict", errors)
        expect_equal(review_status.get("baseline_result"), None, "review_status.baseline_result", errors)
        expect_equal(review_status.get("compare_result"), None, "review_status.compare_result", errors)
        expect_equal(summary.get("questions", {}).get("compare_questions"), [], "questions.compare_questions", errors)
        expect_equal(summary.get("questions", {}).get("next_questions"), candidate_questions, "questions.next_questions", errors)
        expect_equal(front_page.get("supporting_surfaces"), expected_supporting_surfaces, "front_page.supporting_surfaces", errors)
        expected_route_provenance = [
            build_route_provenance_entry(
                summary=candidate_summary,
                fallback_summary_path=candidate_summary_path,
                fallback_report_markdown_path=str(Path(artifact_context.get("candidate_shelf_root", "")) / "biography.index.report.md"),
                fallback_check_text_path=str(Path(artifact_context.get("candidate_shelf_root", "")) / "biography.index.check.txt"),
                route_id="candidate_shelf",
                summary_schema="system_compiler.biography_index/v0",
            )
        ]
        expect_equal(summary.get("route_provenance"), expected_route_provenance, "route_provenance", errors)
        validate_drift_digest(summary, None, errors)
        collapse_surface = summary.get("collapse_surface", {})
        expect_equal(collapse_surface.get("changed"), False, "collapse_surface.changed", errors)
        for key in (
            "regressed_entries",
            "removed_worlds",
            "added_failed_entries",
            "front_page_entry_detail_changed_anchors",
            "affected_worlds",
            "affected_profiles",
            "narratives",
        ):
            expect_equal(collapse_surface.get(key), [], f"collapse_surface.{key}", errors)
        return

    baseline_summary = load_json(Path(baseline_summary_path))
    compare_summary = load_json(Path(compare_summary_path))
    validate_json_schema(baseline_summary, index_schema, "baseline shelf summary", errors)
    validate_json_schema(compare_summary, compare_schema, "compare summary", errors)
    validate_drift_digest(summary, compare_summary, errors)
    expected_supporting_surfaces.append(
        build_front_page_surface(
            summary=compare_summary,
            fallback_summary_path=compare_summary_path,
            fallback_report_markdown_path=str(Path(compare_output_root) / "report.md"),
            fallback_check_text_path=str(Path(compare_output_root) / "check.txt"),
            surface_id="shelf_compare",
            role="shelf_compare",
            label_prefix="shelf compare",
            summary_schema="system_compiler.biography_index_compare/v0",
        )
    )
    expected_supporting_surfaces.append(
        build_front_page_surface(
            summary=baseline_summary,
            fallback_summary_path=baseline_summary_path,
            fallback_report_markdown_path=str(Path(baseline_shelf_root) / "biography.index.report.md"),
            fallback_check_text_path=str(Path(baseline_shelf_root) / "biography.index.check.txt"),
            surface_id="baseline_shelf",
            role="baseline_shelf",
            label_prefix="baseline shelf",
            summary_schema="system_compiler.biography_index/v0",
        )
    )
    expected_route_provenance = [
        build_route_provenance_entry(
            summary=candidate_summary,
            fallback_summary_path=candidate_summary_path,
            fallback_report_markdown_path=str(Path(artifact_context.get("candidate_shelf_root", "")) / "biography.index.report.md"),
            fallback_check_text_path=str(Path(artifact_context.get("candidate_shelf_root", "")) / "biography.index.check.txt"),
            route_id="candidate_shelf",
            summary_schema="system_compiler.biography_index/v0",
        ),
        build_route_provenance_entry(
            summary=compare_summary,
            fallback_summary_path=compare_summary_path,
            fallback_report_markdown_path=str(Path(compare_output_root) / "report.md"),
            fallback_check_text_path=str(Path(compare_output_root) / "check.txt"),
            route_id="shelf_compare",
            summary_schema="system_compiler.biography_index_compare/v0",
        ),
        build_route_provenance_entry(
            summary=baseline_summary,
            fallback_summary_path=baseline_summary_path,
            fallback_report_markdown_path=str(Path(baseline_shelf_root) / "biography.index.report.md"),
            fallback_check_text_path=str(Path(baseline_shelf_root) / "biography.index.check.txt"),
            route_id="baseline_shelf",
            summary_schema="system_compiler.biography_index/v0",
        ),
    ]

    compare_mode = review_status.get("compare_mode")
    if compare_mode not in ("self-compare", "baseline-compare"):
        errors.append(f"review_status.compare_mode: unexpected value when compare_enabled=true -> {compare_mode!r}")

    expect_equal(review_verdict, compare_summary.get("shelf_verdict"), "review_verdict", errors)
    expect_equal(review_status.get("baseline_result"), baseline_summary.get("result"), "review_status.baseline_result", errors)
    expect_equal(review_status.get("compare_result"), compare_summary.get("result"), "review_status.compare_result", errors)
    expect_equal(review_status.get("baseline_profile"), baseline_summary.get("profile"), "review_status.baseline_profile", errors)
    expect_equal(
        review_status.get("baseline_biography_count"),
        baseline_summary.get("summary", {}).get("biography_count"),
        "review_status.baseline_biography_count",
        errors,
    )
    expect_equal(
        review_status.get("baseline_unique_world_count"),
        baseline_summary.get("summary", {}).get("unique_world_count"),
        "review_status.baseline_unique_world_count",
        errors,
    )
    expect_equal(
        review_status.get("baseline_compare_attached_count"),
        baseline_summary.get("summary", {}).get("compare_attached_count"),
        "review_status.baseline_compare_attached_count",
        errors,
    )
    expect_equal(
        review_status.get("baseline_not_attached_count"),
        baseline_summary.get("summary", {}).get("not_attached_count"),
        "review_status.baseline_not_attached_count",
        errors,
    )

    expect_equal(
        summary.get("questions", {}).get("compare_questions"),
        compare_summary.get("questions", {}).get("compare_questions", []),
        "questions.compare_questions",
        errors,
    )
    expect_equal(
        summary.get("questions", {}).get("next_questions"),
        compare_summary.get("questions", {}).get("next_questions", []),
        "questions.next_questions",
        errors,
    )
    expect_equal(front_page.get("supporting_surfaces"), expected_supporting_surfaces, "front_page.supporting_surfaces", errors)
    expect_equal(summary.get("route_provenance"), expected_route_provenance, "route_provenance", errors)
    expect_equal(summary.get("collapse_surface"), compare_summary.get("collapse_surface"), "collapse_surface", errors)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler world shelf review summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to review summary JSON. If omitted, --bundle-root/world-shelf.review.summary.json is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing world-shelf.review.summary.json.",
    )
    args = parser.parse_args()

    try:
        import jsonschema  # noqa: F401
    except ImportError:
        print("jsonschema is required. Install it with: python -m pip install jsonschema", file=sys.stderr)
        return 2

    repo_root = Path(__file__).resolve().parent.parent
    if args.summary:
        summary_path = Path(args.summary).resolve()
    else:
        bundle_root = Path(args.bundle_root or "out/system-compiler-world-shelf-review").resolve()
        summary_path = bundle_root / "world-shelf.review.summary.json"

    review_schema_path = (repo_root / REVIEW_SCHEMA_PATH).resolve()
    index_schema_path = (repo_root / INDEX_SCHEMA_PATH).resolve()
    compare_schema_path = (repo_root / COMPARE_SCHEMA_PATH).resolve()

    try:
        summary = load_json(summary_path)
        review_schema = load_json(review_schema_path)
        index_schema = load_json(index_schema_path)
        compare_schema = load_json(compare_schema_path)
        import jsonschema

        jsonschema.validate(summary, review_schema)
        errors: list[str] = []
        validate_references(summary, index_schema, compare_schema, errors)
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] review verdict -> {summary.get('review_verdict', '')}")
    print(f"[OK] compare mode -> {summary.get('review_status', {}).get('compare_mode', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
