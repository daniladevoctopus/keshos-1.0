#include <keshui/keshui.hpp>
#include "MainWindow.g.hpp"

#include <iostream>
#include <memory>

using namespace keshui;

int main() {
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
        setStatus("Standard Button: Click routed from generated XAML into C++ code-behind.");
    });
    binder.Register("OnAccentButton", [&](UIElement&) {
        setStatus("Accent Button: semantic accent resources + visual states are active.");
    });
    binder.Register("OnHyperlink", [&](UIElement&) {
        setStatus("HyperlinkButton: lightweight command surface fired.");
    });
    binder.Register("OnToggleTheme", [&](UIElement&) {
        auto& tm = ThemeManager::Current();
        tm.SetTheme(tm.Theme() == ThemeKind::Dark ? ThemeKind::Light : ThemeKind::Dark);
        setStatus(tm.Theme() == ThemeKind::Dark
            ? "ThemeResource: switched to Dark theme."
            : "ThemeResource: switched to Light theme.");
    });
    binder.Register("OnChangeAccent", [&](UIElement&) {
        static int accent = 0;
        const Color accents[] = {
            Color::FromHex("#F28A22"), Color::FromHex("#7C8CFF"),
            Color::FromHex("#4CC38A"), Color::FromHex("#E35D9A")
        };
        accent = (accent + 1) % 4;
        ThemeManager::Current().SetAccent(accents[accent]);
        setStatus("Accent resource changed. Semantic accent controls resolve the new palette on redraw.");
    });
    binder.Register("OnDialogDemo", [&](UIElement&) {
        setStatus("ContentDialog overlay hosting is still a foundation. v0.3 focused on the XAML compiler/runtime rather than adding more widgets.");
    });

    try {
        auto window = keshui_generated::BuildMainWindow(binder);
        weakWindow = window;
        std::cout << "KeshUI SDK Gallery v0.3\n"
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
