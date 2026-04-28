import argparse
import json
import sys
from pathlib import Path


SCHEMA_PATH = "schemas/system_compiler.biography.v0.schema.json"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def ensure_exists(path_value: str | None, label: str, errors: list[str]):
    if path_value is None:
        return
    if not isinstance(path_value, str) or not path_value.strip():
        errors.append(f"{label}: missing path")
        return
    if not Path(path_value).exists():
        errors.append(f"{label}: not found -> {path_value}")


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


def validate_references(summary: dict):
    errors: list[str] = []

    front_page = summary.get("front_page", {})
    if isinstance(front_page, dict):
        validate_front_page(front_page, "front_page", errors)

    delivery = summary.get("delivery", {})
    if isinstance(delivery, dict):
        ensure_exists(delivery.get("output_root"), "delivery.output_root", errors)
        ensure_exists(delivery.get("summary_path"), "delivery.summary_path", errors)
        ensure_exists(delivery.get("report_markdown_path"), "delivery.report_markdown_path", errors)
        ensure_exists(delivery.get("check_text_path"), "delivery.check_text_path", errors)

    artifact_context = summary.get("artifact_context", {})
    if isinstance(artifact_context, dict):
        ensure_exists(artifact_context.get("canonical_world_path"), "artifact_context.canonical_world_path", errors)
        ensure_exists(artifact_context.get("runtime_evidence_summary"), "artifact_context.runtime_evidence_summary", errors)
        ensure_exists(
            artifact_context.get("runtime_evidence_report_markdown_path"),
            "artifact_context.runtime_evidence_report_markdown_path",
            errors,
        )
        ensure_exists(
            artifact_context.get("runtime_evidence_check_text_path"),
            "artifact_context.runtime_evidence_check_text_path",
            errors,
        )
        ensure_exists(artifact_context.get("witness_bundle_summary"), "artifact_context.witness_bundle_summary", errors)
        ensure_exists(
            artifact_context.get("witness_bundle_report_markdown_path"),
            "artifact_context.witness_bundle_report_markdown_path",
            errors,
        )
        ensure_exists(
            artifact_context.get("witness_bundle_check_text_path"),
            "artifact_context.witness_bundle_check_text_path",
            errors,
        )
        ensure_exists(artifact_context.get("baseline_witness_summary"), "artifact_context.baseline_witness_summary", errors)
        ensure_exists(artifact_context.get("world_compare_summary"), "artifact_context.world_compare_summary", errors)
        ensure_exists(
            artifact_context.get("world_compare_report_markdown_path"),
            "artifact_context.world_compare_report_markdown_path",
            errors,
        )
        ensure_exists(
            artifact_context.get("world_compare_check_text_path"),
            "artifact_context.world_compare_check_text_path",
            errors,
        )

    world = summary.get("world", {})
    if isinstance(world, dict):
        for index, contract_ref in enumerate(world.get("contract_refs", [])):
            ensure_exists(contract_ref, f"world.contract_refs[{index}]", errors)

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler biography summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to biography summary JSON. If omitted, --bundle-root/biography.summary.json is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default="",
        help="Bundle root containing biography.summary.json.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-biography").resolve()
        summary_path = bundle_root / "biography.summary.json"

    schema_path = (repo_root / SCHEMA_PATH).resolve()

    try:
        summary = load_json(summary_path)
        schema = load_json(schema_path)
        jsonschema.validate(summary, schema)
        errors = validate_references(summary)
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if errors:
        for message in errors:
            print(f"[ERROR] {message}", file=sys.stderr)
        return 1

    print(f"[OK] schema -> {summary_path}")
    print(f"[OK] profile -> {summary.get('profile', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
