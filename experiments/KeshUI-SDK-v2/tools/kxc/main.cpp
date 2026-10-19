#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

struct Attribute {
    std::string name;
    std::string value;
};

struct Node {
    std::string name;
    std::vector<Attribute> attributes;
    std::vector<std::unique_ptr<Node>> children;
    std::string text;
    int line{1};
};

struct ResourceModel {
    std::string key;
    std::string type;
    std::string value;
    std::optional<std::string> theme;
    int line{1};
};

struct SetterModel {
    std::string property;
    std::string value;
};

struct StyleModel {
    std::string key;
    std::string targetType;
    std::string basedOn;
    std::vector<SetterModel> setters;
    int line{1};
};

struct CompileModel {
    int nodeCount{0};
    int attributeCount{0};
    std::vector<ResourceModel> resources;
    std::vector<StyleModel> styles;
    std::unordered_set<std::string> names;
    std::unordered_set<std::string> resourceKeys;
};

[[noreturn]] void Fail(int line, std::string_view code, const std::string& message) {
    throw std::runtime_error(std::string(code) + " at XAML line " + std::to_string(line) + ": " + message);
}

std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("KX0001: cannot open input: " + path);
    std::ostringstream stream;
    stream << in.rdbuf();
    return stream.str();
}

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

std::string DecodeEntities(std::string value) {
    const std::pair<std::string_view, std::string_view> pairs[] = {
        {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""},
        {"&apos;", "'"}, {"&amp;", "&"}
    };
    for (const auto& [from, to] : pairs) {
        std::size_t pos = 0;
        while ((pos = value.find(from, pos)) != std::string::npos) {
            value.replace(pos, from.size(), to);
            pos += to.size();
        }
    }
    return value;
}

class Parser {
public:
    explicit Parser(std::string source) : source_(std::move(source)) {}

    std::unique_ptr<Node> Parse() {
        SkipTrivia();
        auto root = ParseNode();
        SkipTrivia();
        if (pos_ != source_.size()) Error("unexpected trailing content");
        return root;
    }

private:
    [[noreturn]] void Error(const std::string& message) const {
        Fail(line_, "KX1000", message);
    }

    bool Starts(std::string_view value) const {
        return source_.compare(pos_, value.size(), value) == 0;
    }

    char Peek() const {
        return pos_ < source_.size() ? source_[pos_] : '\0';
    }

    char Get() {
        if (pos_ >= source_.size()) Error("unexpected end of file");
        const char c = source_[pos_++];
        if (c == '\n') ++line_;
        return c;
    }

    void SkipSpace() {
        while (std::isspace(static_cast<unsigned char>(Peek()))) Get();
    }

    void SkipTrivia() {
        for (;;) {
            SkipSpace();
            if (Starts("<?")) {
                pos_ += 2;
                while (pos_ < source_.size() && !Starts("?>")) Get();
                if (!Starts("?>")) Error("unterminated processing instruction");
                pos_ += 2;
                continue;
            }
            if (Starts("<!--")) {
                pos_ += 4;
                while (pos_ < source_.size() && !Starts("-->")) Get();
                if (!Starts("-->")) Error("unterminated comment");
                pos_ += 3;
                continue;
            }
            break;
        }
    }

    std::string Name() {
        const std::size_t start = pos_;
        while (pos_ < source_.size()) {
            const char c = Peek();
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ':' ||
                c == '.' || c == '-') {
                ++pos_;
            } else {
                break;
            }
        }
        if (start == pos_) Error("expected XML/XAML name");
        return source_.substr(start, pos_ - start);
    }

    std::string Quoted() {
        SkipSpace();
        const char quote = Get();
        if (quote != '"' && quote != '\'') Error("expected quoted attribute value");
        std::string out;
        while (Peek() != quote) {
            if (Peek() == '\0') Error("unterminated attribute value");
            out += Get();
        }
        Get();
        return DecodeEntities(std::move(out));
    }

    std::unique_ptr<Node> ParseNode() {
        SkipTrivia();
        if (Get() != '<') Error("expected '<'");
        if (Peek() == '/') Error("unexpected closing tag");

        auto node = std::make_unique<Node>();
        node->line = line_;
        node->name = Name();

        for (;;) {
            SkipSpace();
            if (Starts("/>")) {
                pos_ += 2;
                return node;
            }
            if (Peek() == '>') {
                Get();
                break;
            }
            Attribute attribute;
            attribute.name = Name();
            SkipSpace();
            if (Get() != '=') Error("expected '=' after attribute name");
            attribute.value = Quoted();
            node->attributes.push_back(std::move(attribute));
        }

        std::string text;
        for (;;) {
            if (pos_ >= source_.size()) Error("unterminated element " + node->name);
            if (Starts("<!--")) {
                SkipTrivia();
                continue;
            }
            if (Starts("</")) {
                pos_ += 2;
                const auto closing = Name();
                SkipSpace();
                if (Get() != '>') Error("expected '>' after closing element");
                if (closing != node->name) {
                    Error("closing tag mismatch: expected " + node->name + ", got " + closing);
                }
                break;
            }
            if (Peek() == '<') node->children.push_back(ParseNode());
            else text += Get();
        }
        node->text = Trim(DecodeEntities(std::move(text)));
        return node;
    }

    std::string source_;
    std::size_t pos_{0};
    int line_{1};
};

