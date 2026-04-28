import argparse
import json
import sys
from pathlib import Path


SCHEMA_PATH = "schemas/system_compiler.witness_bundle.v0.schema.json"


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

    artifact_context = summary.get("artifact_context", {})
    if isinstance(artifact_context, dict):
        ensure_exists(artifact_context.get("canonical_world"), "artifact_context.canonical_world", errors)
        ensure_exists(artifact_context.get("runtime_evidence_summary"), "artifact_context.runtime_evidence_summary", errors)
        ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
        ensure_exists(artifact_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
        ensure_exists(artifact_context.get("check_text_path"), "artifact_context.check_text_path", errors)
        for index, report_path in enumerate(artifact_context.get("artifact_reports", [])):
            ensure_exists(report_path, f"artifact_context.artifact_reports[{index}]", errors)

    world = summary.get("world", {})
    if isinstance(world, dict):
        for index, contract_ref in enumerate(world.get("contract_refs", [])):
            ensure_exists(contract_ref, f"world.contract_refs[{index}]", errors)

    for index, entry in enumerate(summary.get("witness_entries", [])):
        if not isinstance(entry, dict):
            errors.append(f"witness_entries[{index}]: invalid entry")
            continue

        status = entry.get("status")
        source_path = entry.get("source_path")
        if status != "missing":
            ensure_exists(source_path, f"witness_entries[{index}].source_path", errors)

        for ref_index, artifact_ref in enumerate(entry.get("artifact_refs", [])):
            ensure_exists(artifact_ref, f"witness_entries[{index}].artifact_refs[{ref_index}]", errors)

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler witness bundle summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to witness bundle summary.json. If omitted, --bundle-root/summary.json is used.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-witness-bundle").resolve()
        summary_path = bundle_root / "summary.json"

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
    print(f"[OK] world -> {summary.get('world', {}).get('name', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
