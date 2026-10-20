# Fluent 2 visual pass

This pass replaces the earlier custom blue-black showcase styling with a visual system based on Microsoft's public Fluent 2 guidance and the open-source Fluent UI token model.

## What changed

- Neutral surfaces now follow Fluent 2 dark layering: page, layer, card and control backgrounds are distinct neutrals rather than blue-tinted blacks.
- Kesh orange is treated as a **brand token**, not as a general surface color.
- The gallery uses one primary action at a time and keeps secondary commands neutral or subtle.
- Navigation uses a compact neutral rail, a subtle selected background and a small brand indicator.
- Spacing follows the Fluent 2 4 px rhythm, with 8 / 12 / 16 / 24 / 32 px as the main steps.
- Typography follows the Fluent 2 hierarchy: 12 px captions, 14 px body, 20 px section titles, 28–32 px page titles, semibold for emphasis.
- Controls use small radii. 4 px is the default control radius, while 8 px is reserved for larger cards and surfaces.
- Dark focus uses a high-contrast light stroke instead of recoloring the control.

## Public references studied

- `fluent2.microsoft.design/design-tokens`
- `fluent2.microsoft.design/color`
- `fluent2.microsoft.design/color-tokens`
- `fluent2.microsoft.design/typography`
- `fluent2.microsoft.design/layout`
- `fluent2.microsoft.design/elevation`
- `fluent2.microsoft.design/iconography`
- Microsoft Fluent UI open-source token sources in `microsoft/fluentui`

## Dark neutral mapping used by KeshUI

The compact KeshUI theme palette maps the public Fluent 2 dark token hierarchy to its existing fields:

| KeshUI field | Fluent 2 role | Value |
|---|---|---|
| `applicationBackground` | neutral background 2 | `#1F1F1F` |
| `layerFill` | neutral background 1 | `#292929` |
| `layerFillAlt` | neutral background 3 | `#141414` |
| `cardFill` | neutral card background | `#333333` |
| `cardStroke` | neutral stroke 2 | `#525252` |
| `textPrimary` | neutral foreground 1 | `#FFFFFF` |
| `textSecondary` | neutral foreground 2 | `#D6D6D6` |
| `controlStroke` | neutral stroke 1 | `#666666` |

Kesh deliberately keeps its own orange brand accent (`#F28A22`) rather than adopting Microsoft's default blue brand ramp.

## Important boundary

This is a **visual/token reference pass**, not a claim that KeshUI is Fluent UI or WinUI. The runtime, XAML compiler, renderer, namespaces and ABI remain independent KeshUI implementations.
