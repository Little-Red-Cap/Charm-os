# charm-docs-utf8

> status: `supporting`

Use this skill when Markdown displays mojibake, strict decoding fails or a task
changes Chinese text.

## Procedure

1. Read the file with explicit UTF-8 and compare bytes/Git content before
   deciding that the file is corrupt.
2. Distinguish terminal decoding, valid-but-mojibake text and invalid UTF-8.
3. Recover damaged text only from a trustworthy source, source code or Git
   history. Do not infer missing Chinese prose from context.
4. Keep an unrecoverable file out of recommended routes and mark it damaged.
5. Validate strict UTF-8, Markdown links, `git diff --check` and the affected
   document route after editing.

PowerShell session and decoder commands are documented in
[`Powershell设置utf8.md`](../../../project/tooling/Powershell设置utf8.md). Repository
document roles and deletion rules are in
[`documentation_maintenance.md`](../../../documentation_maintenance.md).