std::string LocalName(std::string_view raw) {
    const auto pos = raw.rfind(':');
    return std::string(pos == std::string_view::npos ? raw : raw.substr(pos + 1));
}

const Attribute* FindAttribute(const Node& node, std::string_view name) {
    const auto it = std::find_if(node.attributes.begin(), node.attributes.end(), [&](const Attribute& attribute) {
        return attribute.name == name;
    });
    return it == node.attributes.end() ? nullptr : &*it;
}

std::string AttributeValue(const Node& node, std::string_view name, std::string fallback = {}) {
    if (const auto* attribute = FindAttribute(node, name)) return attribute->value;
    return fallback;
}

bool HasAttribute(const Node& node, std::string_view name) {
    return FindAttribute(node, name) != nullptr;
}

const std::unordered_set<std::string>& KnownVisualTypes() {
    static const std::unordered_set<std::string> types = {
        "Window", "Grid", "StackPanel", "Canvas", "Border", "TextBlock",
        "Button", "AccentButton", "HyperlinkButton", "ToggleSwitch", "CheckBox",
        "RadioButton", "Slider", "ProgressBar", "ProgressRing", "TextBox",
        "SearchBox", "ComboBox", "ListView", "TabView", "NavigationView",
        "InfoBar", "Badge", "Card", "SettingsCard", "BreadcrumbBar", "Expander",
        "Separator", "ScrollViewer", "Tooltip", "ContextMenu", "ContentDialog"
    };
    return types;
}

const std::unordered_set<std::string>& KnownMetadataTypes() {
    static const std::unordered_set<std::string> types = {
        "ResourceDictionary", "Style", "Setter", "ControlTemplate", "DataTemplate",
        "VisualStateManager", "VisualStateGroup", "VisualState", "VisualTransition",
        "Storyboard", "ObjectAnimationUsingKeyFrames", "DiscreteObjectKeyFrame",
        "DoubleAnimation", "BrushTransition", "ScalarTransition",
        "SolidColorBrush", "Color", "Thickness", "CornerRadius", "String", "Double"
    };
    return types;
}

bool IsPropertyElement(const Node& node) {
    return node.name.find('.') != std::string::npos;
}

bool IsResourcesProperty(const Node& node) {
    const auto local = LocalName(node.name);
    return local.size() > 10 && local.ends_with(".Resources");
}

bool IsThemeDictionariesProperty(const Node& node) {
    return LocalName(node.name).ends_with(".ThemeDictionaries");
}

bool IsMetadataNode(const Node& node) {
    return IsPropertyElement(node) || KnownMetadataTypes().contains(LocalName(node.name));
}

std::string ParseResourceKey(std::string_view value) {
    const std::string text = Trim(value);
    constexpr std::string_view prefix = "{StaticResource ";
    if (text.rfind(prefix, 0) == 0 && text.size() > prefix.size() && text.back() == '}') {
        return Trim(std::string_view(text).substr(prefix.size(), text.size() - prefix.size() - 1));
    }
    return {};
}

std::optional<std::string> ThemeFromKey(std::string_view value) {
    const auto lower = Lower(Trim(value));
    if (lower == "default") return std::string("Default");
    if (lower == "light") return std::string("Light");
    if (lower == "dark") return std::string("Dark");
    if (lower == "highcontrast") return std::string("HighContrast");
    return std::nullopt;
}

std::string ResourceValue(const Node& node) {
    if (const auto* value = FindAttribute(node, "Value")) return value->value;
    if (const auto* color = FindAttribute(node, "Color")) return color->value;
    return node.text;
}

