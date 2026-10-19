# KeshUI design system v0.2

The visual language is intentionally Fluent-adjacent without trying to impersonate Windows. The runtime uses semantic tokens so the same control tree can render as KeshOS Dark, Light, High Contrast, or a future custom theme.

## Core dimensions

1. Control corner radius: 4 px
2. Overlay corner radius: 8 px
3. Standard control height: 32 px
4. Compact control height: 28 px
5. Touch control height: 40 px
6. Spacing 2
7. Spacing 4
8. Spacing 8
9. Spacing 12
10. Spacing 16
11. Spacing 24
12. Spacing 32
13. Caption text: 12 px
14. Body text: 14 px
15. Subtitle text: 16 px
16. Title text: 20 px
17. Display text: 28 px
18. Focus stroke: 2 px
19. Low elevation
20. Medium elevation
21. High elevation
22. Fast motion: 83 ms
23. Normal motion: 167 ms
24. Slow motion: 250 ms

## Semantic color tokens

- `ApplicationBackground`
- `LayerFill`
- `LayerFillAlt`
- `CardFill`
- `CardStroke`
- `ControlFillColorDefaultBrush`
- `ControlFillColorSecondaryBrush`
- `ControlFillColorTertiaryBrush`
- `ControlFillColorDisabledBrush`
- `ControlStrokeColorDefaultBrush`
- `TextFillColorPrimaryBrush`
- `TextFillColorSecondaryBrush`
- `TextFillColorDisabledBrush`
- `AccentFillColorDefaultBrush`
- `AccentFillColorSecondaryBrush`
- `AccentFillColorTertiaryBrush`
- `TextOnAccentFillColorPrimaryBrush`
- `Danger`
- `Success`
- `Warning`

## Control-state contract

Interactive controls should expose a common baseline state group:

```text
Normal
PointerOver
Pressed
Disabled
```

Selectable controls extend it with checked/selected states. State changes should resolve semantic resources rather than embedding raw colors in the control implementation.

## Typography

Linux preview preference:

```text
Inter
Noto Sans
system sans-serif fallback
```

KeshOS target:

```text
Inter for UI
JetBrains Mono for terminal/code
```

## Theme philosophy

KeshOS Dark is graphite-first with a restrained orange accent. Avoid neon, cyberpunk glow, giant corner radii, excessive transparency, and “everything is a floating pill” styling.

The intended character is a modern classic desktop: strong hierarchy, crisp surfaces, soft anti-aliased edges, practical density, subtle depth and obvious interaction states.
