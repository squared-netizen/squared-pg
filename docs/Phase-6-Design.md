---
title: Phase 6 Scene2D Design
tags:
  - architecture
  - scene2d
  - phase-6
---

# Phase 6 Scene2D Design

Phase 6 builds higher-level two-dimensional application structure on the
proven Phase 5 graphics foundation. The first slice is intentionally a small,
independent `squared_scene2d` module rather than a widget toolkit.

## First slice

- `Actor` owns parent-relative bounds, visibility, touchability, and the frame
  traversal entry point.
- `Group` owns children through `std::unique_ptr`, advances them in insertion
  order, and searches hits in reverse order so the last child is topmost.
- `Stage` owns the root group and defines the logical stage bounds.
- Coordinates are translation-only and hit coordinates are local to each
  actor.

Scene2D requires an exact Graphics2D package version even though this initial
hierarchy slice remains independently testable without a GPU. Rendering will
therefore extend the established module rather than create a competing scene
ownership model.

## Deferred contracts

Rendering callbacks, rotation and scaling, coordinate conversion, input event
capture and bubbling, focus, actions, layout, skins, and widgets require their
own observable contracts. They are deferred until each can be validated with
native behavior tests and an Android runtime proof. The first slice does not
publish placeholders for them.
