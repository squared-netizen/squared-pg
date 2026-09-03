---
title: Spec audit 01
tags: [spec, meta, audit]
---

# Spec audit 01

```
Audit run:  2026-09-02
Task:       sqcart/prompt-plan.md (read-only pass)
Scope:      all 39 notes in docs/spec/ + repository root + AGENTS.md
            (task scope list stops at §2.7; the vault runs to §2.16 and the
            meta notes, so the whole vault was audited as one document)
Status:     each finding below awaits an APPROVED / REJECTED mark before
            any edit (see sqcart/prompt-edit.md)
```

## Summary

| Class | Count | Blocking | Serious | Minor |
|---|---|---|---|---|
| A — section numbering integrity | 3 | 0 | 1 | 2 |
| B — contradictory duplicate content | 2 | 0 | 1 | 1 |
| C — cross-reference validity | 2 | 0 | 1 | 1 |
| D — conversational / meta leakage | 2 | 0 | 1 | 1 |
| E — markdown structural damage | 3 | 0 | 2 | 1 |
| F — normative language | 2 | 0 | 1 | 1 |
| G — spec/repository divergence | 6 | 2 | 1 | 3 |
| H — unresolved deferrals | 1 | 0 | 1 | 0 |
| I — redundancy | 7 | 0 | 0 | 7 |
| J — ordering conflicts | 0 | 0 | 0 | 0 |
| **Total** | **28** | **2** | **9** | **17** |

Severity (per task): blocking = cannot be implemented from as written; serious =
an implementer would likely get it wrong; minor = editorial.

Verified-clean positives are recorded inline under each class (A-004, C-001,
E-004, F-002, G-007, H-002, J-001, J-002). No D-class remnants of the kind the
task warns about were found in any note body.

---

## A — Section numbering integrity

### AUD-A-001 — §2.7 outline names a subsection that the headings do not
**Status:** awaiting decision · **Severity:** serious

File: `02 Generator architecture/2.7 Project generation model.md`
Outline item 2.7.12 ("Generated project boundary", line 46) vs heading
`## 2.7.12 Ownership transition` (line 1630). The TOC is the first thing a
reader uses; it points at a section that has a different name. The subsection
then repeats its own title as `### Ownership transition` (line 1636).

**Correction:** align the two: rename the heading to "Generated project boundary"
or the outline to "Ownership transition"; delete the redundant `### Ownership
transition` sub-heading. Decide alongside AUD-B-001.

### AUD-A-002 — §2.5 outline title drift
**Status:** awaiting decision · **Severity:** minor

File: `02 Generator architecture/2.5 Lua control layer.md`
Outline "2.5.4 CLI control" vs heading `### 2.5.4 Command-line interface control`.

**Correction:** make the outline item read "2.5.4 Command-line interface control".

### AUD-A-003 — §2.6 outline title drift (three items)
**Status:** awaiting decision · **Severity:** minor

File: `02 Generator architecture/2.6 Engine-Lua boundary.md`
Outline "2.6.7 Resource identity" vs heading `Resource identity across the
boundary`; "2.6.9 Ownership and lifetime" vs `Ownership and lifetime rules`;
"2.6.13 Filesystem abstraction" vs `Filesystem and path abstraction`.

**Correction:** update the outline to the actual heading titles.

### AUD-A-004 — numbering continuity: clean
Subsection sequences are contiguous in every file — no gaps or duplicated
numbers (verified mechanically: 2.4.1–2.4.13, 2.5.1–2.5.11, 2.6.1–2.6.16,
2.7.0–2.7.13, 2.8.1–2.8.11, 2.9.1–2.9.9, 2.10.1–2.10.9, 2.11.1–2.11.11,
2.12.1–2.12.6, 2.13.1–2.13.6, 2.14.1–2.14.7, 2.15.1–2.15.14, 2.16.1–2.16.4).
No action.

---

## B — Contradictory duplicate content

### AUD-B-001 — §2.7.12 restates the boundary guarantee whose home is §2.12
**Status:** requires decision · **Severity:** serious

File: `02 Generator architecture/2.7 Project generation model.md`
Heading: `2.7.12 Ownership transition`

The subsection's own opening note (lines 1632–1634) says the guarantee "is
stated in [[2.12 Generated-project boundary]]" and that §2.7.12 defines only the
mechanism — exactly what D-006a decided. Yet `### Independence` (lines
1658–1670) and `### Leakage prohibition` (lines 1672–1684) restate the
guarantee verbatim, and the two lists diverge:

