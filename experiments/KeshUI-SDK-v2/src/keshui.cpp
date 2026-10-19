#include <keshui/keshui.hpp>

#include <array>
#include <cctype>
#include <charconv>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>

namespace keshui {

namespace {
constexpr float kInf = 1'000'000.0f;

std::string Trim(std::string_view v) {
    std::size_t a = 0, b = v.size();
    while (a < b && std::isspace(static_cast<unsigned char>(v[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(v[b - 1]))) --b;
    return std::string(v.substr(a, b - a));
}

std::string Lower(std::string_view v) {
    std::string s(v);
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

int ParseInt(std::string_view value, int fallback = 0) {
    auto s = Trim(value);
    int out = fallback;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{} ? out : fallback;
}

Color ResolveAnyColor(const std::any& value, Color fallback) {
    if (const auto* c = std::any_cast<Color>(&value)) return *c;
    if (const auto* rr = std::any_cast<ResourceReference>(&value)) {
        auto resolved = ThemeManager::Current().Resolve(*rr);
        if (const auto* c = std::any_cast<Color>(&resolved)) return *c;
    }
    if (const auto* s = std::any_cast<std::string>(&value)) return ParseColorOrResource(*s, fallback);
    return fallback;
}

ResourceReference ParseResourceReference(std::string_view value) {
    auto s = Trim(value);
    ResourceReference out;
    if (s.rfind("{ThemeResource ", 0) == 0 && s.size() > 16 && s.back() == '}') {
        out.kind = ResourceReference::Kind::Theme;
        out.key = s.substr(15, s.size() - 16);
    } else if (s.rfind("{StaticResource ", 0) == 0 && s.size() > 17 && s.back() == '}') {
        out.kind = ResourceReference::Kind::Static;
        out.key = s.substr(16, s.size() - 17);
    }
    return out;
}

bool IsResourceSyntax(std::string_view value) {
    auto s = Trim(value);
    return (s.rfind("{ThemeResource ", 0) == 0 || s.rfind("{StaticResource ", 0) == 0) && !s.empty() && s.back() == '}';
}

float Max0(float v) { return std::max(0.0f, v); }

Rect Deflate(Rect r, Thickness t) {
    r.x += t.left;
    r.y += t.top;
    r.width = Max0(r.width - t.left - t.right);
    r.height = Max0(r.height - t.top - t.bottom);
    return r;
}

std::vector<Grid::Track> ParseTracks(std::string_view spec) {
    std::vector<Grid::Track> out;
    auto parts = SplitList(spec, ',');
    for (auto& raw : parts) {
        auto s = Trim(raw);
        if (s.empty()) continue;
        if (Lower(s) == "auto") {
            out.push_back({Grid::Track::Kind::Auto, 1});
        } else if (s.back() == '*') {
            auto prefix = s.substr(0, s.size() - 1);
            float weight = prefix.empty() ? 1.0f : ParseFloat(prefix, 1.0f);
            out.push_back({Grid::Track::Kind::Star, std::max(0.001f, weight)});
        } else {
            out.push_back({Grid::Track::Kind::Pixel, std::max(0.0f, ParseFloat(s, 0))});
        }
    }
    if (out.empty()) out.push_back({Grid::Track::Kind::Star, 1});
    return out;
}

Color SeverityColor(InfoBar::Severity s) {
    auto& p = ThemeManager::Current().Palette();
    switch (s) {
        case InfoBar::Severity::Success: return p.success;
        case InfoBar::Severity::Warning: return p.warning;
        case InfoBar::Severity::Error: return p.danger;
        default: return p.accent;
    }
}

} // namespace

// -----------------------------------------------------------------------------
// Primitive parsing
// -----------------------------------------------------------------------------

Color Color::FromHex(std::string_view hex) {
    std::string s = Trim(hex);
    if (!s.empty() && s[0] == '#') s.erase(s.begin());
    unsigned value = 0;
    try { value = static_cast<unsigned>(std::stoul(s, nullptr, 16)); }
    catch (...) { return {}; }
    if (s.size() == 6) {
        return {((value >> 16) & 0xff) / 255.0f, ((value >> 8) & 0xff) / 255.0f,
                (value & 0xff) / 255.0f, 1.0f};
    }
    if (s.size() == 8) {
        return {((value >> 16) & 0xff) / 255.0f, ((value >> 8) & 0xff) / 255.0f,
                (value & 0xff) / 255.0f, ((value >> 24) & 0xff) / 255.0f};
    }
    return {};
}

Color Color::Lerp(Color x, Color y, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return {x.r + (y.r-x.r)*t, x.g + (y.g-x.g)*t, x.b + (y.b-x.b)*t, x.a + (y.a-x.a)*t};
}

float ParseFloat(std::string_view value, float fallback) {
    try { return std::stof(Trim(value)); } catch (...) { return fallback; }
}

bool ParseBool(std::string_view value, bool fallback) {
    auto s = Lower(Trim(value));
    if (s == "true" || s == "1" || s == "yes") return true;
    if (s == "false" || s == "0" || s == "no") return false;
    return fallback;
}

Thickness ParseThickness(std::string_view value) {
    auto parts = SplitList(value, ',');
    if (parts.size() == 1) return Thickness::Uniform(ParseFloat(parts[0]));
    if (parts.size() == 2) {
        float h = ParseFloat(parts[0]), v = ParseFloat(parts[1]);
        return {h,v,h,v};
    }
    if (parts.size() >= 4) return {ParseFloat(parts[0]), ParseFloat(parts[1]), ParseFloat(parts[2]), ParseFloat(parts[3])};
    return {};
}

Color ParseColorOrResource(std::string_view value, Color fallback) {
    auto s = Trim(value);
    if (s.empty()) return fallback;
    if (s[0] == '#') return Color::FromHex(s);
    if (IsResourceSyntax(s)) return ThemeManager::Current().ResolveColor(ParseResourceReference(s).key);
    return ThemeManager::Current().ResolveColor(s);
}

std::vector<std::string> SplitList(std::string_view value, char delimiter) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= value.size()) {
        auto end = value.find(delimiter, start);
        if (end == std::string_view::npos) end = value.size();
        out.push_back(Trim(value.substr(start, end - start)));
        if (end == value.size()) break;
        start = end + 1;
    }
    return out;
}

// -----------------------------------------------------------------------------
// Resources / themes
// -----------------------------------------------------------------------------

const std::any* ResourceDictionary::TryGet(std::string_view key) const {
    auto it = values_.find(std::string(key));
    return it == values_.end() ? nullptr : &it->second;
}

void ResourceDictionary::SetThemeDictionary(ThemeKind theme, std::shared_ptr<ResourceDictionary> dictionary) {
    themes_[static_cast<int>(theme)] = std::move(dictionary);
}

const std::any* ResourceDictionary::TryGetTheme(std::string_view key, ThemeKind theme) const {
    auto it = themes_.find(static_cast<int>(theme));
    if (it != themes_.end() && it->second) if (auto* v = it->second->TryGet(key)) return v;
    return TryGet(key);
}

ThemeManager& ThemeManager::Current() {
    static ThemeManager manager;
    return manager;
}

ThemeManager::ThemeManager() { Rebuild(); }
void ThemeManager::SetTheme(ThemeKind theme) { theme_ = theme; Rebuild(); }
void ThemeManager::SetAccent(Color accent) { accent_ = accent; Rebuild(); }

void ThemeManager::Rebuild() {
    if (theme_ == ThemeKind::Light) {
        palette_.applicationBackground = Color::FromHex("#F3F3F3");
        palette_.layerFill = Color::FromHex("#FFFFFF");
        palette_.layerFillAlt = Color::FromHex("#F9F9F9");
        palette_.cardFill = Color::FromHex("#FFFFFF");
        palette_.cardStroke = Color::FromHex("#E0E0E0");
        palette_.controlFillDefault = Color::FromHex("#FDFDFD");
        palette_.controlFillHover = Color::FromHex("#F6F6F6");
        palette_.controlFillPressed = Color::FromHex("#EFEFEF");
        palette_.controlFillDisabled = Color::FromHex("#F5F5F5");
        palette_.controlStroke = Color::FromHex("#D2D2D2");
        palette_.controlStrokeStrong = Color::FromHex("#8A8A8A");
        palette_.textPrimary = Color::FromHex("#1B1B1B");
        palette_.textSecondary = Color::FromHex("#5E5E5E");
        palette_.textDisabled = Color::FromHex("#9A9A9A");
    } else if (theme_ == ThemeKind::HighContrast) {
        palette_.applicationBackground = Color::FromHex("#000000");
        palette_.layerFill = Color::FromHex("#000000");
        palette_.layerFillAlt = Color::FromHex("#000000");
        palette_.cardFill = Color::FromHex("#000000");
        palette_.cardStroke = Color::FromHex("#FFFFFF");
        palette_.controlFillDefault = Color::FromHex("#000000");
        palette_.controlFillHover = Color::FromHex("#FFFFFF");
        palette_.controlFillPressed = Color::FromHex("#FFFFFF");
        palette_.controlFillDisabled = Color::FromHex("#222222");
        palette_.controlStroke = Color::FromHex("#FFFFFF");
        palette_.controlStrokeStrong = Color::FromHex("#FFFFFF");
        palette_.textPrimary = Color::FromHex("#FFFFFF");
        palette_.textSecondary = Color::FromHex("#FFFFFF");
        palette_.textDisabled = Color::FromHex("#A0A0A0");
    } else {
        palette_.applicationBackground = Color::FromHex("#101216");
        palette_.layerFill = Color::FromHex("#171A20");
        palette_.layerFillAlt = Color::FromHex("#1D2027");
        palette_.cardFill = Color::FromHex("#1C2027");
        palette_.cardStroke = Color::FromHex("#303640");
        palette_.controlFillDefault = Color::FromHex("#242830");
        palette_.controlFillHover = Color::FromHex("#2C313A");
        palette_.controlFillPressed = Color::FromHex("#20242B");
        palette_.controlFillDisabled = Color::FromHex("#1B1E24");
        palette_.controlStroke = Color::FromHex("#3B424D");
        palette_.controlStrokeStrong = Color::FromHex("#596372");
        palette_.textPrimary = Color::FromHex("#F4F6F8");
        palette_.textSecondary = Color::FromHex("#AAB2BE");
        palette_.textDisabled = Color::FromHex("#676E78");
    }
    palette_.accent = accent_;
    palette_.accentHover = Color::Lerp(accent_, Color{1,1,1,1}, 0.12f);
    palette_.accentPressed = Color::Lerp(accent_, Color{0,0,0,1}, 0.12f);
    palette_.textOnAccent = Color::FromHex("#101010");
    palette_.danger = Color::FromHex("#E85D5D");
    palette_.success = Color::FromHex("#5CCF8A");
    palette_.warning = Color::FromHex("#E9B949");
    palette_.focus = accent_;
}

Color ThemeManager::ResolveColor(std::string_view key) const {
    const auto& p = palette_;
    const std::string k(key);
    if (k == "ApplicationPageBackgroundThemeBrush" || k == "ApplicationBackground") return p.applicationBackground;
    if (k == "LayerFillColorDefaultBrush" || k == "LayerFill") return p.layerFill;
    if (k == "LayerFillColorAltBrush" || k == "LayerFillAlt") return p.layerFillAlt;
    if (k == "CardBackgroundFillColorDefaultBrush" || k == "CardFill") return p.cardFill;
    if (k == "CardStrokeColorDefaultBrush" || k == "CardStroke") return p.cardStroke;
    if (k == "ControlFillColorDefaultBrush" || k == "ButtonBackground") return p.controlFillDefault;
    if (k == "ControlFillColorSecondaryBrush" || k == "ButtonBackgroundPointerOver") return p.controlFillHover;
    if (k == "ControlFillColorTertiaryBrush" || k == "ButtonBackgroundPressed") return p.controlFillPressed;
    if (k == "ControlFillColorDisabledBrush" || k == "ButtonBackgroundDisabled") return p.controlFillDisabled;
    if (k == "ControlStrokeColorDefaultBrush" || k == "ControlElevationBorderBrush") return p.controlStroke;
    if (k == "TextFillColorPrimaryBrush" || k == "ButtonForeground") return p.textPrimary;
    if (k == "TextFillColorSecondaryBrush" || k == "ButtonForegroundPressed") return p.textSecondary;
    if (k == "TextFillColorDisabledBrush" || k == "ButtonForegroundDisabled") return p.textDisabled;
    if (k == "AccentFillColorDefaultBrush" || k == "Accent") return p.accent;
    if (k == "AccentFillColorSecondaryBrush") return p.accentHover;
    if (k == "AccentFillColorTertiaryBrush") return p.accentPressed;
    if (k == "TextOnAccentFillColorPrimaryBrush") return p.textOnAccent;
    if (k == "SystemFillColorCriticalBrush" || k == "Danger") return p.danger;
    if (k == "SystemFillColorSuccessBrush" || k == "Success") return p.success;
    if (k == "SystemFillColorCautionBrush" || k == "Warning") return p.warning;
    return p.textPrimary;
}

std::any ThemeManager::Resolve(const ResourceReference& ref) const {
    (void)ref.kind;
    return std::any(ResolveColor(ref.key));
}

// -----------------------------------------------------------------------------
// Dependency properties
// -----------------------------------------------------------------------------

const DependencyProperty* DependencyProperty::RegisterImpl(std::string name, std::type_index type,
                                                            std::type_index owner, PropertyMetadata metadata) {
    static std::mutex mutex;
    static std::vector<std::unique_ptr<DependencyProperty>> registry;
    std::scoped_lock lock(mutex);
    const std::size_t id = registry.size() + 1;
    registry.push_back(std::unique_ptr<DependencyProperty>(new DependencyProperty(
        std::move(name), type, owner, std::move(metadata), id)));
    return registry.back().get();
}

std::any DependencyObject::GetValue(const DependencyProperty* property) const {
    if (!property) return {};
    if (auto it = animatedValues_.find(property->Id()); it != animatedValues_.end()) return it->second;
    if (auto it = localValues_.find(property->Id()); it != localValues_.end()) return it->second;
    if (auto it = styleValues_.find(property->Id()); it != styleValues_.end()) return it->second;
    if (property->Metadata().inherits) {
        if (auto parent = inheritanceParent_.lock()) return parent->GetValue(property);
    }
    return property->Metadata().defaultValue;
}

void DependencyObject::SetValue(const DependencyProperty* property, std::any value) {
    if (!property) return;
    auto old = GetValue(property);
    localValues_[property->Id()] = std::move(value);
    auto now = GetValue(property);
    OnPropertyChanged(property, old, now);
    if (property->Metadata().changed) property->Metadata().changed(*this, old, now);
}

void DependencyObject::ClearValue(const DependencyProperty* property) {
    if (!property) return;
    auto old = GetValue(property);
    localValues_.erase(property->Id());
    auto now = GetValue(property);
    OnPropertyChanged(property, old, now);
    if (property->Metadata().changed) property->Metadata().changed(*this, old, now);
}

void DependencyObject::SetStyleValue(const DependencyProperty* property, std::any value) {
    if (!property) return;
    styleValues_[property->Id()] = std::move(value);
}
void DependencyObject::SetAnimatedValue(const DependencyProperty* property, std::any value) {
    if (!property) return;
    animatedValues_[property->Id()] = std::move(value);
}
void DependencyObject::ClearAnimatedValue(const DependencyProperty* property) {
    if (property) animatedValues_.erase(property->Id());
}

void Style::Apply(DependencyObject& object) const {
    if (basedOn_) basedOn_->Apply(object);
    for (const auto& setter : setters_) object.SetStyleValue(setter.property, setter.value);
}

// -----------------------------------------------------------------------------
// UI tree
// -----------------------------------------------------------------------------

void UIElement::AddChild(std::shared_ptr<UIElement> child) {
    if (!child) return;
    child->parent_ = shared_from_this();
    child->SetInheritanceParent(shared_from_this());
    children_.push_back(std::move(child));
}

void UIElement::RemoveChild(const std::shared_ptr<UIElement>& child) {
    children_.erase(std::remove(children_.begin(), children_.end(), child), children_.end());
}

std::shared_ptr<UIElement> UIElement::FindName(std::string_view name) {
    if (name_ == name) return shared_from_this();
    for (auto& child : children_) if (auto found = child->FindName(name)) return found;
    return nullptr;
}

void UIElement::Measure(const Size& available) {
    if (visibility_ == Visibility::Collapsed) { desiredSize_ = {}; return; }
    desiredSize_ = MeasureOverride(available);
}

void UIElement::Arrange(const Rect& finalRect) {
    bounds_ = finalRect;
    ArrangeOverride({finalRect.width, finalRect.height});
}

void UIElement::Render(Renderer& renderer) {
    if (visibility_ == Visibility::Collapsed) return;
    RenderSelf(renderer);
    for (auto& child : children_) child->Render(renderer);
}

std::shared_ptr<UIElement> UIElement::HitTest(Point p) {
    if (visibility_ == Visibility::Collapsed || !hitTestVisible_ || !bounds_.Contains(p)) return nullptr;
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if (auto hit = (*it)->HitTest(p)) return hit;
    }
    return shared_from_this();
}

void UIElement::AddHandler(std::string eventName, RoutedHandler handler) {
    handlers_[std::move(eventName)].push_back(std::move(handler));
}

void UIElement::RaiseEvent(std::string_view eventName, RoutedEventArgs& args, bool bubble) {
    if (!args.originalSource) args.originalSource = this;
    UIElement* current = this;
    while (current) {
        args.source = current;
        auto it = current->handlers_.find(std::string(eventName));
        if (it != current->handlers_.end()) {
            for (auto& handler : it->second) {
                handler(args);
                if (args.handled) return;
            }
        }
        if (!bubble) break;
        auto p = current->parent_.lock();
        current = p.get();
    }
}

void UIElement::OnPointerMove(PointerEventArgs& args) { RaiseEvent("PointerMoved", args); }
void UIElement::OnPointerPressed(PointerEventArgs& args) { RaiseEvent("PointerPressed", args); }
void UIElement::OnPointerReleased(PointerEventArgs& args) { RaiseEvent("PointerReleased", args); }
void UIElement::OnKey(KeyEventArgs& args) { RaiseEvent(args.pressed ? "KeyDown" : "KeyUp", args); }

bool UIElement::SetXamlProperty(std::string_view name, std::string_view value) {
    if (name == "x:Name" || name == "Name") { SetName(Trim(value)); return true; }
    if (name.find('.') != std::string_view::npos) { SetAttached(std::string(name), std::string(value)); return true; }
    if (name == "Visibility") { SetVisibility(Lower(value) == "collapsed" ? Visibility::Collapsed : Visibility::Visible); return true; }
    if (name == "IsHitTestVisible") { SetHitTestVisible(ParseBool(value, true)); return true; }
    return false;
}

bool UIElement::BindXamlEvent(std::string_view eventName, std::string_view handlerName, XamlEventBinder& binder) {
    AddHandler(std::string(eventName), [&binder, name = std::string(handlerName)](RoutedEventArgs& e) {
        if (e.source) binder.Invoke(name, *e.source);
    });
    return true;
}

std::string UIElement::Attached(std::string_view key, std::string_view fallback) const {
    auto it = attached_.find(std::string(key));
    return it == attached_.end() ? std::string(fallback) : it->second;
}

bool UIElement::WantsContinuousFrame() const {
    for (const auto& child : children_) if (child->WantsContinuousFrame()) return true;
    return false;
}

Size UIElement::MeasureOverride(const Size& available) {
    Size desired{};
    for (auto& child : children_) {
        child->Measure(available);
        desired.width = std::max(desired.width, child->DesiredSize().width);
        desired.height = std::max(desired.height, child->DesiredSize().height);
    }
    return desired;
}

Size UIElement::ArrangeOverride(const Size& finalSize) {
    for (auto& child : children_) child->Arrange({bounds_.x, bounds_.y, finalSize.width, finalSize.height});
    return finalSize;
}

// -----------------------------------------------------------------------------
// FrameworkElement / layout
// -----------------------------------------------------------------------------

bool FrameworkElement::SetXamlProperty(std::string_view name, std::string_view value) {
    if (UIElement::SetXamlProperty(name, value)) return true;
    if (name == "Width") { SetWidth(ParseFloat(value, -1)); return true; }
    if (name == "Height") { SetHeight(ParseFloat(value, -1)); return true; }
    if (name == "MinWidth") { SetMinWidth(ParseFloat(value)); return true; }
    if (name == "MinHeight") { SetMinHeight(ParseFloat(value)); return true; }
    if (name == "MaxWidth") { SetMaxWidth(ParseFloat(value, kInf)); return true; }
    if (name == "MaxHeight") { SetMaxHeight(ParseFloat(value, kInf)); return true; }
    if (name == "Margin") { SetMargin(ParseThickness(value)); return true; }
    if (name == "HorizontalAlignment") {
        auto s = Lower(value); SetHorizontalAlignment(s=="left"?HorizontalAlignment::Left:s=="center"?HorizontalAlignment::Center:s=="right"?HorizontalAlignment::Right:HorizontalAlignment::Stretch); return true;
    }
    if (name == "VerticalAlignment") {
        auto s = Lower(value); SetVerticalAlignment(s=="top"?VerticalAlignment::Top:s=="center"?VerticalAlignment::Center:s=="bottom"?VerticalAlignment::Bottom:VerticalAlignment::Stretch); return true;
    }
    return false;
}

void FrameworkElement::Measure(const Size& available) {
    if (GetVisibility() == Visibility::Collapsed) { desiredSize_ = {}; return; }
    Size inner{Max0(available.width - margin_.left - margin_.right), Max0(available.height - margin_.top - margin_.bottom)};
    if (width_ >= 0) inner.width = width_;
    if (height_ >= 0) inner.height = height_;
    auto measured = MeasureOverride(inner);
    if (width_ >= 0) measured.width = width_;
    if (height_ >= 0) measured.height = height_;
    measured.width = std::clamp(measured.width, minWidth_, maxWidth_);
    measured.height = std::clamp(measured.height, minHeight_, maxHeight_);
    desiredSize_ = {measured.width + margin_.left + margin_.right, measured.height + margin_.top + margin_.bottom};
}

void FrameworkElement::Arrange(const Rect& finalRect) {
    Rect slot = Deflate(finalRect, margin_);
    float w = width_ >= 0 ? width_ : (horizontalAlignment_ == HorizontalAlignment::Stretch ? slot.width : std::min(slot.width, Max0(desiredSize_.width - margin_.left - margin_.right)));
    float h = height_ >= 0 ? height_ : (verticalAlignment_ == VerticalAlignment::Stretch ? slot.height : std::min(slot.height, Max0(desiredSize_.height - margin_.top - margin_.bottom)));
    float x = slot.x, y = slot.y;
    if (horizontalAlignment_ == HorizontalAlignment::Center) x += (slot.width - w) / 2;
    else if (horizontalAlignment_ == HorizontalAlignment::Right) x += slot.width - w;
    if (verticalAlignment_ == VerticalAlignment::Center) y += (slot.height - h) / 2;
    else if (verticalAlignment_ == VerticalAlignment::Bottom) y += slot.height - h;
    bounds_ = {x,y,std::max(0.0f,w),std::max(0.0f,h)};
    ArrangeOverride({bounds_.width, bounds_.height});
}

bool StackPanel::SetXamlProperty(std::string_view name, std::string_view value) {
    if (FrameworkElement::SetXamlProperty(name, value)) return true;
    if (name == "Orientation") { orientation_ = Lower(value)=="horizontal" ? Orientation::Horizontal : Orientation::Vertical; return true; }
    if (name == "Spacing") { spacing_ = ParseFloat(value); return true; }
    if (name == "Padding") { padding_ = ParseThickness(value); return true; }
    return false;
}

Size StackPanel::MeasureOverride(const Size& available) {
    Size desired{padding_.left + padding_.right, padding_.top + padding_.bottom};
    float major = 0, cross = 0;
    int count = 0;
    for (auto& child : children_) {
        Size childAvailable = available;
        if (orientation_ == Orientation::Vertical) childAvailable.height = kInf;
        else childAvailable.width = kInf;
        child->Measure(childAvailable);
        if (orientation_ == Orientation::Vertical) { major += child->DesiredSize().height; cross = std::max(cross, child->DesiredSize().width); }
        else { major += child->DesiredSize().width; cross = std::max(cross, child->DesiredSize().height); }
        ++count;
    }
    if (count > 1) major += spacing_ * (count - 1);
    if (orientation_ == Orientation::Vertical) { desired.width += cross; desired.height += major; }
    else { desired.width += major; desired.height += cross; }
    return desired;
}

Size StackPanel::ArrangeOverride(const Size& finalSize) {
    float cursor = orientation_ == Orientation::Vertical ? bounds_.y + padding_.top : bounds_.x + padding_.left;
    for (auto& child : children_) {
        if (orientation_ == Orientation::Vertical) {
            float h = child->DesiredSize().height;
            child->Arrange({bounds_.x + padding_.left, cursor, Max0(finalSize.width-padding_.left-padding_.right), h});
            cursor += h + spacing_;
        } else {
            float w = child->DesiredSize().width;
            child->Arrange({cursor, bounds_.y + padding_.top, w, Max0(finalSize.height-padding_.top-padding_.bottom)});
            cursor += w + spacing_;
        }
    }
    return finalSize;
}

void Grid::SetColumns(std::string_view spec) { columns_ = ParseTracks(spec); }
void Grid::SetRows(std::string_view spec) { rows_ = ParseTracks(spec); }

bool Grid::SetXamlProperty(std::string_view name, std::string_view value) {
    if (FrameworkElement::SetXamlProperty(name, value)) return true;
    if (name == "Columns") { SetColumns(value); return true; }
    if (name == "Rows") { SetRows(value); return true; }
    if (name == "ColumnSpacing") { columnSpacing_ = ParseFloat(value); return true; }
    if (name == "RowSpacing") { rowSpacing_ = ParseFloat(value); return true; }
    return false;
}

Size Grid::MeasureOverride(const Size& available) {
    Size desired{};
    for (auto& child : children_) {
        child->Measure(available);
        desired.width = std::max(desired.width, child->DesiredSize().width);
        desired.height = std::max(desired.height, child->DesiredSize().height);
    }
    if (available.width < kInf) desired.width = available.width;
    if (available.height < kInf) desired.height = available.height;
    return desired;
}

static std::vector<float> ResolveTracks(const std::vector<Grid::Track>& tracks, float total, float spacing,
                                        const std::vector<std::shared_ptr<UIElement>>& children, bool columns) {
    std::vector<float> sizes(tracks.size(), 0);
    float fixed = spacing * std::max(0, int(tracks.size()) - 1);
    float starWeight = 0;
    for (std::size_t i=0;i<tracks.size();++i) {
        if (tracks[i].kind == Grid::Track::Kind::Pixel) { sizes[i] = tracks[i].value; fixed += sizes[i]; }
        else if (tracks[i].kind == Grid::Track::Kind::Auto) {
            float autoSize = 0;
            for (auto& child : children) {
                int index = ParseInt(child->Attached(columns ? "Grid.Column" : "Grid.Row", "0"));
                if (index == static_cast<int>(i)) autoSize = std::max(autoSize, columns ? child->DesiredSize().width : child->DesiredSize().height);
            }
            sizes[i] = autoSize; fixed += autoSize;
        } else starWeight += tracks[i].value;
    }
    float remainder = std::max(0.0f, total - fixed);
    if (starWeight > 0) for (std::size_t i=0;i<tracks.size();++i) if (tracks[i].kind == Grid::Track::Kind::Star) sizes[i] = remainder * tracks[i].value / starWeight;
    return sizes;
}

Size Grid::ArrangeOverride(const Size& finalSize) {
    auto col = ResolveTracks(columns_, finalSize.width, columnSpacing_, children_, true);
    auto row = ResolveTracks(rows_, finalSize.height, rowSpacing_, children_, false);
    std::vector<float> x(col.size(), bounds_.x), y(row.size(), bounds_.y);
    for (std::size_t i=1;i<col.size();++i) x[i] = x[i-1] + col[i-1] + columnSpacing_;
    for (std::size_t i=1;i<row.size();++i) y[i] = y[i-1] + row[i-1] + rowSpacing_;
    for (auto& child : children_) {
        int c = std::clamp(ParseInt(child->Attached("Grid.Column", "0")), 0, std::max(0,int(col.size())-1));
        int r = std::clamp(ParseInt(child->Attached("Grid.Row", "0")), 0, std::max(0,int(row.size())-1));
        int cs = std::max(1, ParseInt(child->Attached("Grid.ColumnSpan", "1"), 1));
        int rs = std::max(1, ParseInt(child->Attached("Grid.RowSpan", "1"), 1));
        float w = 0, h = 0;
        for (int i=c;i<std::min(int(col.size()),c+cs);++i) { w += col[i]; if (i>c) w += columnSpacing_; }
        for (int i=r;i<std::min(int(row.size()),r+rs);++i) { h += row[i]; if (i>r) h += rowSpacing_; }
        child->Arrange({x[c], y[r], w, h});
    }
    return finalSize;
}

Size Canvas::MeasureOverride(const Size& available) {
    for (auto& child : children_) child->Measure({kInf,kInf});
    return available;
}
Size Canvas::ArrangeOverride(const Size& finalSize) {
    for (auto& child : children_) {
        float left = ParseFloat(child->Attached("Canvas.Left", "0"));
        float top = ParseFloat(child->Attached("Canvas.Top", "0"));
        child->Arrange({bounds_.x+left,bounds_.y+top,child->DesiredSize().width,child->DesiredSize().height});
    }
    return finalSize;
}

bool Border::SetXamlProperty(std::string_view name, std::string_view value) {
    if (FrameworkElement::SetXamlProperty(name,value)) return true;
    if (name == "Background") { background_ = ParseColorOrResource(value, background_); return true; }
    if (name == "BorderBrush") { borderBrush_ = ParseColorOrResource(value, borderBrush_); return true; }
    if (name == "BorderThickness") { borderThickness_ = ParseFloat(value); return true; }
    if (name == "CornerRadius") { cornerRadius_ = CornerRadius::Uniform(ParseFloat(value)); return true; }
    if (name == "Padding") { padding_ = ParseThickness(value); return true; }
    return false;
}
Size Border::MeasureOverride(const Size& available) {
    Size d{padding_.left+padding_.right, padding_.top+padding_.bottom};
    if (!children_.empty()) { children_[0]->Measure({Max0(available.width-d.width),Max0(available.height-d.height)}); d.width += children_[0]->DesiredSize().width; d.height += children_[0]->DesiredSize().height; }
    return d;
}
Size Border::ArrangeOverride(const Size& finalSize) {
    if (!children_.empty()) children_[0]->Arrange(Deflate(bounds_, padding_));
    return finalSize;
}
void Border::RenderSelf(Renderer& r) {
    if (background_.a > 0) r.FillRoundedRect(bounds_, cornerRadius_, background_);
    if (borderThickness_ > 0 && borderBrush_.a > 0) r.StrokeRoundedRect(bounds_, cornerRadius_, borderBrush_, borderThickness_);
}

// -----------------------------------------------------------------------------
// Controls
// -----------------------------------------------------------------------------

const DependencyProperty* Control::BackgroundProperty() {
    static auto* p = DependencyProperty::Register<Control>("Background", ResourceReference{ResourceReference::Kind::Theme,"ControlFillColorDefaultBrush"}); return p;
}
const DependencyProperty* Control::ForegroundProperty() {
    static auto* p = DependencyProperty::Register<Control>("Foreground", ResourceReference{ResourceReference::Kind::Theme,"TextFillColorPrimaryBrush"}, {}, true); return p;
}
const DependencyProperty* Control::BorderBrushProperty() {
    static auto* p = DependencyProperty::Register<Control>("BorderBrush", ResourceReference{ResourceReference::Kind::Theme,"ControlStrokeColorDefaultBrush"}); return p;
}
const DependencyProperty* Control::CornerRadiusProperty() {
    static auto* p = DependencyProperty::Register<Control>("CornerRadius", 4.0f); return p;
}
const DependencyProperty* Control::IsEnabledProperty() {
    static auto* p = DependencyProperty::Register<Control>("IsEnabled", true); return p;
}

void Control::SetStyle(std::shared_ptr<Style> style) { style_ = std::move(style); if (style_) style_->Apply(*this); }
void Control::SetEnabled(bool v) { Set(IsEnabledProperty(),v); SetState(v?ControlState::Normal:ControlState::Disabled); }

bool Control::SetXamlProperty(std::string_view name, std::string_view value) {
    if (FrameworkElement::SetXamlProperty(name,value)) return true;
    if (name == "IsEnabled") { SetEnabled(ParseBool(value,true)); return true; }
    if (name == "Background") { SetValue(BackgroundProperty(), IsResourceSyntax(value) ? std::any(ParseResourceReference(value)) : std::any(ParseColorOrResource(value))); return true; }
    if (name == "Foreground") { SetValue(ForegroundProperty(), IsResourceSyntax(value) ? std::any(ParseResourceReference(value)) : std::any(ParseColorOrResource(value))); return true; }
    if (name == "BorderBrush") { SetValue(BorderBrushProperty(), IsResourceSyntax(value) ? std::any(ParseResourceReference(value)) : std::any(ParseColorOrResource(value))); return true; }
    if (name == "CornerRadius") { Set(CornerRadiusProperty(), ParseFloat(value,4)); return true; }
    return false;
}

void Control::OnPointerMove(PointerEventArgs& args) {
    if (IsEnabled() && state_ != ControlState::Pressed) SetState(bounds_.Contains(args.position) ? ControlState::PointerOver : ControlState::Normal);
    UIElement::OnPointerMove(args);
}
void Control::OnPointerPressed(PointerEventArgs& args) {
    if (IsEnabled() && args.button == 1) { SetState(ControlState::Pressed); focusState_ = FocusState::Pointer; }
    UIElement::OnPointerPressed(args);
}
void Control::OnPointerReleased(PointerEventArgs& args) {
    if (IsEnabled()) SetState(bounds_.Contains(args.position) ? ControlState::PointerOver : ControlState::Normal);
    UIElement::OnPointerReleased(args);
}
void Control::SetState(ControlState s) { if (!IsEnabled()) state_ = ControlState::Disabled; else state_ = s; }
std::string Control::CurrentVisualStateName() const {
    switch(state_) { case ControlState::PointerOver:return "PointerOver"; case ControlState::Pressed:return "Pressed"; case ControlState::Disabled:return "Disabled"; default:return "Normal"; }
}
Color Control::BackgroundForState() const {
    auto& p = ThemeManager::Current().Palette();
    if (state_ == ControlState::PointerOver) return p.controlFillHover;
    if (state_ == ControlState::Pressed) return p.controlFillPressed;
    if (state_ == ControlState::Disabled) return p.controlFillDisabled;
    return ResolveAnyColor(GetValue(BackgroundProperty()), p.controlFillDefault);
}
Color Control::ForegroundForState() const {
    auto& p = ThemeManager::Current().Palette();
    if (state_ == ControlState::Pressed) return p.textSecondary;
    if (state_ == ControlState::Disabled) return p.textDisabled;
    return ResolveAnyColor(GetValue(ForegroundProperty()), p.textPrimary);
}
Color Control::BorderForState() const {
    auto& p = ThemeManager::Current().Palette();
    if (state_ == ControlState::Pressed) return p.controlStrokeStrong;
    return ResolveAnyColor(GetValue(BorderBrushProperty()), p.controlStroke);
}
void Control::RenderFocus(Renderer& r, const Rect& rect, float radius) {
    if (focusState_ == FocusState::Keyboard) r.StrokeRoundedRect({rect.x-2,rect.y-2,rect.width+4,rect.height+4},CornerRadius::Uniform(radius+2),ThemeManager::Current().Palette().focus,2);
}

bool VisualStateManager::GoToState(Control& control, std::string_view state, bool) {
    auto s = Lower(state);
    if (s=="normal") control.OnPointerMove(*new PointerEventArgs{}); // state foundation; controls normally drive this from input.
    return s=="normal"||s=="pointerover"||s=="pressed"||s=="disabled";
}

bool ContentControl::SetXamlProperty(std::string_view name, std::string_view value) {
    if (Control::SetXamlProperty(name,value)) return true;
    if (name == "Content") { SetContent(std::string(value)); return true; }
    return false;
}

bool TextBlock::SetXamlProperty(std::string_view name, std::string_view value) {
    if (FrameworkElement::SetXamlProperty(name,value)) return true;
    if (name == "Text") { text_ = std::string(value); return true; }
    if (name == "FontSize") { fontSize_ = ParseFloat(value,14); return true; }
    if (name == "FontWeight") { auto s=Lower(value); fontWeight_=s=="semibold"?FontWeight::SemiBold:s=="medium"?FontWeight::Medium:s=="bold"?FontWeight::Bold:FontWeight::Normal; return true; }
    if (name == "Foreground") { foreground_=ParseColorOrResource(value); useThemeForeground_=false; return true; }
    if (name == "TextAlignment") { auto s=Lower(value); textAlignment_=s=="center"?TextAlignment::Center:s=="right"?TextAlignment::Right:TextAlignment::Left; return true; }
    return false;
}
Size TextBlock::MeasureOverride(const Size&) { return {std::max(1.0f,float(text_.size())*fontSize_*0.58f),fontSize_*1.45f}; }
void TextBlock::RenderSelf(Renderer& r) { r.DrawText(text_,bounds_,useThemeForeground_?ThemeManager::Current().Palette().textPrimary:foreground_,fontSize_,fontWeight_,textAlignment_); }

Button::Button() { SetMinHeight(32); }
void Button::OnPointerReleased(PointerEventArgs& args) {
    bool click = IsEnabled() && args.button == 1 && bounds_.Contains(args.position) && State() == ControlState::Pressed;
    Control::OnPointerReleased(args);
    if (click) {
        if (click_) click_();
        RoutedEventArgs e{this,this,false}; RaiseEvent("Click",e);
    }
}
bool Button::BindXamlEvent(std::string_view eventName, std::string_view handlerName, XamlEventBinder& binder) {
    if (eventName == "Click") { OnClick([&binder,name=std::string(handlerName),this]{ binder.Invoke(name,*this); }); return true; }
    return Control::BindXamlEvent(eventName,handlerName,binder);
}
Size Button::MeasureOverride(const Size& available) { return {std::min(available.width,std::max(76.0f,float(content_.size())*8.0f+28.0f)),32}; }
void Button::RenderSelf(Renderer& r) {
    float radius=Get<float>(CornerRadiusProperty());
    r.FillRoundedRect(bounds_,CornerRadius::Uniform(radius),BackgroundForState());
    r.StrokeRoundedRect(bounds_,CornerRadius::Uniform(radius),BorderForState(),1);
    r.DrawText(content_,bounds_,ForegroundForState(),14,FontWeight::Normal,TextAlignment::Center);
    RenderFocus(r,bounds_,radius);
}
void AccentButton::RenderSelf(Renderer& r) {
    auto& p=ThemeManager::Current().Palette(); Color bg=State()==ControlState::PointerOver?p.accentHover:State()==ControlState::Pressed?p.accentPressed:State()==ControlState::Disabled?p.controlFillDisabled:p.accent;
    r.FillRoundedRect(bounds_,CornerRadius::Uniform(4),bg); r.StrokeRoundedRect(bounds_,CornerRadius::Uniform(4),Color::Lerp(bg,{1,1,1,1},.15f),1); r.DrawText(content_,bounds_,State()==ControlState::Disabled?p.textDisabled:p.textOnAccent,14,FontWeight::Medium,TextAlignment::Center); RenderFocus(r,bounds_,4);
}
void HyperlinkButton::RenderSelf(Renderer& r) {
    auto& p=ThemeManager::Current().Palette(); r.DrawText(content_,bounds_,State()==ControlState::Pressed?p.accentPressed:p.accent,14,FontWeight::Normal,TextAlignment::Left); if(State()==ControlState::PointerOver) r.DrawLine({bounds_.x,bounds_.y+bounds_.height-5},{bounds_.x+bounds_.width,bounds_.y+bounds_.height-5},p.accent,1);
}

bool ToggleSwitch::SetXamlProperty(std::string_view n,std::string_view v){ if(Control::SetXamlProperty(n,v))return true; if(n=="Header"){header_=std::string(v);return true;} if(n=="IsOn"){on_=ParseBool(v);return true;} return false; }
Size ToggleSwitch::MeasureOverride(const Size& a){return {std::min(a.width,220.0f),40};}
void ToggleSwitch::OnPointerReleased(PointerEventArgs& a){bool click=State()==ControlState::Pressed&&bounds_.Contains(a.position);Control::OnPointerReleased(a);if(click){on_=!on_;if(toggled_)toggled_(on_);}}
void ToggleSwitch::RenderSelf(Renderer& r){auto&p=ThemeManager::Current().Palette();r.DrawText(header_,{bounds_.x,bounds_.y,bounds_.width-56,bounds_.height},p.textPrimary,14);Rect track{bounds_.x+bounds_.width-48,bounds_.y+9,44,22};r.FillRoundedRect(track,CornerRadius::Uniform(11),on_?p.accent:p.controlFillDefault);r.StrokeRoundedRect(track,CornerRadius::Uniform(11),on_?p.accent:p.controlStroke,1);float cx=on_?track.x+33:track.x+11;r.FillRoundedRect({cx-7,track.y+4,14,14},CornerRadius::Uniform(7),on_?p.textOnAccent:p.textSecondary);}

bool CheckBox::SetXamlProperty(std::string_view n,std::string_view v){if(Control::SetXamlProperty(n,v))return true;if(n=="Content"){content_=std::string(v);return true;}if(n=="IsChecked"){checked_=ParseBool(v);return true;}return false;}
Size CheckBox::MeasureOverride(const Size&a){return {std::min(a.width,std::max(100.0f,float(content_.size())*8+34)),32};}
void CheckBox::OnPointerReleased(PointerEventArgs&a){bool click=State()==ControlState::Pressed&&bounds_.Contains(a.position);Control::OnPointerReleased(a);if(click)checked_=!checked_;}
void CheckBox::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();Rect box{bounds_.x,bounds_.y+6,20,20};r.FillRoundedRect(box,CornerRadius::Uniform(3),checked_?p.accent:p.controlFillDefault);r.StrokeRoundedRect(box,CornerRadius::Uniform(3),checked_?p.accent:p.controlStrokeStrong,1);if(checked_){r.DrawLine({box.x+4,box.y+10},{box.x+8,box.y+15},p.textOnAccent,2);r.DrawLine({box.x+8,box.y+15},{box.x+16,box.y+5},p.textOnAccent,2);}r.DrawText(content_,{bounds_.x+30,bounds_.y,bounds_.width-30,bounds_.height},p.textPrimary,14);}
void RadioButton::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();Rect box{bounds_.x,bounds_.y+6,20,20};r.FillRoundedRect(box,CornerRadius::Uniform(10),p.controlFillDefault);r.StrokeRoundedRect(box,CornerRadius::Uniform(10),IsChecked()?p.accent:p.controlStrokeStrong,IsChecked()?2:1);if(IsChecked())r.FillRoundedRect({box.x+6,box.y+6,8,8},CornerRadius::Uniform(4),p.accent);}

