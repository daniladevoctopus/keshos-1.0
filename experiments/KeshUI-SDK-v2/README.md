# KeshUI SDK v2

KeshUI SDK v2 is an experimental native C++20 UI framework for Linux now and KeshOS/Prism later. It deliberately studies the architecture and design language of Microsoft WinUI 3 / Fluent controls while keeping an independent runtime, independent namespaces, and an independent renderer.

This branch is a research/prototype branch. It does not claim binary or source compatibility with WinUI 3.

## What is implemented

### WinUI-inspired runtime architecture

- `DependencyObject` / `DependencyProperty` with local, style, animated, inherited and default value precedence.
- `ResourceDictionary`, theme dictionaries and dynamic `ThemeResource` references.
- `Style`, `Setter`, `ControlTemplate` foundation.
- `VisualStateManager` and control states (`Normal`, `PointerOver`, `Pressed`, `Disabled`, checked states).
- `FrameworkElement` two-pass `Measure` / `Arrange` layout.
- routed input events with bubbling.
- `x:Name` namescope.
- lightweight binding foundation (`OneWay`, `TwoWay`, `OneTime`).
- Fluent-like design tokens: fills, strokes, text colors, corner radii, spacing, typography, control height, focus visuals, motion durations.
- light, dark and high-contrast theme palettes.

### 25 gallery features / controls

1. Button
2. Accent Button
3. Hyperlink Button
4. ToggleSwitch
5. CheckBox
6. RadioButton
7. Slider
8. ProgressBar
9. TextBox
10. SearchBox
11. ComboBox
12. ListView
13. TabView
14. NavigationView
15. InfoBar
16. Badge
17. Card
18. SettingsCard
19. Tooltip foundation
20. ContentDialog foundation
21. ContextMenu foundation
22. BreadcrumbBar
23. ScrollViewer
24. Expander
25. ProgressRing

Layout primitives additionally include `Grid`, `StackPanel`, `Canvas`, `Border` and `Separator`.

## KXamlC

`kxc` is KeshUI's own XAML compiler. It does not invoke Microsoft's compiler.

Pipeline:

```text
MainWindow.xaml
      |
      v
  KXamlC parser
      |
      +-- schema/tag validation
      +-- property validation foundation
      +-- x:Name namescope metadata
      +-- event handler metadata
      +-- markup extension preservation
      |
      v
MainWindow.g.hpp
      |
      v
C++20 application
```

The generated C++ creates the object tree directly, so the demo does not need to parse XAML at runtime.

Supported XAML concepts in this prototype include nested controls, `x:Name`, attached-property names such as `Grid.Column`, common scalar properties, `{ThemeResource ...}`, `{StaticResource ...}`, simple `{Binding ...}` preservation, and event handler names such as `Click="OnClick"`.

## Design source study

The implementation was designed after studying the open-source `microsoft/microsoft-ui-xaml` repository, especially:

- the property-system design notes and effective-value precedence;
- `FrameworkElement` / panel `MeasureOverride` + `ArrangeOverride` patterns;
- `ResourceDictionary` and theme dictionaries;
- `ControlTemplate` and `TemplateBinding` patterns;
- `VisualStateManager`;
- `Button_themeresources.xaml`, including Normal / PointerOver / Pressed / Disabled states and short brush transitions;
- WinUI XAML compiler architecture: schema/type resolution, DOM validation, code generation, generated page code and XBF concepts.

Microsoft UI Xaml is MIT licensed. See `THIRD_PARTY_NOTICES.md`.

## Arch / CachyOS build

```bash
sudo pacman -S --needed base-devel cmake ninja pkgconf libx11 cairo pango

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/keshui_gallery
```

The Linux preview backend uses X11 + Cairo + Pango. The core UI tree and XAML compiler are renderer-independent; the intended KeshOS backend is Prism surfaces + KeshUI's native text renderer.

## Useful commands

Validate XAML only:

```bash
./build/kxc --check demo/MainWindow.xaml
```

Compile XAML manually:

```bash
./build/kxc demo/MainWindow.xaml generated/MainWindow.g.hpp
```

## Current boundaries

This is intentionally a substantial prototype, not WinUI itself. The following are foundations rather than full WinUI-equivalent implementations: complex text editing, virtualization, accessibility/automation peers, full data binding reflection, `x:Bind`, full XAML namespace resolution, animation storyboards, composition effects, IME, touch/pen, and GPU rendering.

The next iteration should deepen these subsystems instead of adding random widgets.
