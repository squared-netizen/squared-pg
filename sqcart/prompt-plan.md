# Task: Audit the squared-pg specification (PLAN ONLY — make no edits)

You are auditing the squared-pg specification for structural and normative
defects. This is a read-only pass. Do not modify any file. Produce a written
audit report only.

## Scope

Audit these files as a single document:

- `docs/spec/` (Obsidian vault, all notes)
- Item 1 — Repository conventions
- Item 2 — Generator Architecture (section stubs)
- 2.1 Architectural model
- 2.2 Engine responsibility and role
- 2.3 Generation pipeline
- 2.4 Engine lifecycle
- 2.5 Lua control layer
- 2.6 Engine/Lua boundary
- 2.7 Project generation model

Also read `AGENTS.md`, the repository root, and
`sqcart/docs/developer/sqcart-functional-spec.md` for comparison. The sqcart
functional spec is the quality bar: numbered `FR-<AREA>-N` requirements,
no conversational text, consistent normative language.

## What to look for

Report every instance. Do not summarise a class of defect and move on;
enumerate occurrences with file and heading.

**A. Section numbering integrity**
Duplicate section numbers, numbers assigned to two different topics, gaps in
sequences, and any table of contents that does not match the headings that
follow it.

**B. Contradictory duplicate content**
Sections written twice with materially different content. Where two versions
exist, state which is more consistent with the surrounding architecture and
why. Flag any case where both versions define the same mechanism differently.

**C. Cross-reference validity**
Every `§N.N` reference. Verify the target exists and covers the topic being
referenced. Report references pointing at the wrong section, and references
to sections that are still unwritten stubs.

**D. Conversational and meta-commentary leakage**
The spec was authored through dialogue and retains artefacts: phrases like
"Proposed section:", "Continuing from the established model", "This sets up
the next section naturally", "Understood.", and second-person address to a
reader. A normative specification contains none of this. List every
occurrence.

**E. Markdown structural damage**
Unclosed or mismatched code fences, nested fences that break rendering,
ASCII diagrams fragmented by stray fence markers, and headings that have
lost their `#` prefix and now render as body paragraphs.

**F. Normative language**
The spec uses MUST/SHOULD/MAY without ever defining them. Report the absence
of an RFC 2119 conformance clause. Separately, flag any requirement whose
keyword choice looks wrong for its intent (a MUST that reads advisory, or a
SHOULD stating an architectural invariant that cannot be optional).

**G. Spec/repository divergence**
Compare the documented repository layout against the actual tree. Check at
minimum: directory naming (hyphen vs underscore), directories the spec
requires that do not exist, directories that exist and the spec does not
mention, and the vendored dependency list against what is actually in-tree.

**H. Unresolved deferrals**
Requirements that defer a decision to another section. For each, state
whether the target section resolves it or is still a stub.

**I. Redundancy**
Concepts re-explained in full in multiple sections. Identify the canonical
location for each and list the sites that should become cross-references.

**J. Ordering conflicts**
Two sections both prescribe a sequence for the same process. Report any
mismatch in stage count, stage naming, or stage order.

## Output

Write the report to `docs/spec/_meta/spec-audit-01.md`. Structure it as:

1. Summary table: defect class, count, severity (blocking / serious / minor).
2. One section per defect class. Each finding gets an ID (`AUD-A-001`), the
   file and heading, a one-sentence statement of the problem, and the
   proposed correction.
3. A "requires a decision from Jesse" section for anything where the fix is
   not mechanical — contradictory duplicates where both versions are
   defensible, and deferrals whose resolution is a design choice rather than
   an editorial one.

Severity: blocking means the spec cannot be implemented from as written.
Serious means an implementer would likely get it wrong. Minor is editorial.

Do not fix anything. Do not open files for writing except the report.