void Slider::SetValue(double v){value_=std::clamp(v,minimum_,maximum_);}
bool Slider::SetXamlProperty(std::string_view n,std::string_view v){if(Control::SetXamlProperty(n,v))return true;if(n=="Minimum"){minimum_=ParseFloat(v);return true;}if(n=="Maximum"){maximum_=ParseFloat(v,100);return true;}if(n=="Value"){SetValue(ParseFloat(v,50));return true;}return false;}
Size Slider::MeasureOverride(const Size&a){return {std::min(a.width,260.0f),32};}
void Slider::SetFromPointer(Point p){double t=std::clamp((p.x-bounds_.x)/std::max(1.0f,bounds_.width),0.0f,1.0f);value_=minimum_+(maximum_-minimum_)*t;}
void Slider::OnPointerPressed(PointerEventArgs&a){Control::OnPointerPressed(a);if(a.button==1){dragging_=true;SetFromPointer(a.position);}}
void Slider::OnPointerMove(PointerEventArgs&a){Control::OnPointerMove(a);if(dragging_)SetFromPointer(a.position);}
void Slider::OnPointerReleased(PointerEventArgs&a){if(dragging_)SetFromPointer(a.position);dragging_=false;Control::OnPointerReleased(a);}
void Slider::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();float y=bounds_.y+bounds_.height/2;double t=(value_-minimum_)/std::max(.0001,maximum_-minimum_);r.FillRoundedRect({bounds_.x,y-2,bounds_.width,4},CornerRadius::Uniform(2),p.controlStroke);r.FillRoundedRect({bounds_.x,y-2,float(bounds_.width*t),4},CornerRadius::Uniform(2),p.accent);float x=bounds_.x+float(bounds_.width*t);r.FillRoundedRect({x-7,y-7,14,14},CornerRadius::Uniform(7),p.accent);}

