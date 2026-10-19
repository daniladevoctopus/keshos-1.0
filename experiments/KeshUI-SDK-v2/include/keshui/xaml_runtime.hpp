#pragma once

#include <keshui/keshui.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace keshui::xaml {

enum class MarkupKind {
    Literal,
    StaticResource,
    ThemeResource,
    Binding,
    XBind,
    TemplateBinding,
    Unknown
};

struct MarkupExtension {
    MarkupKind kind{MarkupKind::Literal};
    std::string head;
    std::unordered_map<std::string, std::string> arguments;
    std::string raw;
};

struct ResourceSpec {
    std::string key;
    std::string value;
    std::optional<ThemeKind> theme;
};

struct StyleSetterSpec {
    std::string property;
    std::string value;
};

struct StyleSpec {
    std::string key;
    std::string targetType;
    std::string basedOn;
    std::vector<StyleSetterSpec> setters;
};

struct BindingSpec {
    std::string path;
    std::string elementName;
    BindingMode mode{BindingMode::OneWay};
    std::string converter;
    std::string fallbackValue;
};

struct Diagnostic {
    enum class Severity { Info, Warning, Error };
    Severity severity{Severity::Info};
    std::string code;
    std::string message;
};

MarkupExtension ParseMarkupExtension(std::string_view value);
BindingSpec ParseBinding(const MarkupExtension& extension);

class BuildContext {
public:
    explicit BuildContext(XamlEventBinder& binder) : binder_(&binder) {}

    void AddResource(ResourceSpec resource);
    void AddStyle(StyleSpec style);

    const ResourceSpec* FindResource(std::string_view key) const;
    const StyleSpec* FindStyle(std::string_view key) const;

    void Apply(const std::shared_ptr<UIElement>& element,
               std::string_view property,
               std::string_view value);

    void AddChild(const std::shared_ptr<UIElement>& parent,
                  const std::shared_ptr<UIElement>& child);

    void RegisterName(const std::shared_ptr<UIElement>& element);
    std::shared_ptr<UIElement> FindName(std::string_view name) const;

    void Finalize(const std::shared_ptr<UIElement>& root);

    const std::vector<Diagnostic>& Diagnostics() const { return diagnostics_; }
    bool HasErrors() const;

private:
    std::string ResolveStaticValue(std::string_view value, int depth = 0) const;
    std::string ResolveThemeValue(std::string_view key, int depth = 0) const;
    void ApplyStyle(const std::shared_ptr<UIElement>& element,
                    std::string_view key,
                    int depth = 0);
    void RecordBinding(const std::shared_ptr<UIElement>& element,
                       std::string_view property,
                       const MarkupExtension& extension);
    void AddDiagnostic(Diagnostic::Severity severity,
                       std::string code,
                       std::string message);

    struct PendingBinding {
        std::weak_ptr<UIElement> target;
        std::string property;
        BindingSpec binding;
    };

    XamlEventBinder* binder_{};
    std::unordered_map<std::string, std::vector<ResourceSpec>> resources_;
    std::unordered_map<std::string, StyleSpec> styles_;
    std::unordered_map<std::string, std::weak_ptr<UIElement>> names_;
    std::vector<PendingBinding> bindings_;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace keshui::xaml
