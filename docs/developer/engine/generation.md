# Generation

Plan building, substitution and ownership. Programmer counterpart:
[operations](../../programmer/engine/operations.md). Specification: §2.7.

## Phases

`Engine::Impl::resolve_set` runs §2.7.9 phases 2–6; `ProjectService::build_plan`
is phase 7; `TransactionService::apply` is phases 8–18.

Phases 2–6 mutate nothing. That is not a convention — `ProjectService` and the
four typed resolution services hold no reference to `FilesystemService`, so
mutating during resolution would not compile.

## Cross-validation (phase 6)

Runs after everything resolves, before a transaction can open. It detects:

| Check | Code |
|---|---|
| kit writes an integration area the template does not declare | `kit.integration_point.undeclared` |
| two kits write a `single`-arity area | `kit.integration_point.conflict` |
| two resources contribute the same workspace path | `kit.integration_point.conflict` |
| a kit declares `conflicts_with` another selected kit | `kit.compatibility.conflict` |
| the template requires a kit the workflow did not select | `template.kit.required` |
| a `generated` path falls inside the working directory | `template.ownership.invalid` |

Integration-point arity comes from the template's `integration_arity` map,
defaulting to `single`. Conservative on purpose: a false conflict is reported
and fixed, a missed one silently produces a workspace where two kits fought over
a file.

## Ownership classification

`detail::classify_path` matches a workspace path against the resource's
`ownership` globs, read from `consumers.squared_pg.ownership`.

**At most one list may match.** Two lists claiming one path is a manifest
defect: the author has said two contradictory things about one file, and only
they know which was meant. It is reported as a warning and resolved toward the
class that cannot destroy work — `generated` is the only class this engine
overwrites, so anything else wins. Two patterns in the *same* list is not a
conflict and is silent.

**An unmatched path takes `default`.** Absent `default` means `seeded`
(§2.7.10). Defaulting to `generated` would make the safe case the one you have
to remember, and forgetting would mean overwriting user work.

### What this replaced, and why

Until D-051 this used a specificity heuristic: `glob_specificity` scored
literal characters up and wildcards down, `**` more than `*`, and the highest
score won.

**That heuristic worked.** Every shipped template classified correctly under
it, and `user: ["**"]` behaved exactly as `default: "user"` does now. Nothing
was broken.

It was replaced because nobody can predict its outcome without knowing the
scoring table. Whether `mk/**` outranks `**/*.mk` is a question you have to
compute — it does, −2 against −4 — and ownership is a thing template authors
meet on their first day. A rule that fits in a sentence beats a rule that fits
in a lookup table, even when the lookup table is correct.

What went with it: wildcard carve-outs. Under specificity,
`generated: ["mk/**"]` with `user: ["mk/local.mk"]` worked, the literal beating
the wildcard. That is now an overlap warning, and the author writes
non-overlapping patterns instead — `generated: ["mk/squared_*.mk"]`. A real
cost, and a small one in practice.

### Two vocabularies

`ownership_class_from_manifest` maps the manifest's words onto the engine's,
and the mapping is not the identity.

A manifest author writes about *who owns the file*. The engine reasons about
*what it may do on a later pass*. So the manifest's `user` maps to
`OwnershipClass::seeded`, not to `::user`: a file the generator is
materialising cannot be `user`, because the generator is by definition writing
it. `OwnershipClass::user` is for files that appear afterwards by the author's
own hand, which no manifest can enumerate.

The list-based path always did this mapping, inline and unremarked. `default`
made it visible by getting it wrong — routing the field through
`ownership_class_from_string` produced `::user` for every materialised file
and broke two suites.

The manifest's `shared` also maps to `seeded`, because `shared` is what
§2.7.10 reserves as `merged` and explicitly does not implement in v1 (D-020).

## Substitution

`detail::substitute` expands `{{ name }}`. Three behaviours are worth knowing.

**An unknown parameter is an error, never an empty string** (§2.8.8). A silent
substitution failure produces a project that looks correctly generated and does
not build, which is the most expensive kind of wrong.

**A placeholder name must be an identifier.** Templates are mostly C++ source,
so `std::vector<int> v{{1, 2}}` will appear in one. Without the identifier
check, `1, 2` would be read as a parameter name and the file would fail to
substitute. Tested directly in `test_support.cpp`.

**Binary payloads are copied verbatim.** Detected by a NUL byte in the first 8
KiB. Substituting into a PNG would corrupt it in a way nothing downstream would
notice.

Paths are substituted too, so a template can emit `{{project_name}}.desktop`
without a special case.

## Effective parameters

Three layers, later winning: the template's declared defaults (§2.8.7), the
engine's built-ins derived from the request, then the workflow's values.

`detail::effective_parameters` is shared by validation and plan building, and
that sharing is load-bearing. Validating the raw workflow values while
substituting the effective ones would reject `project_name` as missing every
time — a parameter the engine was about to supply itself. That bug existed
during development and is the reason the function is not inlined into
`build_plan`.

Built-ins derive only from the request and the engine. No clock, no
environment — §2.7.13 needs byte-identical output from equivalent inputs, and a
timestamp would end that immediately.

## Step ordering

Steps are stable-sorted by phase, then directories before files, then path.
`std::stable_sort` rather than `sort` so contribution order survives among
equals, which is what makes two runs over the same inputs emit the same list.