bool ProgressBar::SetXamlProperty(std::string_view n,std::string_view v){if(Control::SetXamlProperty(n,v))return true;if(n=="Value"){SetValue(ParseFloat(v,42));return true;}return false;}
Size ProgressBar::MeasureOverride(const Size&a){return {std::min(a.width,260.0f),8};}
void ProgressBar::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();r.FillRoundedRect(bounds_,CornerRadius::Uniform(4),p.controlFillDefault);r.FillRoundedRect({bounds_.x,bounds_.y,float(bounds_.width*value_/100.0),bounds_.height},CornerRadius::Uniform(4),p.accent);}
bool ProgressRing::SetXamlProperty(std::string_view n,std::string_view v){if(Control::SetXamlProperty(n,v))return true;if(n=="Value"){SetValue(ParseFloat(v,68));return true;}return false;}
Size ProgressRing::MeasureOverride(const Size&){return {44,44};}
void ProgressRing::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();Point c{bounds_.x+bounds_.width/2,bounds_.y+bounds_.height/2};float rad=std::min(bounds_.width,bounds_.height)*.38f;int active=std::clamp(int(std::round(value_/100.0*12)),0,12);constexpr float pi=3.14159265358979323846f;for(int i=0;i<12;++i){float a=float(i)*2*pi/12-pi/2;Point q{c.x+std::cos(a)*rad,c.y+std::sin(a)*rad};r.FillRoundedRect({q.x-2,q.y-2,4,4},CornerRadius::Uniform(2),i<active?p.accent:p.controlStroke);}}

