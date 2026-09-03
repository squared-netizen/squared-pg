# Task: Apply spec audit corrections (EDIT MODE)

Read `docs/spec/_meta/spec-audit-01.md`. Apply only the findings I have
marked `APPROVED` in that file. Skip everything else, including findings in
the "requires a decision" section unless I have written a resolution inline.

## Rules

- File-wide rewrites, not patches. When a section changes, emit the complete
  corrected file.
- Preserve all normative content. Renumbering, deduplication, and cross-
  reference repair must not silently drop or weaken a requirement. If a
  correction would change what a requirement demands, stop and report it
  instead of applying it.
- When merging duplicate sections, keep every distinct requirement from both
  versions. Reconcile contradictions in favour of the version the audit
  identified as canonical, and note the discarded alternative in
  `docs/spec/_meta/decisions.md` as a new D-NNN entry.
- ASCII diagrams: repair fences, keep the diagram content byte-identical.
- Add the RFC 2119 conformance clause as a new §0 if that finding is
  approved.

## After editing

- Update the decision log with any D-NNN entries created.
- Update the status dashboard to reflect newly resolved deferrals.
- Append a `## Applied` section to `spec-audit-01.md` listing each finding ID
  with its outcome: applied, skipped, or escalated.
- Report which cross-references now point at sections that are still stubs.
  These are the next spec-writing targets.

Do not restructure sections beyond what the approved findings require. Do not
add new requirements.
