#include <keshui/xaml_runtime.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace keshui::xaml {
namespace {

std::string Trim(std::string_view value) {
    std::size_t a = 0;
    std::size_t b = value.size();
    while (a < b && std::isspace(static_cast<unsigned char>(value[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(value[b - 1]))) --b;
    return std::string(value.substr(a, b - a));
}

std::string Lower(std::string_view value) {
    std::string out(value);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::vector<std::string> SplitArguments(std::string_view input) {
    std::vector<std::string> out;
    std::size_t start = 0;
    bool quoted = false;
    char quote = 0;
    int braces = 0;
    for (std::size_t i = 0; i <= input.size(); ++i) {
        const char c = i < input.size() ? input[i] : ',';
        if (quoted) {
            if (c == quote) quoted = false;
            continue;
        }
        if (c == '\'' || c == '"') {
            quoted = true;
            quote = c;
            continue;
        }
        if (c == '{') ++braces;
        else if (c == '}') --braces;
        else if (c == ',' && braces == 0) {
            out.push_back(Trim(input.substr(start, i - start)));
            start = i + 1;
        }
    }
    return out;
}

std::optional<std::string> ReadKnownProperty(UIElement& element, std::string_view path) {
    if (path == "Text") {
        if (auto* box = dynamic_cast<TextBox*>(&element)) return box->Text();
    }
    if (path == "Content") {
        if (auto* content = dynamic_cast<ContentControl*>(&element)) return content->Content();
    }
    if (path == "Title") {
        if (auto* window = dynamic_cast<Window*>(&element)) return window->Title();
    }
    if (path == "IsOn") {
        if (auto* toggle = dynamic_cast<ToggleSwitch*>(&element)) return toggle->IsOn() ? "True" : "False";
    }
    if (path == "IsChecked") {
        if (auto* check = dynamic_cast<CheckBox*>(&element)) return check->IsChecked() ? "True" : "False";
    }
    if (path == "Value") {
        if (auto* slider = dynamic_cast<Slider*>(&element)) {
            std::ostringstream out;
            out << slider->Value();
            return out.str();
        }
    }
    return std::nullopt;
}

std::string ResourceKeyFromReference(std::string_view value) {
    const auto extension = ParseMarkupExtension(value);
    if (extension.kind == MarkupKind::StaticResource || extension.kind == MarkupKind::ThemeResource) {
        return extension.head;
    }
    return {};
}

} // namespace

MarkupExtension ParseMarkupExtension(std::string_view value) {
    MarkupExtension result;
    result.raw = std::string(value);
    const std::string text = Trim(value);
    if (text.size() < 3 || text.front() != '{' || text.back() != '}') {
        result.kind = MarkupKind::Literal;
        result.head = text;
        return result;
    }

    const std::string body = Trim(std::string_view(text).substr(1, text.size() - 2));
    const auto firstSpace = body.find_first_of(" \t,");
    const std::string name = firstSpace == std::string::npos ? body : body.substr(0, firstSpace);
    const std::string rest = firstSpace == std::string::npos ? std::string{} : Trim(std::string_view(body).substr(firstSpace + 1));
    const std::string lowered = Lower(name);

    if (lowered == "staticresource") result.kind = MarkupKind::StaticResource;
    else if (lowered == "themeresource") result.kind = MarkupKind::ThemeResource;
    else if (lowered == "binding") result.kind = MarkupKind::Binding;
    else if (lowered == "x:bind") result.kind = MarkupKind::XBind;
    else if (lowered == "templatebinding") result.kind = MarkupKind::TemplateBinding;
    else result.kind = MarkupKind::Unknown;

    if (result.kind == MarkupKind::StaticResource || result.kind == MarkupKind::ThemeResource ||
        result.kind == MarkupKind::TemplateBinding) {
        result.head = Trim(rest);
        return result;
    }

    for (const auto& item : SplitArguments(rest)) {
        if (item.empty()) continue;
        const auto eq = item.find('=');
        if (eq == std::string::npos) {
            if (result.head.empty()) result.head = Trim(item);
            continue;
        }
        auto key = Trim(std::string_view(item).substr(0, eq));
        auto argValue = Trim(std::string_view(item).substr(eq + 1));
        if (argValue.size() >= 2 && ((argValue.front() == '"' && argValue.back() == '"') ||
                                    (argValue.front() == '\'' && argValue.back() == '\''))) {
            argValue = argValue.substr(1, argValue.size() - 2);
        }
        result.arguments[std::move(key)] = std::move(argValue);
    }
    return result;
}

BindingSpec ParseBinding(const MarkupExtension& extension) {
    BindingSpec spec;
    spec.path = extension.head;
    if (const auto it = extension.arguments.find("Path"); it != extension.arguments.end()) spec.path = it->second;
    if (const auto it = extension.arguments.find("ElementName"); it != extension.arguments.end()) spec.elementName = it->second;
    if (const auto it = extension.arguments.find("Converter"); it != extension.arguments.end()) spec.converter = it->second;
    if (const auto it = extension.arguments.find("FallbackValue"); it != extension.arguments.end()) spec.fallbackValue = it->second;
    if (const auto it = extension.arguments.find("Mode"); it != extension.arguments.end()) {
        const auto mode = Lower(it->second);
        if (mode == "twoway") spec.mode = BindingMode::TwoWay;
        else if (mode == "onetime") spec.mode = BindingMode::OneTime;
        else spec.mode = BindingMode::OneWay;
    }
    if (extension.kind == MarkupKind::XBind && !extension.arguments.contains("Mode")) {
        spec.mode = BindingMode::OneTime;
    }
    return spec;
}

void BuildContext::AddResource(ResourceSpec resource) {
    if (resource.key.empty()) {
        AddDiagnostic(Diagnostic::Severity::Error, "KXR1001", "A XAML resource must have x:Key.");
        return;
    }
    resources_[resource.key] = std::move(resource);
}

void BuildContext::AddStyle(StyleSpec style) {
    if (style.key.empty()) {
        AddDiagnostic(Diagnostic::Severity::Error, "KXR1002", "A compiled Style resource must have x:Key.");
        return;
    }
    styles_[style.key] = std::move(style);
}

const ResourceSpec* BuildContext::FindResource(std::string_view key) const {
    const auto it = resources_.find(std::string(key));
    return it == resources_.end() ? nullptr : &it->second;
}

const StyleSpec* BuildContext::FindStyle(std::string_view key) const {
    const auto it = styles_.find(std::string(key));
    return it == styles_.end() ? nullptr : &it->second;
}

std::string BuildContext::ResolveStaticValue(std::string_view value, int depth) const {
    if (depth > 16) return std::string(value);
    const auto extension = ParseMarkupExtension(value);
    if (extension.kind != MarkupKind::StaticResource) return std::string(value);
    if (const auto* resource = FindResource(extension.head)) {
        return ResolveStaticValue(resource->value, depth + 1);
    }
    return std::string(value);
}

void BuildContext::ApplyStyle(const std::shared_ptr<UIElement>& element,
                              std::string_view key,
                              int depth) {
    if (!element) return;
    if (depth > 16) {
        AddDiagnostic(Diagnostic::Severity::Error, "KXR2001", "Style BasedOn chain exceeded 16 entries (possible cycle)." );
        return;
    }
    const auto* style = FindStyle(key);
    if (!style) {
        AddDiagnostic(Diagnostic::Severity::Warning, "KXR2002", "Static Style resource '" + std::string(key) + "' was not found." );
        return;
    }
    if (!style->basedOn.empty()) ApplyStyle(element, style->basedOn, depth + 1);
    for (const auto& setter : style->setters) {
        const auto resolved = ResolveStaticValue(setter.value);
        ApplyXamlAttribute(element, setter.property, resolved, binder_);
    }
}

void BuildContext::RecordBinding(const std::shared_ptr<UIElement>& element,
                                 std::string_view property,
                                 const MarkupExtension& extension) {
    PendingBinding pending;
    pending.target = element;
    pending.property = std::string(property);
    pending.binding = ParseBinding(extension);
    bindings_.push_back(std::move(pending));
    if (element) element->SetAttached("xaml:Binding:" + std::string(property), extension.raw);
}

void BuildContext::Apply(const std::shared_ptr<UIElement>& element,
                         std::string_view property,
                         std::string_view value) {
    if (!element) return;
    if (property.rfind("xmlns", 0) == 0 || property == "x:Class" || property == "x:Key") return;

    if (property == "Style") {
        const auto extension = ParseMarkupExtension(value);
        if (extension.kind == MarkupKind::StaticResource) {
            ApplyStyle(element, extension.head);
        } else {
            AddDiagnostic(Diagnostic::Severity::Warning, "KXR2003", "Style currently expects {StaticResource ...}." );
        }
        return;
    }

    const auto extension = ParseMarkupExtension(value);
    switch (extension.kind) {
        case MarkupKind::StaticResource: {
            const auto resolved = ResolveStaticValue(value);
            if (resolved == value) {
                AddDiagnostic(Diagnostic::Severity::Warning, "KXR2004", "StaticResource '" + extension.head + "' was not found in the compiled page dictionary." );
            }
            ApplyXamlAttribute(element, property, resolved, binder_);
            break;
        }
        case MarkupKind::Binding:
        case MarkupKind::XBind:
            RecordBinding(element, property, extension);
            break;
        case MarkupKind::TemplateBinding:
            element->SetAttached("xaml:TemplateBinding:" + std::string(property), extension.head);
            break;
        case MarkupKind::ThemeResource:
            // Framework ThemeResource keys are intentionally preserved. KeshUI core
            // resolves those dynamically on redraw through ThemeManager.
            ApplyXamlAttribute(element, property, value, binder_);
            break;
        case MarkupKind::Unknown:
            AddDiagnostic(Diagnostic::Severity::Warning, "KXR2005", "Unknown markup extension in '" + std::string(value) + "'." );
            ApplyXamlAttribute(element, property, value, binder_);
            break;
        case MarkupKind::Literal:
        default:
            ApplyXamlAttribute(element, property, value, binder_);
            break;
    }

    if (property == "x:Name" || property == "Name") RegisterName(element);
}

void BuildContext::AddChild(const std::shared_ptr<UIElement>& parent,
                            const std::shared_ptr<UIElement>& child) {
    if (parent && child) parent->AddChild(child);
}

void BuildContext::RegisterName(const std::shared_ptr<UIElement>& element) {
    if (!element || element->Name().empty()) return;
    const auto it = names_.find(element->Name());
    if (it != names_.end() && !it->second.expired()) {
        AddDiagnostic(Diagnostic::Severity::Error, "KXR1003", "Duplicate x:Name '" + element->Name() + "'." );
        return;
    }
    names_[element->Name()] = element;
}

std::shared_ptr<UIElement> BuildContext::FindName(std::string_view name) const {
    const auto it = names_.find(std::string(name));
    return it == names_.end() ? nullptr : it->second.lock();
}

void BuildContext::Finalize(const std::shared_ptr<UIElement>& root) {
    if (root) {
        // Register any names that were not encountered through generated attributes.
        // The tree itself remains the authoritative runtime namescope via UIElement::FindName.
        for (const auto& [name, weak] : names_) (void)name, (void)weak;
    }

    for (auto& pending : bindings_) {
        auto target = pending.target.lock();
        if (!target) continue;
        auto& binding = pending.binding;
        if (binding.elementName.empty()) {
            AddDiagnostic(Diagnostic::Severity::Info, "KXR3001",
                          "Binding on '" + pending.property + "' was compiled as metadata; DataContext reflection is not implemented yet." );
            continue;
        }
        auto source = FindName(binding.elementName);
        if (!source && root) source = root->FindName(binding.elementName);
        if (!source) {
            if (!binding.fallbackValue.empty()) ApplyXamlAttribute(target, pending.property, binding.fallbackValue, binder_);
            else AddDiagnostic(Diagnostic::Severity::Warning, "KXR3002", "Binding ElementName '" + binding.elementName + "' was not found." );
            continue;
        }
        const auto value = ReadKnownProperty(*source, binding.path);
        if (!value) {
            if (!binding.fallbackValue.empty()) ApplyXamlAttribute(target, pending.property, binding.fallbackValue, binder_);
            else AddDiagnostic(Diagnostic::Severity::Info, "KXR3003",
                               "Binding path '" + binding.path + "' is preserved but has no typed accessor in v0.3." );
            continue;
        }
        ApplyXamlAttribute(target, pending.property, *value, binder_);
        if (binding.mode == BindingMode::TwoWay) {
            AddDiagnostic(Diagnostic::Severity::Info, "KXR3004",
                          "TwoWay binding metadata is retained; live source notifications are a later runtime stage." );
        }
    }
}

void BuildContext::AddDiagnostic(Diagnostic::Severity severity,
                                 std::string code,
                                 std::string message) {
    diagnostics_.push_back({severity, std::move(code), std::move(message)});
}

bool BuildContext::HasErrors() const {
    return std::any_of(diagnostics_.begin(), diagnostics_.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.severity == Diagnostic::Severity::Error;
    });
}

} // namespace keshui::xaml