bool TextBox::SetXamlProperty(std::string_view n,std::string_view v){if(Control::SetXamlProperty(n,v))return true;if(n=="Text"){text_=std::string(v);return true;}if(n=="PlaceholderText"||n=="Placeholder"){placeholder_=std::string(v);return true;}return false;}
Size TextBox::MeasureOverride(const Size&a){return {std::min(a.width,260.0f),34};}
void TextBox::OnPointerPressed(PointerEventArgs&a){Control::OnPointerPressed(a);SetFocusState(FocusState::Keyboard);}
void TextBox::OnKey(KeyEventArgs&a){if(a.pressed){if(a.key==8||a.key==0xff08){if(!text_.empty())text_.pop_back();}else if(!a.text.empty())text_+=a.text;}Control::OnKey(a);}
void TextBox::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();r.FillRoundedRect(bounds_,CornerRadius::Uniform(4),p.controlFillDefault);r.StrokeRoundedRect(bounds_,CornerRadius::Uniform(4),Focus()==FocusState::Keyboard?p.accent:p.controlStroke,Focus()==FocusState::Keyboard?2:1);auto text=text_.empty()?placeholder_:text_;r.DrawText(text,{bounds_.x+10,bounds_.y,bounds_.width-20,bounds_.height},text_.empty()?p.textSecondary:p.textPrimary,14);}
void SearchBox::RenderSelf(Renderer&r){TextBox::RenderSelf(r);auto&p=ThemeManager::Current().Palette();r.DrawText("⌕",{bounds_.x+bounds_.width-28,bounds_.y,22,bounds_.height},p.textSecondary,15,FontWeight::Normal,TextAlignment::Center);}