void CollectStyle(const Node& node, CompileModel& model) {
    StyleModel style;
    style.line = node.line;
    style.key = AttributeValue(node, "x:Key");
    style.targetType = AttributeValue(node, "TargetType");
    style.basedOn = ParseResourceKey(AttributeValue(node, "BasedOn"));

    if (style.key.empty()) Fail(node.line, "KX2101", "Style resource requires x:Key");
    if (style.targetType.empty()) Fail(node.line, "KX2102", "Style resource requires TargetType");
    if (!KnownVisualTypes().contains(LocalName(style.targetType))) {
        Fail(node.line, "KX2103", "unknown Style TargetType '" + style.targetType + "'");
    }

    for (const auto& child : node.children) {
        if (LocalName(child->name) != "Setter") continue;
        SetterModel setter;
        setter.property = AttributeValue(*child, "Property");
        setter.value = AttributeValue(*child, "Value");
        if (setter.property.empty()) Fail(child->line, "KX2104", "Setter requires Property");
        if (setter.value.empty() && !HasAttribute(*child, "Value")) {
            Fail(child->line, "KX2105", "Setter.Value property elements are parsed but not emitted in v0.3; use Value= for now");
        }
        style.setters.push_back(std::move(setter));
    }

    if (!model.resourceKeys.insert(style.key).second) {
        Fail(node.line, "KX2106", "duplicate resource key '" + style.key + "'");
    }
    model.styles.push_back(std::move(style));
}

void CollectResources(const Node& node,
                      CompileModel& model,
                      bool insideResources,
                      std::optional<std::string> theme = std::nullopt) {
    const auto local = LocalName(node.name);

    if (IsResourcesProperty(node)) insideResources = true;

    if (insideResources && IsThemeDictionariesProperty(node)) {
        for (const auto& child : node.children) {
            if (LocalName(child->name) != "ResourceDictionary") continue;
            const auto dictionaryTheme = ThemeFromKey(AttributeValue(*child, "x:Key"));
            for (const auto& grandchild : child->children) {
                CollectResources(*grandchild, model, true, dictionaryTheme);
            }
        }
        return;
    }

    if (insideResources && local == "Style") {
        CollectStyle(node, model);
        return;
    }

    if (insideResources && (local == "SolidColorBrush" || local == "Color" ||
                            local == "Thickness" || local == "CornerRadius" ||
                            local == "String" || local == "Double")) {
        ResourceModel resource;
        resource.line = node.line;
        resource.key = AttributeValue(node, "x:Key");
        resource.type = local;
        resource.value = ResourceValue(node);
        resource.theme = theme;
        if (resource.key.empty()) Fail(node.line, "KX2001", local + " resource requires x:Key");
        if (resource.value.empty()) Fail(node.line, "KX2002", "resource '" + resource.key + "' has no value");

        const std::string uniquenessKey = resource.theme ? resource.key + "@" + *resource.theme : resource.key;
        if (!model.resourceKeys.insert(uniquenessKey).second) {
            Fail(node.line, "KX2003", "duplicate resource key '" + uniquenessKey + "'");
        }
        model.resources.push_back(std::move(resource));
        return;
    }

    for (const auto& child : node.children) {
        CollectResources(*child, model, insideResources, theme);
    }
}

void ValidateMarkupExtension(int line, std::string_view value) {
    const std::string text = Trim(value);
    if (text.empty() || text.front() != '{') return;
    if (text.back() != '}') Fail(line, "KX2201", "unterminated markup extension '" + text + "'");
    const std::string lower = Lower(text);
    if (lower.rfind("{staticresource ", 0) == 0 || lower.rfind("{themeresource ", 0) == 0 ||
        lower.rfind("{binding", 0) == 0 || lower.rfind("{x:bind", 0) == 0 ||
        lower.rfind("{templatebinding ", 0) == 0) return;
    Fail(line, "KX2202", "unsupported markup extension '" + text + "'");
}

void ValidateVisualTree(const Node& node, CompileModel& model, bool inResources = false) {
    ++model.nodeCount;
    model.attributeCount += static_cast<int>(node.attributes.size());

    if (IsResourcesProperty(node)) inResources = true;
    const auto local = LocalName(node.name);

    if (!inResources && !IsMetadataNode(node) && !KnownVisualTypes().contains(local)) {
        Fail(node.line, "KX1101", "unknown KeshUI type '" + node.name + "'");
    }

    if (const auto* name = FindAttribute(node, "x:Name")) {
        if (!model.names.insert(name->value).second) {
            Fail(node.line, "KX1102", "duplicate x:Name '" + name->value + "' in page namescope");
        }
    }

    for (const auto& attribute : node.attributes) {
        if (attribute.name.empty()) Fail(node.line, "KX1103", "empty attribute name");
        ValidateMarkupExtension(node.line, attribute.value);
    }

    for (const auto& child : node.children) ValidateVisualTree(*child, model, inResources);
}

