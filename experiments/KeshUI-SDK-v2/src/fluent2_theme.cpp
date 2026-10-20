#include <keshui/fluent2_theme.hpp>

namespace keshui::fluent2 {
namespace {

void ApplyDark(ThemePalette& p) {
    // Fluent 2 web dark neutral ramp, mapped to the compact KeshUI palette.
    // background1 = grey16 (#292929), background2 = grey12 (#1F1F1F),
    // background3 = grey8 (#141414), card = grey20 (#333333).
    p.applicationBackground = Color::FromHex("#1F1F1F");
    p.layerFill = Color::FromHex("#292929");
    p.layerFillAlt = Color::FromHex("#141414");
    p.cardFill = Color::FromHex("#333333");
    p.cardStroke = Color::FromHex("#525252");

    p.controlFillDefault = Color::FromHex("#292929");
    p.controlFillHover = Color::FromHex("#3D3D3D");
    p.controlFillPressed = Color::FromHex("#1F1F1F");
    p.controlFillDisabled = Color::FromHex("#141414");
    p.controlStroke = Color::FromHex("#666666");
    p.controlStrokeStrong = Color::FromHex("#ADADAD");

    p.textPrimary = Color::FromHex("#FFFFFF");
    p.textSecondary = Color::FromHex("#D6D6D6");
    p.textDisabled = Color::FromHex("#5C5C5C");

    // Kesh brand color intentionally remains orange rather than Microsoft's
    // default blue brand ramp. Interaction direction follows Fluent Windows:
    // lighter on hover, darker on press.
    p.accent = Color::FromHex("#F28A22");
    p.accentHover = Color::FromHex("#F6A14B");
    p.accentPressed = Color::FromHex("#C96A12");
    p.textOnAccent = Color::FromHex("#16100A");

    p.danger = Color::FromHex("#F1707B");
    p.success = Color::FromHex("#54B054");
    p.warning = Color::FromHex("#FCE100");

    // Fluent 2 dark focus uses a high-contrast outer stroke instead of
    // recoloring the control itself.
    p.focus = Color::FromHex("#FFFFFF");
}

void ApplyLight(ThemePalette& p) {
    p.applicationBackground = Color::FromHex("#FAFAFA");
    p.layerFill = Color::FromHex("#FFFFFF");
    p.layerFillAlt = Color::FromHex("#F5F5F5");
    p.cardFill = Color::FromHex("#FFFFFF");
    p.cardStroke = Color::FromHex("#D1D1D1");

    p.controlFillDefault = Color::FromHex("#FFFFFF");
    p.controlFillHover = Color::FromHex("#F5F5F5");
    p.controlFillPressed = Color::FromHex("#E0E0E0");
    p.controlFillDisabled = Color::FromHex("#F0F0F0");
    p.controlStroke = Color::FromHex("#D1D1D1");
    p.controlStrokeStrong = Color::FromHex("#616161");

    p.textPrimary = Color::FromHex("#242424");
    p.textSecondary = Color::FromHex("#424242");
    p.textDisabled = Color::FromHex("#BDBDBD");

    p.accent = Color::FromHex("#D96F0B");
    p.accentHover = Color::FromHex("#BD5E08");
    p.accentPressed = Color::FromHex("#8F4604");
    p.textOnAccent = Color::FromHex("#FFFFFF");

    p.danger = Color::FromHex("#BC2F32");
    p.success = Color::FromHex("#0E700E");
    p.warning = Color::FromHex("#835C00");
    p.focus = Color::FromHex("#000000");
}

void ApplyHighContrast(ThemePalette& p) {
    p.applicationBackground = Color::FromHex("#000000");
    p.layerFill = Color::FromHex("#000000");
    p.layerFillAlt = Color::FromHex("#000000");
    p.cardFill = Color::FromHex("#000000");
    p.cardStroke = Color::FromHex("#FFFFFF");
    p.controlFillDefault = Color::FromHex("#000000");
    p.controlFillHover = Color::FromHex("#1A1A1A");
    p.controlFillPressed = Color::FromHex("#FFFFFF");
    p.controlFillDisabled = Color::FromHex("#000000");
    p.controlStroke = Color::FromHex("#FFFFFF");
    p.controlStrokeStrong = Color::FromHex("#FFFFFF");
    p.textPrimary = Color::FromHex("#FFFFFF");
    p.textSecondary = Color::FromHex("#FFFFFF");
    p.textDisabled = Color::FromHex("#A3A3A3");
    p.accent = Color::FromHex("#FFFF00");
    p.accentHover = Color::FromHex("#FFFFFF");
    p.accentPressed = Color::FromHex("#FFFF00");
    p.textOnAccent = Color::FromHex("#000000");
    p.danger = Color::FromHex("#FF0000");
    p.success = Color::FromHex("#00FF00");
    p.warning = Color::FromHex("#FFFF00");
    p.focus = Color::FromHex("#FFFFFF");
}

} // namespace

void ApplyReferenceTheme(ThemeKind theme) {
    auto& manager = ThemeManager::Current();
    manager.SetTheme(theme);

    // ThemeManager intentionally exposes a read-only palette to ordinary UI
    // code. This compact visual-reference adapter lives inside the SDK and
    // patches the palette after the base theme is rebuilt. A later theme API
    // can make custom theme objects first-class without changing callers.
    auto& palette = const_cast<ThemePalette&>(manager.Palette());

    switch (theme) {
        case ThemeKind::Light:
            ApplyLight(palette);
            break;
        case ThemeKind::HighContrast:
            ApplyHighContrast(palette);
            break;
        case ThemeKind::Dark:
        default:
            ApplyDark(palette);
            break;
    }
}

} // namespace keshui::fluent2