bool ComboBox::SetXamlProperty(std::string_view n,std::string_view v){if(Control::SetXamlProperty(n,v))return true;if(n=="Items"){SetItems(SplitList(v));return true;}if(n=="SelectedIndex"){SetSelectedIndex(ParseInt(v));return true;}return false;}
Size ComboBox::MeasureOverride(const Size&a){return {std::min(a.width,220.0f),34};}
void ComboBox::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();r.FillRoundedRect(bounds_,CornerRadius::Uniform(4),BackgroundForState());r.StrokeRoundedRect(bounds_,CornerRadius::Uniform(4),p.controlStroke,1);std::string t=items_.empty()?"Choose":items_[selected_];r.DrawText(t,{bounds_.x+10,bounds_.y,bounds_.width-36,bounds_.height},p.textPrimary,14);r.DrawText("⌄",{bounds_.x+bounds_.width-28,bounds_.y,22,bounds_.height},p.textSecondary,14,FontWeight::Normal,TextAlignment::Center);}

bool ListView::SetXamlProperty(std::string_view n,std::string_view v){if(Control::SetXamlProperty(n,v))return true;if(n=="Items"){SetItems(SplitList(v));return true;}return false;}
Size ListView::MeasureOverride(const Size&a){return {std::min(a.width,300.0f),std::min(a.height,std::max(44.0f,float(items_.size())*34+12))};}
void ListView::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();r.FillRoundedRect(bounds_,CornerRadius::Uniform(6),p.layerFillAlt);float y=bounds_.y+6;for(std::size_t i=0;i<items_.size()&&y+32<bounds_.y+bounds_.height;i++){Rect row{bounds_.x+6,y,bounds_.width-12,30};if(i==0)r.FillRoundedRect(row,CornerRadius::Uniform(4),Color::Lerp(p.accent,p.layerFill,.80f));r.DrawText(items_[i],{row.x+10,row.y,row.width-20,row.height},p.textPrimary,13);y+=34;}}