void ValidateStyleGraph(const CompileModel& model) {
    std::unordered_map<std::string, const StyleModel*> styles;
    for (const auto& style : model.styles) styles[style.key] = &style;

    for (const auto& style : model.styles) {
        if (style.basedOn.empty()) continue;
        std::set<std::string> chain;
        const StyleModel* current = &style;
        while (current && !current->basedOn.empty()) {
            if (!chain.insert(current->key).second) {
                Fail(style.line, "KX2110", "Style BasedOn cycle involving '" + current->key + "'");
            }
            const auto it = styles.find(current->basedOn);
            if (it == styles.end()) break;
            current = it->second;
        }
    }
}

std::string EscapeCpp(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 16);
    for (const char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string ThemeEnum(const std::optional<std::string>& theme) {
    if (!theme || *theme == "Default") return "std::nullopt";
    if (*theme == "Light") return "keshui::ThemeKind::Light";
    if (*theme == "Dark") return "keshui::ThemeKind::Dark";
    if (*theme == "HighContrast") return "keshui::ThemeKind::HighContrast";
    return "std::nullopt";
}

void Dump(const Node& node, int depth = 0) {
    std::cout << std::string(static_cast<std::size_t>(depth * 2), ' ')
              << (IsMetadataNode(node) ? "@" : "+") << " " << node.name;
    for (const auto& attribute : node.attributes) {
        std::cout << " " << attribute.name << "=\"" << attribute.value << "\"";
    }
    if (!node.text.empty()) std::cout << " text=\"" << node.text << "\"";
    std::cout << "\n";
    for (const auto& child : node.children) Dump(*child, depth + 1);
}

struct Generator {
    int next{0};
    std::ostringstream out;
};

bool IsVisualNode(const Node& node, bool inResources) {
    if (inResources) return false;
    return KnownVisualTypes().contains(LocalName(node.name));
}

std::string EmitVisualNode(Generator& generator,
                           const Node& node,
                           const std::string& parent,
                           bool inResources = false) {
    if (IsResourcesProperty(node) || inResources || !IsVisualNode(node, false)) return {};

    const std::string id = "n" + std::to_string(generator.next++);
    generator.out << "    auto " << id << " = keshui::CreateXamlElement(\""
                  << EscapeCpp(node.name) << "\");\n";
    generator.out << "    if (!" << id << ") throw std::runtime_error(\"KXaml generated unknown element: "
                  << EscapeCpp(node.name) << "\");\n";

    bool hasContent = false;
    for (const auto& attribute : node.attributes) {
        if (attribute.name == "x:Key") continue;
        if (attribute.name == "Content") hasContent = true;
        generator.out << "    ctx.Apply(" << id << ", \"" << EscapeCpp(attribute.name)
                      << "\", \"" << EscapeCpp(attribute.value) << "\");\n";
    }
    if (!node.text.empty() && !hasContent) {
        generator.out << "    ctx.Apply(" << id << ", \"Content\", \""
                      << EscapeCpp(node.text) << "\");\n";
    }
    if (!parent.empty()) generator.out << "    ctx.AddChild(" << parent << ", " << id << ");\n";

    for (const auto& child : node.children) {
        if (IsResourcesProperty(*child) || IsPropertyElement(*child) ||
            KnownMetadataTypes().contains(LocalName(child->name))) continue;
        EmitVisualNode(generator, *child, id, false);
    }
    return id;
}

std::string Generate(const Node& root, const CompileModel& model, const std::string& input) {
    Generator generator;
    generator.out << "// Generated by KXamlC 0.3 from " << EscapeCpp(input) << ". DO NOT EDIT.\n";
    generator.out << "#pragma once\n";
    generator.out << "#include <keshui/keshui.hpp>\n";
    generator.out << "#include <keshui/xaml_runtime.hpp>\n";
    generator.out << "#include <optional>\n#include <stdexcept>\n\n";
    generator.out << "namespace keshui_generated {\n";
    generator.out << "inline std::shared_ptr<keshui::Window> BuildMainWindow(keshui::XamlEventBinder& binder) {\n";
    generator.out << "    keshui::xaml::BuildContext ctx(binder);\n";

    for (const auto& resource : model.resources) {
        generator.out << "    ctx.AddResource(keshui::xaml::ResourceSpec{\"" << EscapeCpp(resource.key)
                      << "\", \"" << EscapeCpp(resource.value) << "\", " << ThemeEnum(resource.theme) << "});\n";
    }

    for (const auto& style : model.styles) {
        generator.out << "    ctx.AddStyle(keshui::xaml::StyleSpec{\"" << EscapeCpp(style.key)
                      << "\", \"" << EscapeCpp(style.targetType) << "\", \""
                      << EscapeCpp(style.basedOn) << "\", {";
        for (std::size_t i = 0; i < style.setters.size(); ++i) {
            const auto& setter = style.setters[i];
            if (i) generator.out << ", ";
            generator.out << "{\"" << EscapeCpp(setter.property) << "\", \""
                          << EscapeCpp(setter.value) << "\"}";
        }
        generator.out << "}});\n";
    }

    const auto rootId = EmitVisualNode(generator, root, {});
    generator.out << "    auto window = std::dynamic_pointer_cast<keshui::Window>(" << rootId << ");\n";
    generator.out << "    if (!window) throw std::runtime_error(\"Root element must be Window\");\n";
    generator.out << "    ctx.Finalize(window);\n";
    generator.out << "    if (ctx.HasErrors()) {\n";
    generator.out << "        for (const auto& d : ctx.Diagnostics()) if (d.severity == keshui::xaml::Diagnostic::Severity::Error) throw std::runtime_error(d.code + \": \" + d.message);\n";
    generator.out << "    }\n";
    generator.out << "    return window;\n";
    generator.out << "}\n}\n";
    return generator.out.str();
}

CompileModel Analyze(const Node& root) {
    CompileModel model;
    ValidateVisualTree(root, model);
    CollectResources(root, model, false);
    ValidateStyleGraph(model);
    return model;
}

void Usage() {
    std::cerr
        << "KXamlC 0.3 - KeshUI compiled XAML compiler\n"
        << "Usage:\n"
        << "  kxc input.xaml output.g.hpp\n"
        << "  kxc --check input.xaml\n"
        << "  kxc --dump-ir input.xaml\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 3) {
            Usage();
            return 2;
        }

        const bool check = std::string_view(argv[1]) == "--check";
        const bool dump = std::string_view(argv[1]) == "--dump-ir";
        const std::string input = (check || dump) ? argv[2] : argv[1];

        auto root = Parser(ReadFile(input)).Parse();
        if (LocalName(root->name) != "Window") Fail(root->line, "KX1100", "root XAML element must be Window");
        const auto model = Analyze(*root);

        if (check) {
            std::cout << "KXamlC: OK - " << model.nodeCount << " nodes, "
                      << model.attributeCount << " attributes, " << model.resources.size()
                      << " resources, " << model.styles.size() << " styles, "
                      << model.names.size() << " named elements\n";
            return 0;
        }

        if (dump) {
            std::cout << "KXaml IR\n"
                      << "  nodes: " << model.nodeCount << "\n"
                      << "  attributes: " << model.attributeCount << "\n"
                      << "  resources: " << model.resources.size() << "\n"
                      << "  styles: " << model.styles.size() << "\n"
                      << "  names: " << model.names.size() << "\n";
            for (const auto& resource : model.resources) {
                std::cout << "  resource " << resource.key << " : " << resource.type
                          << " = " << resource.value;
                if (resource.theme) std::cout << " [" << *resource.theme << "]";
                std::cout << "\n";
            }
            for (const auto& style : model.styles) {
                std::cout << "  style " << style.key << " -> " << style.targetType
                          << " setters=" << style.setters.size();
                if (!style.basedOn.empty()) std::cout << " basedOn=" << style.basedOn;
                std::cout << "\n";
            }
            Dump(*root);
            return 0;
        }

        std::ofstream out(argv[2], std::ios::binary);
        if (!out) throw std::runtime_error("KX0002: cannot open output: " + std::string(argv[2]));
        out << Generate(*root, model, input);
        out.close();

        std::cout << "KXamlC: generated " << argv[2] << " (" << model.nodeCount
                  << " nodes, " << model.resources.size() << " resources, "
                  << model.styles.size() << " styles)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "KXamlC error: " << error.what() << "\n";
        return 1;
    }
}
