#include <keshui/fluent2_theme.hpp>
#include <keshui/keshui.hpp>
#include "MainWindow.g.hpp"

#include <iostream>
#include <memory>

using namespace keshui;

int main() {
    // The gallery starts with the public Fluent 2 dark neutral ramp, then keeps
    // Kesh's orange accent as its brand color.
    fluent2::ApplyReferenceTheme(ThemeKind::Dark);

    XamlEventBinder binder;
    std::weak_ptr<Window> weakWindow;

    auto setStatus = [&weakWindow](std::string text) {
        if (auto window = weakWindow.lock()) {
            if (auto status = std::dynamic_pointer_cast<TextBlock>(window->FindName("StatusText"))) {
                status->SetText(std::move(text));
            }
        }
    };

    binder.Register("OnStandardButton", [&](UIElement&) {
        setStatus("Standard button · routed Click event from compiled XAML into C++ code-behind.");
    });
    binder.Register("OnAccentButton", [&](UIElement&) {
        setStatus("Primary action · Kesh orange is used as a Fluent 2 brand token, not as a surface color.");
    });
    binder.Register("OnHyperlink", [&](UIElement&) {
        setStatus("Subtle command · low-emphasis actions stay text-forward in the Fluent 2 hierarchy.");
    });
    binder.Register("OnToggleTheme", [&](UIElement&) {
        auto& tm = ThemeManager::Current();
        const auto next = tm.Theme() == ThemeKind::Dark ? ThemeKind::Light : ThemeKind::Dark;
        fluent2::ApplyReferenceTheme(next);
        setStatus(next == ThemeKind::Dark
            ? "Theme changed to Fluent 2 dark reference palette."
            : "Theme changed to Fluent 2 light reference palette.");
    });
    binder.Register("OnDialogDemo", [&](UIElement&) {
        setStatus("ContentDialog hosting is still a runtime foundation; this pass focuses on visual language and tokens.");
    });

    try {
        auto window = keshui_generated::BuildMainWindow(binder);
        weakWindow = window;
        std::cout << "KeshUI SDK Gallery v0.3 - Fluent 2 visual pass\n"
                  << "  Visual language: Fluent 2 public token model + Kesh brand accent\n"
                  << "  XAML: KXamlC typed IR -> generated C++\n"
                  << "  Resources: ResourceDictionary + StaticResource + ThemeResource\n"
                  << "  Styles: Setter + BasedOn compiled into BuildContext\n"
                  << "  Namescope: x:Name + ElementName binding foundation\n"
                  << "  Runtime: DependencyProperty + VSM + Measure/Arrange\n"
                  << "  Backend: X11 / Cairo / Pango\n";
        return RunX11(window);
    } catch (const std::exception& e) {
        std::cerr << "KeshUI gallery failed: " << e.what() << "\n";
        return 1;
    }
}