bool TabView::SetXamlProperty(std::string_view n,std::string_view v){if(Control::SetXamlProperty(n,v))return true;if(n=="Tabs"){SetTabs(SplitList(v));return true;}return false;}
Size TabView::MeasureOverride(const Size&a){return {std::min(a.width,360.0f),42};}
void TabView::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();float x=bounds_.x;for(std::size_t i=0;i<tabs_.size();++i){float w=std::max(80.0f,float(tabs_[i].size())*8+28);Rect tab{x,bounds_.y,w,bounds_.height};r.FillRoundedRect(tab,CornerRadius::Uniform(4),i==size_t(selected_)?p.controlFillHover:p.layerFill);r.DrawText(tabs_[i],tab,p.textPrimary,13,FontWeight::Normal,TextAlignment::Center);if(i==size_t(selected_))r.FillRoundedRect({tab.x+14,tab.y+tab.height-3,tab.width-28,3},CornerRadius::Uniform(2),p.accent);x+=w+4;}}

bool NavigationView::SetXamlProperty(std::string_view n,std::string_view v){if(Control::SetXamlProperty(n,v))return true;if(n=="Header"){header_=std::string(v);return true;}if(n=="Items"){SetItems(SplitList(v));return true;}return false;}
Size NavigationView::MeasureOverride(const Size&a){return {std::min(a.width,260.0f),a.height};}
void NavigationView::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();r.FillRect(bounds_,p.layerFill);r.DrawText(header_,{bounds_.x+20,bounds_.y+16,bounds_.width-40,36},p.textPrimary,18,FontWeight::SemiBold);float y=bounds_.y+66;for(std::size_t i=0;i<items_.size();++i){Rect row{bounds_.x+10,y,bounds_.width-20,38};if(i==size_t(selected_)){r.FillRoundedRect(row,CornerRadius::Uniform(4),p.controlFillHover);r.FillRoundedRect({row.x,row.y+8,3,row.height-16},CornerRadius::Uniform(2),p.accent);}r.DrawText(items_[i],{row.x+16,row.y,row.width-22,row.height},i==size_t(selected_)?p.textPrimary:p.textSecondary,14);y+=42;}}

