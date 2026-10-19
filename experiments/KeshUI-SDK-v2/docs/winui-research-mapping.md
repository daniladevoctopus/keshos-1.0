# WinUI research mapping

This file documents which public Microsoft UI Xaml areas influenced each KeshUI subsystem. It is intentionally explicit so future contributors can keep the clean boundary between architectural study and KeshUI's independent implementation.

| KeshUI subsystem | Microsoft UI Xaml area studied | KeshUI result |
|---|---|---|
| Dependency properties | `docs/property-system.md`, `dxaml/xcp/core/core/elements/DependencyProperty.cpp` | `DependencyProperty`, `DependencyObject`, effective value sources |
| Metadata/codegen idea | `dxaml/xcp/components/metadata`, `XCPTypesAutoGen` | static property registration + future schema table |
| Layout | `FrameworkElement`, `Canvas`, `StackPanel`, `Grid` sources | `Measure` / `Arrange`, panel overrides |
| Theme resources | `controls/dev/CommonStyles/*_themeresources.xaml` | semantic resource keys and theme palettes |
| Corner radii | `CommonStyles/CornerRadius_themeresources.xaml` | 4 px control / 8 px overlay token scale |
| Buttons | `CommonStyles/Button_themeresources.xaml` | Normal/PointerOver/Pressed/Disabled visuals, accent/subtle concept |
| VSM | `VisualStateManager` core / DirectUI layers | `VisualStateManager` runtime foundation |
| Routed events | `RoutedEventArgs`, pointer/key routed event types | routed input event objects and bubbling |
| Binding | `Microsoft.UI.Xaml.Data.cs`, BindingExpression layers | binding expression foundation and modes |
| XAML compiler | `docs/design-notes/xamlcompiler.md`, `src/XamlCompiler` | own KXamlC parser, validation and generated C++ |
| Resource lookup | Resource dictionaries / theme dictionaries | `ResourceDictionary`, `ThemeResource` references |
| Control templates | common style XAML templates | `ControlTemplate` factory abstraction |
| Navigation / shell-style controls | `controls/dev/NavigationView`, common styles | `NavigationView` control and Fluent layout language |
| Status surfaces | `controls/dev/InfoBar` | `InfoBar` severity semantics |

## Important differences

KeshUI does **not** implement:

- WinRT/COM activation;
- Microsoft namespaces or ABI;
- XBF binary compatibility;
- Windows App SDK packaging;
- Windows Composition / DirectComposition;
- Microsoft accessibility automation interfaces;
- WinUI's exact internal object layouts.

The purpose is to create a KeshOS-native UI platform with familiar, high-quality XAML concepts and an eventual Prism backend.

## Source license

The Microsoft repository is MIT licensed. The full notice required for substantial copied portions is preserved in `THIRD_PARTY_NOTICES.md`. KeshUI code in this prototype is written as an independent compact implementation rather than a direct wholesale copy of the large WinUI runtime.
