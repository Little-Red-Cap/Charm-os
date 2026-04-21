import argparse
import json
import sys
from pathlib import Path


SCHEMA_FILES = {
    "materialized_graph.export_case_manifest/v1": "schemas/materialized_graph.export_case_manifest.v1.schema.json",
    "materialized_graph.sample/v2": "schemas/materialized_graph.sample.v2.schema.json",
    "materialized_graph.export_bundle/v1": "schemas/materialized_graph.export_bundle.v1.schema.json",
    "materialized_graph.bundle_diff/v1": "schemas/materialized_graph.bundle_diff.v1.schema.json",
    "materialized_graph.ci_summary/v1": "schemas/materialized_graph.ci_summary.v1.schema.json",
    "materialized_graph.report_manifest/v1": "schemas/materialized_graph.report_manifest.v1.schema.json",
    "system_compiler.artifact_report/v0": "schemas/system_compiler.artifact_report.v0.schema.json",
    "system_compiler.runtime_observe_snapshot/v0": "schemas/system_compiler.runtime_observe_snapshot.v0.schema.json",
    "system_compiler_result_map/v0": "schemas/system_compiler_result_map.v0.schema.json",
    "system_compiler_summary/v0": "schemas/system_compiler_summary.v0.schema.json",
    "system_input_summary/v0": "schemas/system_input_summary.v0.schema.json",
    "binding_result_summary/v0": "schemas/binding_result_summary.v0.schema.json",
    "bringup_order_summary/v0": "schemas/bringup_order_summary.v0.schema.json",
    "system_formation_summary/v0": "schemas/system_formation_summary.v0.schema.json",
}


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def resolve_path(base: Path, path_value: str) -> Path:
    path = Path(path_value)
    if path.is_absolute():
        return path
    return (base / path).resolve()


def load_schema(repo_root: Path, schema_name: str):
    schema_rel = SCHEMA_FILES.get(schema_name)
    if schema_rel is None:
        raise RuntimeError(f"unsupported schema: {schema_name}")
    return load_json(repo_root / schema_rel)


def build_schema_store(repo_root: Path):
    store = {}
    for schema_rel in sorted(set(SCHEMA_FILES.values())):
        schema = load_json(repo_root / schema_rel)
        schema_id = schema.get("$id")
        if isinstance(schema_id, str) and schema_id:
            store[schema_id] = schema
    return store


def validate_file(path: Path, repo_root: Path):
    import jsonschema

    data = load_json(path)
    schema_name = data.get("schema")
    if not isinstance(schema_name, str) or not schema_name:
        schema_name = data.get("kind")
    if not isinstance(schema_name, str) or not schema_name:
        raise RuntimeError(f"schema/kind field missing: {path}")

    schema = load_schema(repo_root, schema_name)
    resolver = jsonschema.RefResolver.from_schema(schema, store=build_schema_store(repo_root))
    jsonschema.validate(data, schema, resolver=resolver)
    print(f"[OK] {schema_name} -> {path}")
    return data


def validate_bundle_root(bundle_root: Path, repo_root: Path, visited: set[Path]):
    index_path = (bundle_root / "index.json").resolve()
    index_data = validate_once(index_path, repo_root, visited)
    for case_entry in index_data.get("cases", []):
        case_kind = case_entry.get("case_kind", "materialized_graph")
        json_value = case_entry.get("json")
        runtime_observe_value = case_entry.get("runtime_observe")

        if isinstance(json_value, str) and json_value:
            json_path = resolve_path(bundle_root.resolve(), json_value)
            validate_once(json_path, repo_root, visited)
        elif case_kind != "runtime_only":
            raise RuntimeError(
                f"bundle case '{case_entry.get('name', '<unknown>')}' is missing json for case_kind={case_kind}"
            )

        if case_kind == "runtime_only" and not (isinstance(runtime_observe_value, str) and runtime_observe_value):
            raise RuntimeError(
                f"runtime_only bundle case '{case_entry.get('name', '<unknown>')}' is missing runtime_observe"
            )


