#include <keshui/keshui.hpp>
#include <keshui/xaml_runtime.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

using namespace keshui;
using namespace keshui::xaml;

namespace {

bool Near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

bool SameColor(Color a, Color b) {
    return Near(a.r, b.r) && Near(a.g, b.g) && Near(a.b, b.b) && Near(a.a, b.a);
}

int Fail(const std::string& message) {
    std::cerr << "xaml_runtime_tests: " << message << "\n";
    return 1;
}

} // namespace

int main() {
    XamlEventBinder binder;

    // Markup-extension parser: the compiler and runtime use the same conceptual
    // vocabulary even though KXamlC performs its own front-end validation.
    {
        const auto binding = ParseMarkupExtension(
            "{Binding ElementName=SourceBox, Path=Text, Mode=OneTime, FallbackValue=none}");
        if (binding.kind != MarkupKind::Binding) return Fail("Binding kind not recognized");
        const auto spec = ParseBinding(binding);
        if (spec.elementName != "SourceBox" || spec.path != "Text" ||
            spec.mode != BindingMode::OneTime || spec.fallbackValue != "none") {
            return Fail("Binding arguments parsed incorrectly");
        }

        const auto theme = ParseMarkupExtension("{ThemeResource PageBrush}");
        if (theme.kind != MarkupKind::ThemeResource || theme.head != "PageBrush") {
            return Fail("ThemeResource parsed incorrectly");
        }
    }

    // Page ResourceDictionary semantics: theme-qualified entries beat the
    // unqualified fallback for the currently active theme.
    {
        ThemeManager::Current().SetTheme(ThemeKind::Dark);
        BuildContext context(binder);
        context.AddResource({"PageBrush", "#101010", std::nullopt});
        context.AddResource({"PageBrush", "#203040", ThemeKind::Dark});
        context.AddResource({"PageBrush", "#F4F4F4", ThemeKind::Light});

        auto button = std::make_shared<Button>();
        context.Apply(button, "Background", "{ThemeResource PageBrush}");
        const auto actual = button->Get<Color>(Control::BackgroundProperty());
        if (!SameColor(actual, Color::FromHex("#203040"))) {
            return Fail("Dark ThemeResource did not select dark page resource");
        }
    }

    {
        ThemeManager::Current().SetTheme(ThemeKind::Light);
        BuildContext context(binder);
        context.AddResource({"PageBrush", "#101010", std::nullopt});
        context.AddResource({"PageBrush", "#203040", ThemeKind::Dark});
        context.AddResource({"PageBrush", "#F4F4F4", ThemeKind::Light});

        auto button = std::make_shared<Button>();
        context.Apply(button, "Background", "{ThemeResource PageBrush}");
        const auto actual = button->Get<Color>(Control::BackgroundProperty());
        if (!SameColor(actual, Color::FromHex("#F4F4F4"))) {
            return Fail("Light ThemeResource did not select light page resource");
        }
    }

    // StaticResource chains and Style.BasedOn are resolved by the compiled page
    // BuildContext before ordinary local properties are applied.
    {
        ThemeManager::Current().SetTheme(ThemeKind::Dark);
        BuildContext context(binder);
        context.AddResource({"BaseBrush", "#334455", std::nullopt});
        context.AddResource({"AliasBrush", "{StaticResource BaseBrush}", std::nullopt});

        StyleSpec base;
        base.key = "BaseButtonStyle";
        base.targetType = "Button";
        base.setters.push_back({"Background", "{StaticResource AliasBrush}"});
        context.AddStyle(std::move(base));

        StyleSpec derived;
        derived.key = "DerivedButtonStyle";
        derived.targetType = "Button";
        derived.basedOn = "BaseButtonStyle";
        derived.setters.push_back({"CornerRadius", "7"});
        context.AddStyle(std::move(derived));

        auto button = std::make_shared<Button>();
        context.Apply(button, "Style", "{StaticResource DerivedButtonStyle}");
        if (!SameColor(button->Get<Color>(Control::BackgroundProperty()), Color::FromHex("#334455"))) {
            return Fail("Style/StaticResource chain did not apply Background");
        }
        if (!Near(button->Get<float>(Control::CornerRadiusProperty()), 7.0f)) {
            return Fail("Derived style setter did not apply CornerRadius");
        }
    }

    // x:Name namescope plus a compiled ElementName binding. This is deliberately
    // OneTime in v0.3; live source notifications are a later binding-engine stage.
    {
        BuildContext context(binder);
        auto root = std::make_shared<StackPanel>();
        auto source = std::make_shared<TextBox>();
        auto target = std::make_shared<TextBox>();

        context.Apply(source, "x:Name", "SourceBox");
        context.Apply(source, "Text", "KeshUI binding works");
        context.Apply(target, "Text",
                      "{Binding ElementName=SourceBox, Path=Text, Mode=OneTime, FallbackValue=failed}");
        context.AddChild(root, source);
        context.AddChild(root, target);
        context.Finalize(root);

        if (target->Text() != "KeshUI binding works") {
            return Fail("ElementName OneTime binding did not transfer Text");
        }
        if (context.HasErrors()) return Fail("BuildContext produced unexpected error diagnostics");
    }

    ThemeManager::Current().SetTheme(ThemeKind::Dark);
    std::cout << "xaml_runtime_tests: PASS\n";
    return 0;
}
