# KeshUI SDK v0.3

KeshUI is an experimental native C++20 XAML UI framework for Linux now and KeshOS/Prism later. It studies the public architecture and design language of Microsoft WinUI 3 / Fluent while keeping an independent runtime, namespaces, renderer and compiler.

This is a research/prototype branch. It does **not** claim WinUI binary compatibility.

## v0.3 focus

v0.3 stops adding random widgets and deepens the framework underneath them.

The important change is that XAML is no longer just a tree-description convenience. KXamlC now builds a typed intermediate model and compiles page resources, styles, namescope metadata and markup extensions into generated C++.

```text
MainWindow.xaml
      |
      v
  KXamlC 0.3
      |
      +-- XML/XAML parser
      +-- schema/type validation
      +-- namescope validation
      +-- ResourceDictionary collection
      +-- Style / Setter / BasedOn analysis
      +-- markup-extension validation
      +-- compiler diagnostics (KX....)
      +-- typed IR dump
      |
      v
MainWindow.g.hpp
      |
      v
Xaml BuildContext
      |
      +-- StaticResource resolution
      +-- compiled Style application
      +-- x:Name registration
      +-- ElementName binding foundation
      +-- event binding
      |
      v
DependencyObject/UIElement tree
      |
      v
Measure -> Arrange -> Renderer
```

The gallery does not parse `MainWindow.xaml` at runtime.

## Runtime architecture

Implemented foundations:

- `DependencyObject` / `DependencyProperty`.
- effective-value precedence: animated > local > style > inherited > default.
- `ResourceDictionary` and semantic theme keys.
- dynamic framework `{ThemeResource ...}` resolution.
- compiled page `{StaticResource ...}` values.
- compiled `Style`, `Setter` and `BasedOn` chains.
- `FrameworkElement` two-pass `Measure` / `Arrange` layout.
- routed pointer/key events with bubbling.
- `x:Name` namescope.
- `{Binding ...}` and `{x:Bind ...}` parsing metadata.
- working OneTime-style `ElementName` binding for the typed accessors currently exposed by the prototype.
- `ControlTemplate` and `VisualStateManager` foundations.
- Fluent-like semantic fills, strokes, corner radii, typography, spacing, focus and motion tokens.
- Light, Dark and High Contrast palettes.

## KXamlC diagnostics

Examples of compiler checks:

- duplicate `x:Name` in one page namescope;
- malformed XML/XAML structure;
- unknown visual element type;
- unsupported markup extension;
- duplicate resource keys;
- invalid Style target type;
- missing Setter properties/values;
- Style `BasedOn` cycles.

Useful commands:

```bash
./build/kxc --check demo/MainWindow.xaml
./build/kxc --dump-ir demo/MainWindow.xaml
./build/kxc demo/MainWindow.xaml generated/MainWindow.g.hpp
```

## Gallery controls

The Linux gallery currently demonstrates at least 25 UI surfaces/features:

1. Button
2. AccentButton
3. HyperlinkButton
4. ToggleSwitch
5. CheckBox
6. RadioButton
7. Slider
8. ProgressBar
9. ProgressRing
10. TextBox
11. SearchBox
12. ComboBox
13. ListView
14. TabView
15. NavigationView
16. InfoBar
17. Badge
18. Card
19. SettingsCard
20. BreadcrumbBar
21. Expander
22. ScrollViewer
23. Tooltip foundation
24. ContentDialog foundation
25. ContextMenu foundation

Layout primitives include `Grid`, `StackPanel`, `Canvas`, `Border` and `Separator`.

## Public WinUI source study

The design work is grounded in the public `microsoft/microsoft-ui-xaml` repository, especially:

- `docs/property-system.md` and DependencyProperty implementation areas;
- `FrameworkElement`, `StackPanel`, `Grid` and `Canvas` Measure/Arrange patterns;
- `ResourceDictionary` and theme-dictionary patterns;
- common `*_themeresources.xaml` files;
- `Button_themeresources.xaml` with Normal / PointerOver / Pressed / Disabled states and short brush transitions;
- `VisualStateManager` core/DirectUI layers;
- RoutedEventArgs and pointer/key routed event types;
- Binding / BindingExpression model types;
- `docs/design-notes/xamlcompiler.md` and `src/XamlCompiler` build tasks/code generation concepts.

Microsoft UI Xaml is MIT licensed. See `THIRD_PARTY_NOTICES.md` and `docs/winui-research-mapping.md`.

## Arch / CachyOS build

```bash
sudo pacman -S --needed base-devel cmake ninja pkgconf libx11 cairo pango

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/keshui_gallery
```

The Linux preview backend is X11 + Cairo + Pango. The UI object model, compiler and page construction layer are renderer-independent. The intended KeshOS backend is Prism surfaces plus KeshUI native rendering.

## Honest boundaries

Still foundation/prototype rather than WinUI-equivalent:

- full live data-binding reflection and source notifications;
- complete `x:Bind` code generation;
- full ControlTemplate materialization;
- Storyboard/transition execution;
- virtualization;
- IME/touch/pen;
- accessibility/automation peers;
- composition effects and GPU rendering;
- WinRT/COM ABI and Windows App SDK compatibility.

Those are the next deepening areas. The goal is a real KeshOS-native UI platform, not a skin over a collection of hand-drawn widgets.