bool InfoBar::SetXamlProperty(std::string_view n,std::string_view v){if(ContentControl::SetXamlProperty(n,v))return true;if(n=="Title"){title_=std::string(v);return true;}if(n=="Severity"){auto s=Lower(v);severity_=s=="success"?Severity::Success:s=="warning"?Severity::Warning:s=="error"?Severity::Error:Severity::Informational;return true;}return false;}
Size InfoBar::MeasureOverride(const Size&a){return {a.width,72};}
void InfoBar::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();auto c=SeverityColor(severity_);r.FillRoundedRect(bounds_,CornerRadius::Uniform(6),Color::Lerp(c,p.layerFill,.86f));r.StrokeRoundedRect(bounds_,CornerRadius::Uniform(6),Color::Lerp(c,p.cardStroke,.30f),1);r.FillRoundedRect({bounds_.x+12,bounds_.y+17,6,38},CornerRadius::Uniform(3),c);r.DrawText(title_,{bounds_.x+30,bounds_.y+8,bounds_.width-42,26},p.textPrimary,14,FontWeight::SemiBold);r.DrawText(content_,{bounds_.x+30,bounds_.y+32,bounds_.width-42,26},p.textSecondary,13);}
Size Badge::MeasureOverride(const Size&){return {std::max(24.0f,float(content_.size())*7+16),24};}
void Badge::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();r.FillRoundedRect(bounds_,CornerRadius::Uniform(12),p.accent);r.DrawText(content_,bounds_,p.textOnAccent,12,FontWeight::SemiBold,TextAlignment::Center);}

bool Card::SetXamlProperty(std::string_view n,std::string_view v){if(ContentControl::SetXamlProperty(n,v))return true;if(n=="Title"){title_=std::string(v);return true;}if(n=="Subtitle"){subtitle_=std::string(v);return true;}return false;}
Size Card::MeasureOverride(const Size&a){return {std::min(a.width,360.0f),128};}
void Card::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();r.FillRoundedRect(bounds_,CornerRadius::Uniform(8),p.cardFill);r.StrokeRoundedRect(bounds_,CornerRadius::Uniform(8),p.cardStroke,1);r.DrawText(title_,{bounds_.x+18,bounds_.y+15,bounds_.width-36,28},p.textPrimary,16,FontWeight::SemiBold);r.DrawText(subtitle_,{bounds_.x+18,bounds_.y+43,bounds_.width-36,24},p.textSecondary,13);r.DrawText(content_,{bounds_.x+18,bounds_.y+72,bounds_.width-36,bounds_.height-86},p.textPrimary,14);}
bool SettingsCard::SetXamlProperty(std::string_view n,std::string_view v){if(Card::SetXamlProperty(n,v))return true;if(n=="Description"){description_=std::string(v);return true;}return false;}
void SettingsCard::RenderSelf(Renderer&r){Card::RenderSelf(r);auto&p=ThemeManager::Current().Palette();r.DrawText(description_,{bounds_.x+18,bounds_.y+bounds_.height-34,bounds_.width-36,24},p.textSecondary,12);}