- Leakage: 2.7.12 has 5 bullets; §2.12.2 has 7 (missing "resources from
  `resources/generator/`" and "generator-internal state, handles, or temporary
  artifacts").
- Independence: 2.7.12 says "relocate"; §2.12.1 says "relocate to another path,
  machine, or filesystem".

An implementer reconciling the two sources cannot tell which list is
authoritative. §2.12 is the more consistent version: it is the section D-006a
created for this purpose and the section §2.16.2's register cites.

**Correction:** delete `### Independence` and `### Leakage prohibition` from
2.7.12 (keep the ownership-transfer mechanism) and leave the guarantee in §2.12
only, cited via the existing link. Confirm no distinct requirement is lost in
the deletions (compare against AUD-A-001).

### AUD-B-002 — the Template/Kit/Package/Asset taxonomy block is written five times
**Status:** requires decision · **Severity:** minor

Instances: `2.7` lines 788–791 (`2.7.6`), 1696–1699 (`2.7.13`), 1019–1022
(`2.7.7`, variant wording); `2.11` lines 53–56 (`2.11.1`); `Spec glossary`
Resources table. No contradiction between copies today, but four normative
restatements of one taxonomy will drift.

**Correction:** make the glossary (or 2.11.1) canonical and turn the other sites
into cross-references (`[[2.11 Packages]]` or `[[Spec glossary]]`), keeping any
sentence that adds a requirement beyond the taxonomy itself.

---

## C — Cross-reference validity

### AUD-C-001 — targets: clean
All 75 `§N.N` references resolve to real, written headings (no stubs exist in
the vault — status dashboard reports `stub` 0), and every `[[wikilink]]` target
is a real note (the two flagged by naive scan, `[[wikilink]]` in README and
`[[nodiscard]]` in the AGENTS proposal, are prose, not links). No reference in
the "wrong section" class was found; `2.10 §2.7.7`, `2.11 §2.7.6/§2.7.11`,
`2.14 §2.4.9/§2.6.15`, `2.15 §2.4.7/§2.6.12/§2.7.9`, `2.8 §2.7.4`, `2.9
§2.7.5/§2.8.6`, `2.6 §2.4.6/§2.5.9`, `2.5 §2.1.6` all land on the right topic.
No action.

### AUD-C-002 — bare `§` numbers without wikilinks
**Status:** awaiting decision · **Severity:** serious

Per `Spec conventions` ("Wikilinks only … Never a bare section number in prose
without a link"), 46 `§` references sit on lines with no wikilink, mostly in the
two heaviest notes:

`2.7 Project generation model.md` — 24 instances, lines 29, 74, 145, 165, 208
(`§2.5.5`, `§2.5.6`), 262, 349, 351, 360, 385, 413, 536, 626, 752, 769, 929,
1098, 1159, 1309, 1409, 1436, 1464, 1569, 1670.

`2 Generator architecture.md` — 4 instances, lines 50, 51, 52 (two refs), 55.

`Spec decisions.md` — 9 instances, lines 100, 121, 128, 129, 134, 209, 219,
222, 223.

`Spec open questions.md` — 2 instances, lines 25, 28.
`README.md` — 2 (range notation "§1.1 – §1.13", not references).
`Specification Index.md` — 1, line 63.
`2.9 Kits.md` — 1, line 185.
`2.8 Templates.md` — 1, line 146 (self-reference within the same note).
`2.4 Engine lifecycle.md` — 1, line 237.
`2.13 Dependency graph.md` — 1, line 14.

A further five appear on lines that do contain a link but keep a bare suffix:
`1 Repository conventions.md:40`, `2.3 Generation pipeline.md:17` (second ref),
`Spec status.md:34` and `:44`.

**Correction option 1 (strict):** convert every bare `§N.N` to
`[[Note name]]` with the number as an anchor suffix (e.g.
`[[2.7 Project generation model]] §2.7.10`), matching the pattern already used
throughout. **Option 2 (document):** amend `Spec conventions` so bare `§N.N`
is permitted when the note is named on the same line or in the enclosing
table, and keep the 50+ instances. Decide; if option 1, this is a file-wide
mechanical pass over the listed files.

### AUD-C-003 — the reciprocal-definition rule has exactly one broken pair
**Status:** awaiting decision · **Severity:** minor

`Spec conventions` requires that when A defers to B, B says so too. Checked for
every `## Open questions` block and every "see also" deferral: all pairs are
reciprocal except the Q-25 / §2.16 pair — see AUD-H-001.

---

## D — Conversational and meta-commentary leakage

### AUD-D-001 — `Repository AGENTS.md` is still labelled "proposed" though it shipped
**Status:** awaiting decision · **Severity:** minor

File: `_meta/Repository AGENTS.md`, heading "Repository `AGENTS.md` — proposed
rewrite". The note (and `Specification Index` line 88) presents the embedded
`AGENTS.md` as a *proposal*. Comparison with the repository root `AGENTS.md`
shows the proposal was adopted verbatim — "C++20 exclusively", the offline hard
constraint, the type-aliasing rules, the deliverable format, the completion
report are all present in the live file. The meta note is stale.

**Correction:** no normative content to lose; mark the note as adopted and
re-point it at the shipped file (or delete it, keeping the diff rationale as a
decision entry). See also AUD-I-007.

### AUD-D-002 — §2.7 places `Open questions` and `Related` before `Content`
**Status:** awaiting decision · **Severity:** serious

File: `02 Generator architecture/2.7 Project generation model.md`
The forced note order per `Spec conventions` is Purpose → Outline → Content →
Open questions → Related. In 2.7 the order is Purpose (14) → Outline (32) →
**Open questions (49)** → **Related (57)** → Content (61), so all fourteen
numbered subsections (written, complete content) trail the section trailers.
Anyone reading the note top-down is told what is still open before being told
what the section defines.

**Correction:** move `## Open questions` and `## Related` below `## Content`
(mechanical re-order during the E-002 rewrite).

---

## E — Markdown structural damage

### AUD-E-001 — duplicated sentence in §2.7.3 Build layer
**Status:** awaiting decision · **Severity:** serious

File: `02 Generator architecture/2.7 Project generation model.md`,
heading `2.7.3 Workspace model` → `#### Build layer` (lines 347–351)

The sentence "The ownership class of every build file MUST be declared by the
template (§2.7.10)." appears twice back-to-back. A paste error; either copy
reads as a single normative requirement and the duplication adds a MUST.

**Correction:** remove the second copy (lines 350–351). No content change.

### AUD-E-002 — §2.7 numbered subsections at the wrong heading level
**Status:** requires decision · **Severity:** serious

File: `02 Generator architecture/2.7 Project generation model.md`
All numbered subsections (`2.7.0`–`2.7.13`) use `##` (H2) while every sibling
file uses `###` for numbered subsections and `##` only for Purpose/Outline/
Content/Open questions/Related. `Spec conventions` is explicit: "One second-level
section per note. Third-level subsections are headings within." The `## 2.7.0
Terminology` heading also makes the terminology table a top-level block even
though it is section-local. Anchors still resolve by heading text, but the
Obsidian outline and any automated linter see a structurally different document.

**Correction:** re-tier `## 2.7.N` → `### 2.7.N` file-wide (and demote the
deeper `####`/`#####` headings to match), then apply D-002's order fix in the
same pass.

### AUD-E-003 — §2.15 inserts unnumbered H2 part dividers among numbered H3s
**Status:** awaiting decision · **Severity:** minor

File: `02 Generator architecture/2.15 Error and transaction model.md`
`## Part 1 — Errors` (line 35) and `## Part 2 — Transactions` (line 152) are a
second H2 level inside one note, interleaved with `### 2.15.N` subsections.
Consistent with nothing else in the vault and with the one-H2 rule.

**Correction:** demote both to `###` (e.g. `### 2.15.0 Part 1 — Errors`), or to
a bold lead-in, keeping the two-part split visible.

### AUD-E-004 — fences and diagrams: clean
All 39 notes have balanced ```` ``` ```` fences (no unclosed or nested fences)
and the ASCII diagrams (2.1.9, 2.4.10, 2.5.1, 2.6.2, 2.7.x, 2.13.1, 2.15
staging states, etc.) are intact inside their fences. No heading has lost its
`#` prefix (no bare "2.N.M" body-paragraph text). No action.

---

## F — Normative language

### AUD-F-001 — the spec never defines MUST/SHOULD/MAY
**Status:** awaiting decision · **Severity:** serious

No vault note contains an RFC 2119 conformance clause: the normative meaning of
MUST/SHOULD/MAY, what reading rules the reader applies (e.g. RFC 2119's
"implementation may ignore a SHOULD only with justification"), and the
hard-vs-soft split for the keyword set. `Spec conventions` *uses* the capitals
and the determinism table but nowhere states the standard-reading clause.

**Correction:** add the clause as §0 (e.g. in `Spec conventions`, or the
vault-level entry note) in the form the follow-up pass already anticipates.

### AUD-F-002 — keyword choices: internally consistent
The MUST/SHOULD census and a contradiction sweep over the requirement pairs
found no property stated as MUST in one note and SHOULD in another, and no
contradicting MAY/MUST pair. Reproducibility (SHOULD, byte-identical) vs
resolution determinism (MUST) is stated identically in `Spec conventions`,
§2.12.4, §2.7.13 and §2.10.5. No lowercase "must"/"should" reads as an
unmarked requirement (2.15.7's "should therefore" is prose). One item is
deferred to a decision (AUD-F-003).

### AUD-F-003 — §2.7.9's reference-phase order is SHOULD, D-027 calls it "normative"
**Status:** requires decision · **Severity:** minor

File: `02 Generator architecture/2.7 Project generation model.md`, heading
`2.7.9 Generation phases`; and `Spec decisions` D-027 / `Spec status` (2.3 row:
"conceptual; defers order to §2.7.9").

The reference order is introduced as "A generation workflow SHOULD follow this
order" while two meta notes describe §2.7.9 as "the normative engine phase
order". No contradiction in fact — phase numbering, naming and the
resolve-before-mutate rule are consistent across §2.3, §2.3.7, §2.7.9, §2.11.3,
§2.15.7 — but the keyword reads advisory for something described as normative.
Keep SHOULD (Lua may legitimately reorder, and §2.7.1 forbids the engine from
fixing order), then soften the "normative" label in D-027/Spec status instead.

---

## G — Spec/repository divergence

### AUD-G-001 — vendored-dependency directory is wrong-named and empty
**Status:** blocking · **Severity:** blocking

Spec: §1.1 division `third_party/`; §1.6 `third_party/` containing Lua 5.4.8,
LuaFileSystem 1.8.0, Penlight 1.14.0, LDoc 1.5.0, miniz 3.1.2, yyjson 0.12.0,
plus `third_party/cache/` for downloaded archives; §1.13 offline invariant; the
dependency graph (§2.13.2) draws `third_party/` as a leaf.

Repo: the directory is `third-party/` (hyphen) and contains only `.gitkeep`.
None of the six dependencies exists in-tree; there is no `cache/` directory.

**Correction:** either (a) rename the repo directory to `third_party/` and
provision the six vendored deps + `cache/`, or (b) amend §1.1/§1.6/§2.13.2 to
the hyphenated name and record the dependency list as provisional. This is a
repo-vs-spec decision (see Jesse section).

### AUD-G-002 — `resources/` required but absent
**Status:** blocking · **Severity:** blocking

Spec: §1.1 division lists `resources/`; §1.5 defines the five namespaces
(`assets/`, `generator/`, `kits/`, `packages/`, `templates/`) and the
`manifest.json` rule; §2.10 (resource storage), §2.7.7 (asset namespaces),
§2.12.2 ("resources from `resources/generator/`") and §2.13.3 all rely on it.

Repo: no `resources/` directory at the root.

**Correction:** create `resources/` with the declared namespaces (which may be
empty with `.gitkeep` until authored), or amend the spec to defer the resource
tree to the resources milestone. Decision required.

### AUD-G-003 — `.luarc.json` required at the root but absent
**Status:** awaiting decision · **Severity:** serious

Spec: §1.2 ("The repository root MUST contain … `.luarc.json`").

Repo: root has `.clang-format`, `.clang-tidy`, `.gitignore` — no `.luarc.json`.

**Correction:** add a root `.luarc.json` (least-privilege config matching the
tooling the spec's README/AGENTS describe), or amend §1.2.

### AUD-G-004 — `docs/programmer/` and `docs/developer/` absent
**Status:** awaiting decision · **Severity:** minor

Spec: §1.8 ("`squared-pg` MUST maintain two parallel, cross-linked
documentation trees") at `docs/programmer/` and `docs/developer/`. The spec's
own dashboard acknowledges this (`Spec status` — "What this specification does
not yet cover": "no pages yet"), so it is a tracked gap rather than a silent
divergence. §2.15.3 additionally mandates that every error `code` be documented
in `docs/programmer/`.

**Correction:** none in this pass — record as scheduled work; §2.15.3's
obligation fires when the first codes ship.

### AUD-G-005 — `sqcart/` exists but no section mentions it
**Status:** requires decision · **Severity:** minor

Repo: `sqcart/` is a second, self-contained project (own `AGENTS.md`,
`CMakeLists.txt`, `src/`, `tests/`, `docs/`, `tools/`) at the repository root.

Spec: §1.1's division names `engine/ lua/ resources/ third_party/ tools/ docs/`
plus build/docs at root; §1.2 permits extra root files "when they serve a
repository-wide purpose" — whether `sqcart/` (a nested project, not a file)
falls inside that allowance is an open judgement call.

**Correction:** either add `sqcart/` to the §1.1 division (with a one-line
purpose) or record in Spec status why it is excluded.

### AUD-G-006 — `docs/index.md` exists but is absent from §1.8
**Status:** awaiting decision · **Severity:** minor

Repo: `docs/index.md` present. Spec §1.8 lists `docs/programmer/`,
`docs/developer/`, `docs/spec/`, `docs/edu/`, `docs/history.md`, `docs/todo.md`.

**Correction:** add `index.md` to §1.8's division (or drop the file). Trivial.

### AUD-G-007 — conforming components: clean
`engine/` matches §1.3 (`CMakeLists.txt`, `AGENTS.md`, `README.md`, `include/`,
`src/`); `lua/` matches §1.4 (`AGENTS.md`); `tools/` matches §1.7
(`clang-check.fish`, `lua-check.fish`). `docs/edu/Glossary.md` exists, which is
what the README's naming note for `Spec glossary` assumes. No action.

---

## H — Unresolved deferrals

### AUD-H-001 — Q-25 is open in the meta register and nowhere in its own section
**Status:** requires decision · **Severity:** serious

Deferral: `Spec open questions` Q-25 ("Invariant testability", §2.16) with no
decision entry.

Target state: `2.16 Architectural invariants` has **no** `## Open questions`
block at all — it neither resolves nor re-defers Q-25. Its content has moved on:
the register §2.16.2 gives all of I-01…I-18 a detection mechanism and §2.16.3
discusses the known weak spot (I-10, "no undocumented global state"). The
question the meta register keeps open is, on the face of it, answered — but
nothing records that, and `Spec status` still annotates 2.16 with "Q-25
testability".

**Correction:** record the resolution (a new D-NNN: "invariant register names a
detection mechanism per invariant; I-10 partially checkable, gap documented in
§2.16.3"), move Q-25 from Open to the resolved table in `Spec open questions`,
drop the "Q-25" annotation in `Spec status`, and add the reciprocal link in
§2.16. See AUD-C-003.

### AUD-H-002 — remaining open questions: targets written and reciprocal
Q-21 (in `2.8`, open-questions block present) → `Spec open questions` Q-21, no
target section; Q-22 (blocks in `2.9`, `2.11`, `2.7`) → targets written;
Q-23 (`2.7`) → target written; Q-24 (`2.7`, `2.15`) → targets written. Every
deferral with a home has a written, non-stub home; only H-001 breaks the
pattern. No action beyond H-001.

---

## I — Redundancy

### AUD-I-001 — §2.7.0 Terminology duplicates the glossary, with drift
**Status:** requires decision · **Severity:** minor

File: `02 Generator architecture/2.7 Project generation model.md`, heading
`2.7.0 Terminology` (lines 63–78). Seven rows (Workspace, Working directory,
Resource, Materialization, Provenance, Ownership class, Finalization) restate
`Spec glossary` terms. The glossary's own preamble says "Drift between these is
a defect", and drift already exists: glossary "Working directory … Reference
convention `sq_app/`" vs 2.7.0 "The subtree the user authors code in. A subset
of the workspace."

**Correction:** pick one canonical home (recommend `Spec glossary`) and reduce
2.7.0 to the rows specific to generation composition plus a
`[[Spec glossary]]` link, or keep the table and gate it to the glossary's
exact wording.

### AUD-I-002 — "An invalid template MUST NOT enter generation." verbatim twice
**Status:** awaiting decision · **Severity:** minor

`2.8 Templates.md:311` (…2.8.10 Validation) and `2.7 Project generation
model.md:608` (…2.7.4 Template validation).

**Correction:** keep it in `2.8` (the template's own validation section) and
make the `2.7` site a reference.

### AUD-I-003 — "The engine MUST validate the complete kit dependency graph before application." verbatim twice
**Status:** awaiting decision · **Severity:** minor

`2.9 Kits.md:160` (…2.9.5 Dependencies, beside its own identical rule "The
engine MUST validate the complete kit dependency graph before application" is
its own sentence — actually this IS the sentence) and `2.7 Project generation
model.md:713` (…2.7.5 Kit dependencies).

**Correction:** keep in `2.9`; reference from `2.7`.

### AUD-I-004 — "During generation the engine owns resolved asset state, transformation state," verbatim twice
**Status:** awaiting decision · **Severity:** minor

`2.10 Assets and generator resources.md:158` (…2.10.8 Ownership) and `2.7
Project generation model.md:1149` (…2.7.7 Asset ownership and user
modification).

**Correction:** keep the ownership paragraph in one place; reference the other.

### AUD-I-005 — "Resolution MUST complete before materialization." twice within 2.7
**Status:** awaiting decision · **Severity:** minor

`2.7` lines 847 (…2.7.6 Package resolution) and 1081 (…2.7.7 Asset resolution),
plus a variant in `2.11.3`. Consistent parallel phrasing of one rule per
resource type; acceptable under D-006's composition/restatement split, but if
trimming, the canonical rule lives in `2.11.3`.

### AUD-I-006 — "Materialization MUST occur through engine-controlled filesystem operations. The …" twice within 2.7
**Status:** awaiting decision · **Severity:** minor

`2.7` lines 922 (…2.7.6 Package materialization) and 1094 (…2.7.7 Asset
materialization). Same as I-004/I-005 — mechanical parallelism, no drift.

### AUD-I-007 — the shipped AGENTS.md and its meta proposal are the same text
**Status:** awaiting decision · **Severity:** minor

`docs/spec/_meta/Repository AGENTS.md` embeds the adopted `AGENTS.md` verbatim;
the live file is that text. One source duplicated.

**Correction:** fold into AUD-D-001 (retire or re-point the meta note).

---

## J — Ordering conflicts

### AUD-J-001 — §2.3 (12 conceptual stages) vs §2.7.9 (18 reference phases): reconciled
**Status:** verified clean

Stage counts and names differ by design (D-027); §2.3.1's 12 rows are the
workflow-facing view and §2.7.9's 18 steps the engine-facing one. Every §2.3
stage maps onto a subset of the 18 phases; the transfer points (resolution
before mutation; transaction opening) agree, and each note cites the other
reciprocally. No mismatch to repair. No action.

### AUD-J-002 — the 2.7.11 update sequence (1–9) is consistent with the 2.7.9 reference order
**Status:** verified clean

The nine-step update sequence reuses phases 1–7 plus transaction/apply steps
and adds none that conflict with the reference order. No action.

---

## Requires a decision from Jesse

> [!note] Resolution applied 2026-09-02
> Jesse directed: *"for now the project is the one truth, and the spec is the
> strong guideline, weakly enforced guideline. write the spec using that as the
> guiding principle."* Under that directive, decision items **1** (repo wins for
> presence/naming → G-001, G-002), **2** (keep "Ownership transition"; §2.12
> canonical → A-001, B-001), **5** (re-tier → E-002, D-002), **6** (amend the
> convention to permit bare `§` → C-002), **7** (keep SHOULD → F-003),
> **8** (resolve Q-25 → H-001/C-003), **9** (add conformance clause → F-001),
> **10** (soften the `.luarc.json` MUST → G-003), **11** (acknowledge
> `sqcart/` + `docs/index.md` → G-005, G-006), and **12** (re-frame the AGENTS
> meta note → D-001/I-007) were applied. Items **3** (taxonomy home) and **4**
> (terminology-table home) were left open (B-002, I-001 handling noted in the
> `Applied` section). See `## Applied`.

The items below were the ones originally listed as requiring a decision. The
pass applied the above resolutions; the remaining open choices are only items 3
and 4.

1. **Spec is the truth vs repo is the truth (G-001, G-002).** Decide the
   direction of reconciliation for naming and presence: rename/provision the
   repo to match the spec (`third_party/`, `resources/`, six vendored deps,
   `cache/`), or amend the spec to match reality and move `resources/` to a
   later milestone. → **Resolved by directive: repo wins; spec amended.**

2. **§2.7.12 naming and scope (AUD-A-001 + AUD-B-001).** Rename the heading to
   "Generated project boundary", or keep "Ownership transition" and fix the
   outline; in either case delete the duplicated Independence/Leakage
   restatement. If any wording in the 2.7.12 versions is preferred over the
   §2.12 text, say which — otherwise §2.12 is canonical. → **Kept "Ownership
   transition"; §2.12 canonical; duplicates deleted.**

3. **Single home for the resource taxonomy (AUD-B-002).** Glossary, or `2.11.1`?
   Recommendation: glossary. → **Left open; taxonomy kept distributed (see
   Applied → Skipped).**

4. **Single home for the generation terminology (AUD-I-001).** Keep 2.7.0 and
   sync to the glossary, or replace with a link to `Spec glossary`?
   Recommendation: keep 2.7.0 minimal, link the glossary. → **Kept 2.7.0 minimal
   and linked the glossary as canonical.**

5. **§2.7 heading re-tiering (AUD-E-002, large diff).** Approve demoting all
   `## 2.7.N` to `### 2.7.N` and moving `Open questions`/`Related` below
   `Content` (AUD-D-002) in one file-wide rewrite of the note. → **Applied.**

6. **Bare-`§` policy (AUD-C-002).** Strict conversion to
   `[[note]] §N.N` everywhere, or amend `Spec conventions` to permit the
   short form in tables/same-line contexts? → **Amended the convention
   (weak-guideline approach).**

7. **§2.7.9 keyword (AUD-F-003).** Keep SHOULD and relax "normative" in
   D-027/`Spec status` (recommended), or promote to MUST. → **Kept SHOULD;
   D-027 label softened.**

8. **Q-25 (AUD-H-001 / AUD-C-003).** Approve the D-NNN entry "invariant
   register provides detection per invariant; I-10 gap documented in §2.16.3"
   and move Q-25 to resolved. → **Applied as D-028; Q-25 moved to resolved.**

9. **RFC 2119 clause (AUD-F-001).** Approve adding the conformance clause as a
   new §0 (already anticipated by the follow-up pass). → **Applied in `Spec
   conventions` (Normative language).**

10. **`.luarc.json` (AUD-G-003).** Provision a minimal root config, or drop
    the §1.2 MUST? → **Dropped the hard MUST; noted as recommended/absent.**

11. **`sqcart/` and `docs/index.md` (AUD-G-005, AUD-G-006).** Add to the
    §1.1/§1.8 listings, or record as intentionally outside Item 1? → **Added to
    §1.1/§1.8.**

12. **`Repository AGENTS` meta note (AUD-D-001 / AUD-I-007).** Retire it, or
    re-frame as "adopted — historical diff"? → **Re-framed as adopted.**

---

## How to approve this report

Mark each finding heading's status. The follow-up pass (`sqcart/prompt-edit.md`)
applies only findings marked `APPROVED`; for the items above, either write a
resolution after the arrow in the "requires a decision" section or mark the
finding `REJECTED`. The pass will append an `## Applied` section here recording
per-finding outcomes.

---

## Applied

**Guiding principle for this pass:** the repository is the ground truth; the
specification is a strong guideline that is weakly enforced. Where the spec and
the repo disagreed about the repo's present state, the spec was amended to match
reality (G-class). Where the repo is silent, the spec's design intent remains as
a recommendation.

### Applied
- **AUD-A-001** — fixed the §2.7.12 outline item to "Ownership transition" and
  removed the redundant `#### Ownership transition` sub-heading under the
  section of the same name.
- **AUD-A-002** — 2.5 outline item 2.5.4 now reads "Command-line interface
  control" (matches heading).
- **AUD-A-003** — 2.6 outline items 2.6.7, 2.6.9, 2.6.13 now match their
  headings ("…across the boundary", "…rules", "…and path abstraction").
- **AUD-B-001** — deleted `Independence` and `Leakage prohibition` from §2.7.12;
  the boundary guarantee now lives only in [[2.12 Generated-project boundary]]
  (verified to contain every deleted requirement: all 7 leakage items and the
  full independence list). Kept the ownership-transfer mechanism in §2.7.12.
- **AUD-C-002** — amended `Spec conventions` (Cross-references) to permit a bare
  `§N.M` reference when the target note is named on the same line or is
  unambiguous. Chosen over a 46-call-site rewrite because the specification is a
  weakly enforced guideline. The `[[note]] §N.N` form remains preferred for
  first-on-line cross-note references.
- **AUD-C-003** — resolved with AUD-H-001 (reciprocal link added in §2.16).
- **AUD-D-001** — re-framed `_meta/Repository AGENTS.md` from "proposed rewrite"
  to "adopted", pointing at the shipped `AGENTS.md`; kept the historical diff
  rationale. Also updated the `Specification Index` entry.
- **AUD-D-002** — moved `Open questions` and `Related` below `Content` in §2.7
  (now Purpose → Outline → Content → Open questions → Related, matching the
  convention used by every other note).
- **AUD-E-001** — removed the duplicated build-layer sentence in §2.7.3.
- **AUD-E-002** — demoted all §2.7 numbered subsections from `##` (H2) to `###`
  (H3), and their children to `####`/`#####`, matching the rest of the vault.
  Applied as a whole-file rewrite of the note (combined with D-002, B-001,
  E-001, A-001, I-001).
- **AUD-E-003** — demoted `## Part 1 — Errors` / `## Part 2 — Transactions` in
  §2.15 to `###` (one H2 per note).
- **AUD-F-001** — added an RFC 2119 conformance clause (and the weak-enforcement
  policy) to `Spec conventions` → Normative language. Placed in the vault's
  conventions note, which is the home the audit itself allowed for §0.
- **AUD-F-003** — softened D-027's "normative" label to "reference phase order";
  §2.7.9 keeps SHOULD.
- **AUD-G-001** — per the project-is-truth directive: §1.1, §1.6 and decision
  D-026 now use `third-party/` (repo naming) and mark the vendored set as
  intended-not-yet-provisioned (the directory currently holds only `.gitkeep`).
- **AUD-G-002** — §1.1 and §1.5 now mark `resources/` as planned but not yet
  provisioned; §1.5 status set to `draft`, namespaces kept as target design.
- **AUD-G-003** — §1.2 no longer lists `.luarc.json` as a hard MUST; noted as a
  recommended addition that is not yet present.
- **AUD-G-005** — `sqcart/` added to §1.1 (nested companion project) and to
  §1.2's allowance for additional root entries.
- **AUD-G-006** — `docs/index.md` added to §1.8's `docs/` division.
- **AUD-H-001** — resolved Q-25: added decision **D-028**, moved Q-25 to the
  resolved table in `Spec open questions`, removed the "Q-25" annotation in
  `Spec status`, and added the reciprocal note in §2.16.3.
- **AUD-I-001** — §2.7.0 Terminology now links `Spec glossary` as canonical and
  syncs the "Working directory" wording to the user-working-directory concept.
- **AUD-I-007** — no longer a separate duplicated copy; resolved by
  AUD-D-001.

### Skipped (retained; require an explicit canonical-home decision)
- **AUD-B-002** — the Template/Kit/Package/Asset taxonomy block is still written
  at multiple sites for local clarity; no contradiction today.
- **AUD-G-004** — `docs/programmer/` and `docs/developer/` remain a tracked gap
  (already acknowledged in `Spec status`); no edit beyond that acknowledgment.
- **AUD-I-002 … AUD-I-005** — verbatim cross-file sentences kept; they are
  consistent parallel restatements with no contradiction and removing them needs
  a per-pair canonical-home choice that was not given.

### Notes
- RFC 2119 clause was added inside `Spec conventions` rather than as a separate
  `§0` note; that note is the vault-level home the audit allowed (F-001).
- The root `AGENTS.md` previously stated "the specification is normative" and
  "the code and the spec disagreeing is worse than either being wrong alone,"
  which conflicted with the project-as-ground-truth principle. Following Jesse's
  follow-up direction, that section was rewritten to "Specification is a
  guideline (temporary stance)": the repository is ground truth and the spec is
  a weakly enforced guideline during the initial implementation, marked
  temporary. The `_meta/Repository AGENTS.md` note was updated to match.