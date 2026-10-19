# KeshUI v2 architecture

KeshUI v2 deliberately copies *architectural ideas*, not the WinRT ABI. The public API and runtime are independent C++20.

## 1. Object/property layer

```text
DependencyObject
  ├─ local value store
  ├─ style value store
  ├─ animated value store
  └─ inheritance parent
```

Effective values follow the same useful ordering described by WinUI's public property-system design notes:

```text
animated
  > local
  > style
  > inherited/default
```

This makes styling, animations, binding and theming services able to share one property model rather than each control inventing ad-hoc state.

## 2. Visual tree

```text
DependencyObject
  └─ UIElement
      └─ FrameworkElement
          ├─ Panel
          │   ├─ Grid
          │   ├─ StackPanel
          │   └─ Canvas
          └─ Control
              ├─ ContentControl
              ├─ Button
              ├─ TextBox
              ├─ Slider
              └─ ...
```

The tree owns layout, rendering order, hit testing, routed input and inheritance relationships.

## 3. Layout

Layout is a two-pass system:

1. `Measure(availableSize)` calculates `DesiredSize`.
2. `Arrange(finalRect)` assigns the final layout slot.

Controls and panels override `MeasureOverride` / `ArrangeOverride`, mirroring the core pattern used throughout WinUI's `FrameworkElement`, `Canvas`, `StackPanel` and custom controls.

## 4. Resources and themes

KeshUI uses semantic resource keys rather than hard-coding RGB values into controls.

Examples:

- `TextFillColorPrimaryBrush`
- `ControlFillColorDefaultBrush`
- `ControlFillColorSecondaryBrush`
- `ControlStrokeColorDefaultBrush`
- `CardBackgroundFillColorDefaultBrush`
- `AccentFillColorDefaultBrush`

`ThemeResource` is intentionally resolved against the active theme at access/render time, so theme or accent changes can propagate without rebuilding the object tree.

The base control corner radius is 4 px and overlay radius is 8 px, matching the useful scale exposed by WinUI theme resources while keeping KeshUI's own palette.

## 5. Styles/templates

The runtime provides:

- `Style`
- `Setter`
- `ControlTemplate`
- style inheritance (`BasedOn` foundation)
- `VisualStateManager`

Default C++ controls currently render directly for compactness, while the template object model is already present so control chrome can move into XAML templates in the next pass.

## 6. Visual states

The baseline control state group is:

```text
CommonStates
├─ Normal
├─ PointerOver
├─ Pressed
└─ Disabled
```

This follows the structure seen in WinUI common styles. KeshUI design tokens also include a fast transition duration of 83 ms, matching the short brush-transition cadence used by current WinUI button resources.

## 7. Routed input

Input is hit-tested at the visual tree, then routed through the element hierarchy. The current prototype implements bubbling and leaves tunnel/preview events as a straightforward extension.

The Linux backend translates X11 pointer/key events into renderer-independent `PointerEventArgs` and `KeyEventArgs`. A Prism backend can feed equivalent events without changing controls.

## 8. XAML compiler

KXamlC is a standalone executable rather than a runtime XML loader.

```text
XAML source
   ↓
parse / schema validate
   ↓
intermediate object tree
   ↓
C++ codegen
   ↓
generated .g.hpp
   ↓
normal C++ compilation
```

The direction is inspired by WinUI's compiler architecture: explicit schema/type knowledge, validation before code generation, generated page code and a compiled representation. KeshUI does not use WinUI's XBF format.

## 9. Rendering backends

The runtime draws through `Renderer`.

Current:

```text
KeshUI → Renderer → Cairo/Pango → X11
```

Target:

```text
KeshUI → Renderer → KeshOS Prism Surface → dwm.exe
```

This is the important boundary: XAML, property system, controls and layout must not know whether pixels eventually reach X11 or KeshOS Prism.

## 10. Deepening roadmap

The next architectural work should be depth rather than widget count:

1. compiled `x:Bind` and typed binding metadata;
2. full namescopes and template namescopes;
3. `TemplateBinding`;
4. XAML-defined `Style` and `ControlTemplate` parsing;
5. VSM setters/storyboards from XAML;
6. overlay host for Flyout/Tooltip/ContextMenu/ContentDialog;
7. focus traversal and keyboard navigation;
8. accessibility/automation peer layer;
9. virtualization for `ListView`;
10. Prism backend.
