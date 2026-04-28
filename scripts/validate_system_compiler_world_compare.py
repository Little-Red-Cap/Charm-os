import argparse
import json
import sys
from pathlib import Path


SCHEMA_PATH = "schemas/system_compiler.world_compare.v0.schema.json"


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


def validate_references(summary: dict):
    errors: list[str] = []

    artifact_context = summary.get("artifact_context", {})
    if isinstance(artifact_context, dict):
        ensure_exists(artifact_context.get("baseline_witness_bundle"), "artifact_context.baseline_witness_bundle", errors)
        ensure_exists(artifact_context.get("candidate_witness_bundle"), "artifact_context.candidate_witness_bundle", errors)
        ensure_exists(artifact_context.get("output_root"), "artifact_context.output_root", errors)
        ensure_exists(artifact_context.get("report_markdown_path"), "artifact_context.report_markdown_path", errors)
        ensure_exists(artifact_context.get("check_text_path"), "artifact_context.check_text_path", errors)

    world = summary.get("world", {})
    if isinstance(world, dict):
        for index, contract_ref in enumerate(world.get("contract_refs", [])):
            ensure_exists(contract_ref, f"world.contract_refs[{index}]", errors)

    for index, entry in enumerate(summary.get("witness_changes", [])):
        if not isinstance(entry, dict):
            errors.append(f"witness_changes[{index}]: invalid entry")
            continue
        ensure_exists(entry.get("left_source_path"), f"witness_changes[{index}].left_source_path", errors)
        ensure_exists(entry.get("right_source_path"), f"witness_changes[{index}].right_source_path", errors)
        for ref_index, artifact_ref in enumerate(entry.get("artifact_refs_added", [])):
            ensure_exists(artifact_ref, f"witness_changes[{index}].artifact_refs_added[{ref_index}]", errors)
        for ref_index, artifact_ref in enumerate(entry.get("artifact_refs_removed", [])):
            ensure_exists(artifact_ref, f"witness_changes[{index}].artifact_refs_removed[{ref_index}]", errors)

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate system compiler world compare summary and referenced artifacts."
    )
    parser.add_argument(
        "--summary",
        default="",
        help="Path to world compare summary.json. If omitted, --bundle-root/summary.json is used.",
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
        bundle_root = Path(args.bundle_root or "out/system-compiler-world-compare").resolve()
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
    print(f"[OK] world verdict -> {summary.get('world_verdict', '')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
