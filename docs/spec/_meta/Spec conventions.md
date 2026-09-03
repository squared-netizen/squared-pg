---
title: Spec conventions
tags: [spec, meta]
---

# Spec conventions

Back to [[Specification Index]].

## Normative language

The keywords **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT**, and **MAY** in
this specification are to be interpreted as described in
[RFC 2119](https://www.rfc-editor.org/rfc/rfc2119): they mark requirements
(MUST/MUST NOT) and recommendations (SHOULD/SHOULD NOT, which a conforming
implementation may ignore only with documented justification) and permissive
options (MAY). They are always uppercase. Lowercase "must"/"should" in prose is
non-normative and is avoided — if it is a requirement, uppercase it.

The specification is a **strong guideline that is weakly enforced**: it
describes the intended design and the repository is the authoritative statement
of what currently exists. Where a specification statement and the repository
disagree about a fact of the repository's present state, the repository wins and
the specification is amended to match. Normative requirement keywords are
aspirational for the design and binding on implementations that aim to conform,
but nothing in the specification overrides the authority of the repository as the
ground truth of the current implementation.

Rules:

- The same property MUST NOT be stated as MUST in one section and SHOULD in
  another.
- A MAY MUST NOT be followed by a contradicting MUST.
- Prohibitions are MUST NOT, never "should avoid".

### Determinism vocabulary

| Property | Level | Meaning |
|---|---|---|
| **Resolution determinism** | MUST | Same inputs → same resolved resource set. |
| **Output reproducibility** | SHOULD | Same inputs → byte-identical files. |

Byte-identity is SHOULD because timestamps and archive member ordering are not
worth mandating. Resolution determinism is MUST because everything depends on it.

## Status values

| Status | Meaning |
|---|---|
| `stub` | Outline only. |
| `outline` | Outline agreed, content not written. |
| `draft` | Content written, incomplete. |
| `review` | Content complete, awaiting review. |
| `final` | Reviewed and settled. |
| `blocked` | Cannot progress until a dependency exists. |

## Note structure

1. YAML frontmatter — `section`, `title`, `status`, `tags`
2. `# <number> <title>`
3. Navigation callout — index, parent, prev, next
4. `## Purpose`
5. `## Outline` — checkbox list, retained after writing as the completeness checklist
6. `## Content`
7. `## Open questions` (omitted when none remain)
8. `## Related`

## Formatting

- One second-level section per note. Third-level subsections are headings within,
  addressable as `[[2.4 Engine lifecycle#2.4.3 Service registration]]`.
- ASCII diagrams always inside fenced code blocks.
- Never nest fenced blocks.
- No chat residue.
- Code examples are illustrative unless labelled normative.

## Cross-references

Wikilinks are the canonical form: `[[2.15 Error and transaction model]]`. A bare
`§N.M` reference is permitted when the target note is named on the same line or
is otherwise unambiguous (for example a self-reference within the note, or a
range expressed in the notes tree). When a cross-note reference is the first
mention of that note on a line, prefer the wikilink form. This keeps dense
tables readable without requiring a full link at every call site; it is a weak
style rule, not a compliance gate.

When section A defers to section B, both say so: A under `## Open questions` or
inline, B under `## Related`.

## Naming inside the vault

This vault lives at `squared-pg/docs/spec/`. Note names are prefixed `Spec ` in
`_meta/` to avoid collision with `docs/edu/` notes of the same name (notably
`Glossary`) if `docs/` is opened as a single Obsidian vault.