bool BreadcrumbBar::SetXamlProperty(std::string_view n,std::string_view v){if(Control::SetXamlProperty(n,v))return true;if(n=="Items"){SetItems(SplitList(v));return true;}return false;}
Size BreadcrumbBar::MeasureOverride(const Size&a){return {a.width,32};}
void BreadcrumbBar::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();float x=bounds_.x;for(std::size_t i=0;i<items_.size();++i){float w=float(items_[i].size())*7.5f+8;r.DrawText(items_[i],{x,bounds_.y,w,bounds_.height},i+1==items_.size()?p.textPrimary:p.textSecondary,13);x+=w;if(i+1<items_.size()){r.DrawText("›",{x,bounds_.y,18,bounds_.height},p.textSecondary,15,FontWeight::Normal,TextAlignment::Center);x+=20;}}}

bool Expander::SetXamlProperty(std::string_view n,std::string_view v){if(ContentControl::SetXamlProperty(n,v))return true;if(n=="Header"){header_=std::string(v);return true;}if(n=="IsExpanded"){expanded_=ParseBool(v,true);return true;}return false;}
Size Expander::MeasureOverride(const Size&a){return {a.width,expanded_?92.0f:40.0f};}
void Expander::OnPointerReleased(PointerEventArgs&a){bool click=State()==ControlState::Pressed&&bounds_.Contains(a.position);Control::OnPointerReleased(a);if(click)expanded_=!expanded_;}
void Expander::RenderSelf(Renderer&r){auto&p=ThemeManager::Current().Palette();r.FillRoundedRect(bounds_,CornerRadius::Uniform(6),p.layerFillAlt);r.StrokeRoundedRect(bounds_,CornerRadius::Uniform(6),p.cardStroke,1);r.DrawText(expanded_?"⌄":"›",{bounds_.x+10,bounds_.y,24,40},p.textSecondary,15,FontWeight::Normal,TextAlignment::Center);r.DrawText(header_,{bounds_.x+38,bounds_.y,bounds_.width-48,40},p.textPrimary,14,FontWeight::Medium);if(expanded_)r.DrawText(content_,{bounds_.x+18,bounds_.y+42,bounds_.width-36,42},p.textSecondary,13);}
Size Separator::MeasureOverride(const Size&a){return {a.width,1};}
void Separator::RenderSelf(Renderer&r){r.FillRect(bounds_,ThemeManager::Current().Palette().cardStroke);}

Size ScrollViewer::MeasureOverride(const Size& available){if(children_.empty())return available;children_[0]->Measure({available.width,kInf});return {available.width,std::min(available.height,children_[0]->DesiredSize().height)};}
Size ScrollViewer::ArrangeOverride(const Size& finalSize){if(!children_.empty())children_[0]->Arrange({bounds_.x,bounds_.y-offset_,finalSize.width,children_[0]->DesiredSize().height});return finalSize;}
void ScrollViewer::Render(Renderer&r){if(GetVisibility()==Visibility::Collapsed)return;r.PushClip(bounds_);for(auto&c:children_)c->Render(r);r.PopClip();}

// -----------------------------------------------------------------------------
// Binding / XAML bridge
// -----------------------------------------------------------------------------

BindingExpression::BindingExpression(std::weak_ptr<DependencyObject>s,const DependencyProperty*sp,std::weak_ptr<DependencyObject>t,const DependencyProperty*tp,BindingMode m):source_(std::move(s)),sourceProperty_(sp),target_(std::move(t)),targetProperty_(tp),mode_(m){}
void BindingExpression::UpdateTarget(){auto s=source_.lock(),t=target_.lock();if(s&&t)t->SetValue(targetProperty_,s->GetValue(sourceProperty_));}
void BindingExpression::UpdateSource(){if(mode_!=BindingMode::TwoWay)return;auto s=source_.lock(),t=target_.lock();if(s&&t)s->SetValue(sourceProperty_,t->GetValue(targetProperty_));}

bool XamlEventBinder::Invoke(std::string_view name, UIElement& sender) const {auto it=handlers_.find(std::string(name));if(it==handlers_.end())return false;it->second(sender);return true;}

std::shared_ptr<UIElement> CreateXamlElement(std::string_view raw) {
    auto pos=raw.rfind(':'); auto type=std::string(pos==std::string_view::npos?raw:raw.substr(pos+1));
    if(type=="Window")return std::make_shared<Window>();
    if(type=="Grid")return std::make_shared<Grid>();
    if(type=="StackPanel")return std::make_shared<StackPanel>();
    if(type=="Canvas")return std::make_shared<Canvas>();
    if(type=="Border")return std::make_shared<Border>();
    if(type=="TextBlock")return std::make_shared<TextBlock>();
    if(type=="Button")return std::make_shared<Button>();
    if(type=="AccentButton")return std::make_shared<AccentButton>();
    if(type=="HyperlinkButton")return std::make_shared<HyperlinkButton>();
    if(type=="ToggleSwitch")return std::make_shared<ToggleSwitch>();
    if(type=="CheckBox")return std::make_shared<CheckBox>();
    if(type=="RadioButton")return std::make_shared<RadioButton>();
    if(type=="Slider")return std::make_shared<Slider>();
    if(type=="ProgressBar")return std::make_shared<ProgressBar>();
    if(type=="ProgressRing")return std::make_shared<ProgressRing>();
    if(type=="TextBox")return std::make_shared<TextBox>();
    if(type=="SearchBox")return std::make_shared<SearchBox>();
    if(type=="ComboBox")return std::make_shared<ComboBox>();
    if(type=="ListView")return std::make_shared<ListView>();
    if(type=="TabView")return std::make_shared<TabView>();
    if(type=="NavigationView")return std::make_shared<NavigationView>();
    if(type=="InfoBar")return std::make_shared<InfoBar>();
    if(type=="Badge")return std::make_shared<Badge>();
    if(type=="Card")return std::make_shared<Card>();
    if(type=="SettingsCard")return std::make_shared<SettingsCard>();
    if(type=="BreadcrumbBar")return std::make_shared<BreadcrumbBar>();
    if(type=="Expander")return std::make_shared<Expander>();
    if(type=="Separator")return std::make_shared<Separator>();
    if(type=="ScrollViewer")return std::make_shared<ScrollViewer>();
    if(type=="Tooltip")return std::make_shared<Tooltip>();
    if(type=="ContextMenu")return std::make_shared<ContextMenu>();
    if(type=="ContentDialog")return std::make_shared<ContentDialog>();
    return nullptr;
}

void ApplyXamlAttribute(const std::shared_ptr<UIElement>& element,std::string_view name,std::string_view value,XamlEventBinder* binder){if(!element)return;if(name.rfind("xmlns",0)==0||name=="x:Class")return;if((name=="Click"||name=="Toggled"||name=="SelectionChanged")&&binder){element->BindXamlEvent(name,value,*binder);return;}element->SetXamlProperty(name,value);}

// -----------------------------------------------------------------------------
// Window
// -----------------------------------------------------------------------------

bool Window::SetXamlProperty(std::string_view n,std::string_view v){if(ContentControl::SetXamlProperty(n,v))return true;if(n=="Title"){title_=std::string(v);return true;}if(n=="Width"){initialWidth_=ParseInt(v,1280);return true;}if(n=="Height"){initialHeight_=ParseInt(v,800);return true;}return false;}
Size Window::MeasureOverride(const Size& available){if(children_.empty())return available;children_[0]->Measure(available);return available;}
Size Window::ArrangeOverride(const Size& finalSize){if(!children_.empty())children_[0]->Arrange(bounds_);return finalSize;}

} // namespace keshui