def validate_ci_output_root(ci_root: Path, repo_root: Path, visited: set[Path]):
    summary_path = (ci_root / "summary.json").resolve()
    summary = validate_once(summary_path, repo_root, visited)

    candidate = summary.get("candidate")
    if isinstance(candidate, dict) and candidate.get("index"):
        validate_once(Path(candidate["index"]).resolve(), repo_root, visited)

    baseline = summary.get("baseline")
    if isinstance(baseline, dict) and baseline.get("index"):
        validate_once(Path(baseline["index"]).resolve(), repo_root, visited)

    diff = summary.get("diff")
    if isinstance(diff, dict) and diff.get("json"):
        validate_once(Path(diff["json"]).resolve(), repo_root, visited)

    report = summary.get("report")
    if isinstance(report, dict) and report.get("manifest"):
        validate_once(Path(report["manifest"]).resolve(), repo_root, visited)

    artifact_report = summary.get("artifact_report")
    if isinstance(artifact_report, dict):
        for case_entry in artifact_report.get("cases", []):
            path_value = case_entry.get("path")
            if isinstance(path_value, str) and path_value:
                validate_once(Path(path_value).resolve(), repo_root, visited)


def validate_manifest_path(manifest_path: Path, repo_root: Path, visited: set[Path]):
    validate_once(manifest_path.resolve(), repo_root, visited)


def validate_once(path: Path, repo_root: Path, visited: set[Path]):
    resolved = path.resolve()
    if resolved in visited:
        return load_json(resolved)
    data = validate_file(resolved, repo_root)
    visited.add(resolved)
    schema_name = data.get("schema")
    if not isinstance(schema_name, str) or not schema_name:
        schema_name = data.get("kind")

    if schema_name == "materialized_graph.export_bundle/v1":
        bundle_root = resolved.parent
        for case_entry in data.get("cases", []):
            path_value = case_entry.get("runtime_observe")
            if isinstance(path_value, str) and path_value:
                validate_once(resolve_path(bundle_root, path_value), repo_root, visited)

    if schema_name == "system_compiler.artifact_report/v0":
        artifacts = data.get("artifacts")
        if isinstance(artifacts, dict):
            path_value = artifacts.get("runtime_observe")
            if isinstance(path_value, str) and path_value:
                validate_once(resolve_path(resolved.parent, path_value), repo_root, visited)

    return data


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate exported JSON artifacts against repo schemas.")
    parser.add_argument("paths", nargs="*", help="JSON artifact paths to validate directly")
    parser.add_argument("--export-case-manifest", action="append", default=[], help="Validate export case manifest JSON files")
    parser.add_argument("--bundle-root", action="append", default=[], help="Validate bundle index.json and referenced case JSONs")
    parser.add_argument("--ci-output-root", action="append", default=[], help="Validate CI summary.json and linked artifacts")
    args = parser.parse_args()

    try:
        import jsonschema  # noqa: F401
    except ImportError:
        print("jsonschema is required. Install it with: python -m pip install jsonschema", file=sys.stderr)
        return 2

    repo_root = Path(__file__).resolve().parent.parent
    visited: set[Path] = set()

    try:
        for manifest_path in args.export_case_manifest:
            validate_manifest_path(Path(manifest_path).resolve(), repo_root, visited)

        for bundle_root in args.bundle_root:
            validate_bundle_root(Path(bundle_root).resolve(), repo_root, visited)

        for ci_root in args.ci_output_root:
            validate_ci_output_root(Path(ci_root).resolve(), repo_root, visited)

        for path_value in args.paths:
            validate_once(Path(path_value).resolve(), repo_root, visited)
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1

    if not args.export_case_manifest and not args.bundle_root and not args.ci_output_root and not args.paths:
        print("[WARN] nothing to validate")
        return 0

    print(f"[OK] validated {len(visited)} artifact(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
