# miniz 3.1.2 — vendoring note

Upstream source, unmodified, with one addition.

## Addition: `miniz_export.h`

Upstream generates this header at configure time (its CMake and Meson builds
both produce it). The distributed source archive therefore does not contain it,
and `miniz.h` includes it unconditionally.

squared-pg must build from a clean checkout with no configure step and no
network (§1.13), and the hand-written `Makefile` has no way to generate it. The
file is a static-build export header with no configuration in it, so the
generated copy is committed rather than regenerated. It is byte-identical to the
one already vendored under `sqcart/third_party/miniz-3.1.2/`.

No other file in this directory is modified (§1.6).
