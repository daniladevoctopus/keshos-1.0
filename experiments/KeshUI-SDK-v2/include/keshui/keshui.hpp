#pragma once

#include <algorithm>
#include <any>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace keshui {

// -----------------------------------------------------------------------------
// Geometry / primitives
// -----------------------------------------------------------------------------

struct Color {
    float r{0}, g{0}, b{0}, a{1};
    static Color FromHex(std::string_view hex);
    static Color Lerp(Color a, Color b, float t);
};

struct Point { float x{0}, y{0}; };
struct Size { float width{0}, height{0}; };
struct Rect {
    float x{0}, y{0}, width{0}, height{0};
    bool Contains(Point p) const { return p.x >= x && p.y >= y && p.x < x + width && p.y < y + height; }
};
struct Thickness {
    float left{0}, top{0}, right{0}, bottom{0};
    static Thickness Uniform(float v) { return {v,v,v,v}; }
};
struct CornerRadius {
    float topLeft{0}, topRight{0}, bottomRight{0}, bottomLeft{0};
    static CornerRadius Uniform(float v) { return {v,v,v,v}; }
};

enum class Orientation { Horizontal, Vertical };
enum class HorizontalAlignment { Left, Center, Right, Stretch };
enum class VerticalAlignment { Top, Center, Bottom, Stretch };
enum class Visibility { Visible, Collapsed };
enum class FontWeight { Normal = 400, Medium = 500, SemiBold = 600, Bold = 700 };
enum class TextAlignment { Left, Center, Right };
enum class FocusState { Unfocused, Pointer, Keyboard };
enum class ThemeKind { Light, Dark, HighContrast };
enum class BindingMode { OneTime, OneWay, TwoWay };

// -----------------------------------------------------------------------------
// Renderer abstraction: X11/Cairo today, Prism later.
// -----------------------------------------------------------------------------

class Renderer {
public:
    virtual ~Renderer() = default;
    virtual void BeginFrame(int width, int height, Color clear) = 0;
    virtual void EndFrame() = 0;
    virtual void FillRect(const Rect& rect, Color color) = 0;
    virtual void FillRoundedRect(const Rect& rect, const CornerRadius& radius, Color color) = 0;
    virtual void StrokeRoundedRect(const Rect& rect, const CornerRadius& radius, Color color, float width) = 0;
    virtual void DrawLine(Point a, Point b, Color color, float width) = 0;
    virtual void DrawText(std::string_view text, const Rect& rect, Color color,
                          float size, FontWeight weight = FontWeight::Normal,
                          TextAlignment alignment = TextAlignment::Left) = 0;
    virtual Size MeasureText(std::string_view text, float size,
                             FontWeight weight = FontWeight::Normal) = 0;
    virtual void PushClip(const Rect& rect) = 0;
    virtual void PopClip() = 0;
};

// -----------------------------------------------------------------------------
// Resources and theme tokens
// -----------------------------------------------------------------------------

struct ResourceReference {
    enum class Kind { Static, Theme };
    Kind kind{Kind::Static};
    std::string key;
};

class ResourceDictionary {
public:
    void Set(std::string key, std::any value) { values_[std::move(key)] = std::move(value); }
    const std::any* TryGet(std::string_view key) const;
    void SetThemeDictionary(ThemeKind theme, std::shared_ptr<ResourceDictionary> dictionary);
    const std::any* TryGetTheme(std::string_view key, ThemeKind theme) const;
private:
    std::unordered_map<std::string, std::any> values_;
    std::unordered_map<int, std::shared_ptr<ResourceDictionary>> themes_;
};

struct DesignTokens {
    // 20 explicit design-system dimensions.
    float controlCornerRadius{4};
    float overlayCornerRadius{8};
    float controlHeight{32};
    float compactControlHeight{28};
    float touchControlHeight{40};
    float space2{2}, space4{4}, space8{8}, space12{12}, space16{16}, space24{24}, space32{32};
    float textCaption{12}, textBody{14}, textSubtitle{16}, textTitle{20}, textDisplay{28};
    float focusStroke{2};
    float elevationLow{1};
    float elevationMedium{6};
    float elevationHigh{16};
    float motionFastMs{83};
    float motionNormalMs{167};
    float motionSlowMs{250};
};

struct ThemePalette {
    Color applicationBackground;
    Color layerFill;
    Color layerFillAlt;
    Color cardFill;
    Color cardStroke;
    Color controlFillDefault;
    Color controlFillHover;
    Color controlFillPressed;
    Color controlFillDisabled;
    Color controlStroke;
    Color controlStrokeStrong;
    Color textPrimary;
    Color textSecondary;
    Color textDisabled;
    Color accent;
    Color accentHover;
    Color accentPressed;
    Color textOnAccent;
    Color danger;
    Color success;
    Color warning;
    Color focus;
};

class ThemeManager {
public:
    static ThemeManager& Current();
    ThemeKind Theme() const { return theme_; }
    void SetTheme(ThemeKind theme);
    void SetAccent(Color accent);
    const ThemePalette& Palette() const { return palette_; }
    const DesignTokens& Tokens() const { return tokens_; }
    Color ResolveColor(std::string_view key) const;
    std::any Resolve(const ResourceReference& ref) const;
private:
    ThemeManager();
    void Rebuild();
    ThemeKind theme_{ThemeKind::Dark};
    Color accent_{Color::FromHex("#F28A22")};
    ThemePalette palette_{};
    DesignTokens tokens_{};
};

// -----------------------------------------------------------------------------
// WinUI-like dependency property system.
// Effective value precedence: animated > local > style > inherited > default.
// -----------------------------------------------------------------------------

class DependencyObject;
using PropertyChangedCallback = std::function<void(DependencyObject&, const std::any&, const std::any&)>;

struct PropertyMetadata {
    std::any defaultValue;
    PropertyChangedCallback changed;
    bool inherits{false};
};

class DependencyProperty {
public:
    const std::string& Name() const { return name_; }
    std::type_index Type() const { return type_; }
    std::type_index OwnerType() const { return owner_; }
    const PropertyMetadata& Metadata() const { return metadata_; }
    std::size_t Id() const { return id_; }

    template<typename Owner, typename T>
    static const DependencyProperty* Register(std::string name, T defaultValue,
                                               PropertyChangedCallback changed = {},
                                               bool inherits = false) {
        PropertyMetadata metadata{std::any(std::move(defaultValue)), std::move(changed), inherits};
        return RegisterImpl(std::move(name), typeid(T), typeid(Owner), std::move(metadata));
    }
private:
    DependencyProperty(std::string name, std::type_index type, std::type_index owner,
                       PropertyMetadata metadata, std::size_t id)
        : name_(std::move(name)), type_(type), owner_(owner), metadata_(std::move(metadata)), id_(id) {}
    static const DependencyProperty* RegisterImpl(std::string, std::type_index, std::type_index, PropertyMetadata);
    std::string name_;
    std::type_index type_{typeid(void)};
    std::type_index owner_{typeid(void)};
    PropertyMetadata metadata_;
    std::size_t id_{0};
};

class DependencyObject {
public:
    virtual ~DependencyObject() = default;

    std::any GetValue(const DependencyProperty* property) const;
    void SetValue(const DependencyProperty* property, std::any value);
    void ClearValue(const DependencyProperty* property);
    void SetStyleValue(const DependencyProperty* property, std::any value);
    void SetAnimatedValue(const DependencyProperty* property, std::any value);
    void ClearAnimatedValue(const DependencyProperty* property);

    template<typename T>
    T Get(const DependencyProperty* property) const {
        auto value = GetValue(property);
        if (auto ref = std::any_cast<ResourceReference>(&value)) value = ThemeManager::Current().Resolve(*ref);
        if (const auto* typed = std::any_cast<T>(&value)) return *typed;
        return T{};
    }

    template<typename T>
    void Set(const DependencyProperty* property, T value) { SetValue(property, std::any(std::move(value))); }

    void SetInheritanceParent(std::weak_ptr<DependencyObject> parent) { inheritanceParent_ = std::move(parent); }

protected:
    virtual void OnPropertyChanged(const DependencyProperty*, const std::any&, const std::any&) {}

private:
    std::unordered_map<std::size_t, std::any> localValues_;
    std::unordered_map<std::size_t, std::any> styleValues_;
    std::unordered_map<std::size_t, std::any> animatedValues_;
    std::weak_ptr<DependencyObject> inheritanceParent_;
};

struct Setter {
    const DependencyProperty* property{};
    std::any value;
};

class Style {
public:
    void SetBasedOn(std::shared_ptr<Style> style) { basedOn_ = std::move(style); }
    void Add(Setter setter) { setters_.push_back(std::move(setter)); }
    void Apply(DependencyObject& object) const;
private:
    std::shared_ptr<Style> basedOn_;
    std::vector<Setter> setters_;
};

// -----------------------------------------------------------------------------
// Routed events
// -----------------------------------------------------------------------------

class UIElement;

struct RoutedEventArgs {
    UIElement* source{};
    UIElement* originalSource{};
    bool handled{false};
};

struct PointerEventArgs : RoutedEventArgs {
    Point position{};
    int button{0};
    bool pressed{false};
};

struct KeyEventArgs : RoutedEventArgs {
    std::uint32_t key{0};
    bool pressed{false};
    std::string text;
};

using RoutedHandler = std::function<void(RoutedEventArgs&)>;

// -----------------------------------------------------------------------------
// UI tree and layout
// -----------------------------------------------------------------------------

class UIElement : public DependencyObject, public std::enable_shared_from_this<UIElement> {
public:
    virtual ~UIElement() = default;

    void AddChild(std::shared_ptr<UIElement> child);
    void RemoveChild(const std::shared_ptr<UIElement>& child);
    const std::vector<std::shared_ptr<UIElement>>& Children() const { return children_; }
    std::shared_ptr<UIElement> Parent() const { return parent_.lock(); }

    void SetName(std::string name) { name_ = std::move(name); }
    const std::string& Name() const { return name_; }
    std::shared_ptr<UIElement> FindName(std::string_view name);

    virtual void Measure(const Size& available);
    virtual void Arrange(const Rect& finalRect);
    virtual void Render(Renderer& renderer);
    virtual std::shared_ptr<UIElement> HitTest(Point p);

    const Size& DesiredSize() const { return desiredSize_; }
    const Rect& Bounds() const { return bounds_; }

    void SetVisibility(Visibility v) { visibility_ = v; }
    Visibility GetVisibility() const { return visibility_; }
    void SetHitTestVisible(bool v) { hitTestVisible_ = v; }
    bool IsHitTestVisible() const { return hitTestVisible_; }

    void AddHandler(std::string eventName, RoutedHandler handler);
    void RaiseEvent(std::string_view eventName, RoutedEventArgs& args, bool bubble = true);

    virtual void OnPointerMove(PointerEventArgs& args);
    virtual void OnPointerPressed(PointerEventArgs& args);
    virtual void OnPointerReleased(PointerEventArgs& args);
    virtual void OnKey(KeyEventArgs& args);

    virtual bool SetXamlProperty(std::string_view name, std::string_view value);
    virtual bool BindXamlEvent(std::string_view eventName, std::string_view handlerName, class XamlEventBinder& binder);

    void SetAttached(std::string key, std::string value) { attached_[std::move(key)] = std::move(value); }
    std::string Attached(std::string_view key, std::string_view fallback = {}) const;

    virtual bool WantsContinuousFrame() const;

protected:
    virtual Size MeasureOverride(const Size& available);
    virtual Size ArrangeOverride(const Size& finalSize);
    virtual void RenderSelf(Renderer&) {}

    std::vector<std::shared_ptr<UIElement>> children_;
    Rect bounds_{};
    Size desiredSize_{};

private:
    std::weak_ptr<UIElement> parent_;
    std::string name_;
    Visibility visibility_{Visibility::Visible};
    bool hitTestVisible_{true};
    std::unordered_map<std::string, std::vector<RoutedHandler>> handlers_;
    std::unordered_map<std::string, std::string> attached_;
};

class FrameworkElement : public UIElement {
public:
    void SetWidth(float v) { width_ = v; }
    void SetHeight(float v) { height_ = v; }
    void SetMinWidth(float v) { minWidth_ = v; }
    void SetMinHeight(float v) { minHeight_ = v; }
    void SetMaxWidth(float v) { maxWidth_ = v; }
    void SetMaxHeight(float v) { maxHeight_ = v; }
    void SetMargin(Thickness v) { margin_ = v; }
    const Thickness& Margin() const { return margin_; }
    void SetHorizontalAlignment(HorizontalAlignment v) { horizontalAlignment_ = v; }
    void SetVerticalAlignment(VerticalAlignment v) { verticalAlignment_ = v; }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;

    void Measure(const Size& available) override;
    void Arrange(const Rect& finalRect) override;

protected:
    float width_{-1};
    float height_{-1};
    float minWidth_{0};
    float minHeight_{0};
    float maxWidth_{1000000};
    float maxHeight_{1000000};
    Thickness margin_{};
    HorizontalAlignment horizontalAlignment_{HorizontalAlignment::Stretch};
    VerticalAlignment verticalAlignment_{VerticalAlignment::Stretch};
};

class Panel : public FrameworkElement {};

class StackPanel : public Panel {
public:
    void SetOrientation(Orientation v) { orientation_ = v; }
    void SetSpacing(float v) { spacing_ = v; }
    void SetPadding(Thickness v) { padding_ = v; }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    Size ArrangeOverride(const Size& finalSize) override;
private:
    Orientation orientation_{Orientation::Vertical};
    float spacing_{0};
    Thickness padding_{};
};

class Grid : public Panel {
public:
    struct Track { enum class Kind { Pixel, Auto, Star }; Kind kind{Kind::Star}; float value{1}; };
    void SetColumns(std::string_view spec);
    void SetRows(std::string_view spec);
    void SetColumnSpacing(float v) { columnSpacing_ = v; }
    void SetRowSpacing(float v) { rowSpacing_ = v; }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    Size ArrangeOverride(const Size& finalSize) override;
private:
    std::vector<Track> columns_{{Track::Kind::Star,1}};
    std::vector<Track> rows_{{Track::Kind::Star,1}};
    float columnSpacing_{0};
    float rowSpacing_{0};
};

class Canvas : public Panel {
protected:
    Size MeasureOverride(const Size& available) override;
    Size ArrangeOverride(const Size& finalSize) override;
};

class Border : public FrameworkElement {
public:
    void SetBackground(Color c) { background_ = c; }
    void SetBorderBrush(Color c) { borderBrush_ = c; }
    void SetBorderThickness(float v) { borderThickness_ = v; }
    void SetCornerRadius(float v) { cornerRadius_ = CornerRadius::Uniform(v); }
    void SetPadding(Thickness v) { padding_ = v; }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    Size ArrangeOverride(const Size& finalSize) override;
    void RenderSelf(Renderer& renderer) override;
private:
    Color background_{0,0,0,0};
    Color borderBrush_{0,0,0,0};
    float borderThickness_{0};
    CornerRadius cornerRadius_{};
    Thickness padding_{};
};

// -----------------------------------------------------------------------------
// Controls, templates and VSM
// -----------------------------------------------------------------------------

enum class ControlState { Normal, PointerOver, Pressed, Disabled };

class Control;
using TemplateFactory = std::function<std::shared_ptr<UIElement>(Control&)>;

class ControlTemplate {
public:
    explicit ControlTemplate(TemplateFactory factory) : factory_(std::move(factory)) {}
    std::shared_ptr<UIElement> Build(Control& control) const { return factory_ ? factory_(control) : nullptr; }
private:
    TemplateFactory factory_;
};

struct VisualStateSetter {
    const DependencyProperty* property{};
    std::any value;
};

struct VisualState {
    std::string name;
    std::vector<VisualStateSetter> setters;
};

class VisualStateManager {
public:
    static bool GoToState(Control& control, std::string_view state, bool useTransitions = true);
};

class Control : public FrameworkElement {
public:
    static const DependencyProperty* BackgroundProperty();
    static const DependencyProperty* ForegroundProperty();
    static const DependencyProperty* BorderBrushProperty();
    static const DependencyProperty* CornerRadiusProperty();
    static const DependencyProperty* IsEnabledProperty();

    void SetStyle(std::shared_ptr<Style> style);
    void SetTemplate(std::shared_ptr<ControlTemplate> templ) { template_ = std::move(templ); }
    ControlState State() const { return state_; }
    bool IsEnabled() const { return Get<bool>(IsEnabledProperty()); }
    void SetEnabled(bool v);
    FocusState Focus() const { return focusState_; }
    void SetFocusState(FocusState s) { focusState_ = s; }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;

    void OnPointerMove(PointerEventArgs& args) override;
    void OnPointerPressed(PointerEventArgs& args) override;
    void OnPointerReleased(PointerEventArgs& args) override;

    std::string CurrentVisualStateName() const;

protected:
    void SetState(ControlState state);
    Color BackgroundForState() const;
    Color ForegroundForState() const;
    Color BorderForState() const;
    void RenderFocus(Renderer& renderer, const Rect& rect, float radius);

private:
    std::shared_ptr<Style> style_;
    std::shared_ptr<ControlTemplate> template_;
    ControlState state_{ControlState::Normal};
    FocusState focusState_{FocusState::Unfocused};
};

class ContentControl : public Control {
public:
    void SetContent(std::string value) { content_ = std::move(value); }
    const std::string& Content() const { return content_; }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    std::string content_;
};

class TextBlock : public FrameworkElement {
public:
    void SetText(std::string v) { text_ = std::move(v); }
    void SetFontSize(float v) { fontSize_ = v; }
    void SetFontWeight(FontWeight v) { fontWeight_ = v; }
    void SetForeground(Color v) { foreground_ = v; }
    void SetTextAlignment(TextAlignment v) { textAlignment_ = v; }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    std::string text_;
    float fontSize_{14};
    FontWeight fontWeight_{FontWeight::Normal};
    Color foreground_{};
    bool useThemeForeground_{true};
    TextAlignment textAlignment_{TextAlignment::Left};
};

class Button : public ContentControl {
public:
    Button();
    void OnClick(std::function<void()> callback) { click_ = std::move(callback); }
    void OnPointerReleased(PointerEventArgs& args) override;
    bool BindXamlEvent(std::string_view eventName, std::string_view handlerName, XamlEventBinder& binder) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
    std::function<void()> click_;
};

class AccentButton : public Button {
protected:
    void RenderSelf(Renderer& renderer) override;
};

class HyperlinkButton : public Button {
protected:
    void RenderSelf(Renderer& renderer) override;
};

class ToggleSwitch : public Control {
public:
    void SetHeader(std::string v) { header_ = std::move(v); }
    void SetOn(bool v) { on_ = v; }
    bool IsOn() const { return on_; }
    void OnToggled(std::function<void(bool)> cb) { toggled_ = std::move(cb); }
    void OnPointerReleased(PointerEventArgs& args) override;
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    bool on_{false};
    std::string header_;
    std::function<void(bool)> toggled_;
};

class CheckBox : public Control {
public:
    void SetContent(std::string v) { content_ = std::move(v); }
    void SetChecked(bool v) { checked_ = v; }
    bool IsChecked() const { return checked_; }
    void OnPointerReleased(PointerEventArgs& args) override;
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    bool checked_{false};
    std::string content_;
};

class RadioButton : public CheckBox {
protected:
    void RenderSelf(Renderer& renderer) override;
};

class Slider : public Control {
public:
    void SetMinimum(double v) { minimum_ = v; }
    void SetMaximum(double v) { maximum_ = v; }
    void SetValue(double v);
    double Value() const { return value_; }
    void OnPointerPressed(PointerEventArgs& args) override;
    void OnPointerMove(PointerEventArgs& args) override;
    void OnPointerReleased(PointerEventArgs& args) override;
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    void SetFromPointer(Point p);
    double minimum_{0}, maximum_{100}, value_{50};
    bool dragging_{false};
};

class ProgressBar : public Control {
public:
    void SetValue(double v) { value_ = std::clamp(v, 0.0, 100.0); }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    double value_{42};
};

class ProgressRing : public Control {
public:
    void SetValue(double v) { value_ = std::clamp(v, 0.0, 100.0); }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    double value_{68};
};

class TextBox : public Control {
public:
    void SetText(std::string v) { text_ = std::move(v); }
    void SetPlaceholder(std::string v) { placeholder_ = std::move(v); }
    const std::string& Text() const { return text_; }
    void OnKey(KeyEventArgs& args) override;
    void OnPointerPressed(PointerEventArgs& args) override;
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    std::string text_;
    std::string placeholder_;
};

class SearchBox : public TextBox {
protected:
    void RenderSelf(Renderer& renderer) override;
};

class ComboBox : public Control {
public:
    void SetItems(std::vector<std::string> items) { items_ = std::move(items); }
    void SetSelectedIndex(int i) { selected_ = std::clamp(i, 0, std::max(0, int(items_.size()) - 1)); }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    std::vector<std::string> items_;
    int selected_{0};
};

class ListView : public Control {
public:
    void SetItems(std::vector<std::string> items) { items_ = std::move(items); }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    std::vector<std::string> items_;
};

class TabView : public Control {
public:
    void SetTabs(std::vector<std::string> tabs) { tabs_ = std::move(tabs); }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    std::vector<std::string> tabs_;
    int selected_{0};
};

class NavigationView : public Control {
public:
    void SetHeader(std::string v) { header_ = std::move(v); }
    void SetItems(std::vector<std::string> items) { items_ = std::move(items); }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    std::string header_{"KeshUI"};
    std::vector<std::string> items_;
    int selected_{0};
};

class InfoBar : public ContentControl {
public:
    enum class Severity { Informational, Success, Warning, Error };
    void SetTitle(std::string v) { title_ = std::move(v); }
    void SetSeverity(Severity v) { severity_ = v; }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    std::string title_{"Information"};
    Severity severity_{Severity::Informational};
};

class Badge : public ContentControl {
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
};

class Card : public ContentControl {
public:
    void SetTitle(std::string v) { title_ = std::move(v); }
    void SetSubtitle(std::string v) { subtitle_ = std::move(v); }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    std::string title_;
    std::string subtitle_;
};

class SettingsCard : public Card {
public:
    void SetDescription(std::string v) { description_ = std::move(v); }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    void RenderSelf(Renderer& renderer) override;
private:
    std::string description_;
};

class BreadcrumbBar : public Control {
public:
    void SetItems(std::vector<std::string> items) { items_ = std::move(items); }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    std::vector<std::string> items_;
};

class Expander : public ContentControl {
public:
    void SetHeader(std::string v) { header_ = std::move(v); }
    void SetExpanded(bool v) { expanded_ = v; }
    void OnPointerReleased(PointerEventArgs& args) override;
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
private:
    std::string header_;
    bool expanded_{true};
};

class Separator : public FrameworkElement {
protected:
    Size MeasureOverride(const Size& available) override;
    void RenderSelf(Renderer& renderer) override;
};

class ScrollViewer : public ContentControl {
public:
    void SetOffset(float v) { offset_ = std::max(0.0f, v); }
protected:
    Size MeasureOverride(const Size& available) override;
    Size ArrangeOverride(const Size& finalSize) override;
    void Render(Renderer& renderer) override;
private:
    float offset_{0};
};

class Tooltip : public ContentControl {};
class ContextMenu : public StackPanel {};
class ContentDialog : public ContentControl {};

// -----------------------------------------------------------------------------
// Binding foundation
// -----------------------------------------------------------------------------

class BindingExpression {
public:
    BindingExpression(std::weak_ptr<DependencyObject> source, const DependencyProperty* sourceProperty,
                      std::weak_ptr<DependencyObject> target, const DependencyProperty* targetProperty,
                      BindingMode mode);
    void UpdateTarget();
    void UpdateSource();
private:
    std::weak_ptr<DependencyObject> source_;
    const DependencyProperty* sourceProperty_{};
    std::weak_ptr<DependencyObject> target_;
    const DependencyProperty* targetProperty_{};
    BindingMode mode_{BindingMode::OneWay};
};

// -----------------------------------------------------------------------------
// XAML event bridge and object factory used by generated code.
// -----------------------------------------------------------------------------

class XamlEventBinder {
public:
    using Handler = std::function<void(UIElement&)>;
    void Register(std::string name, Handler handler) { handlers_[std::move(name)] = std::move(handler); }
    bool Invoke(std::string_view name, UIElement& sender) const;
private:
    std::unordered_map<std::string, Handler> handlers_;
};

std::shared_ptr<UIElement> CreateXamlElement(std::string_view typeName);
void ApplyXamlAttribute(const std::shared_ptr<UIElement>& element, std::string_view name,
                        std::string_view value, XamlEventBinder* binder = nullptr);

// -----------------------------------------------------------------------------
// Window and platform entrypoint
// -----------------------------------------------------------------------------

class Window : public ContentControl {
public:
    void SetTitle(std::string value) { title_ = std::move(value); }
    const std::string& Title() const { return title_; }
    void SetInitialSize(int w, int h) { initialWidth_ = w; initialHeight_ = h; }
    int InitialWidth() const { return initialWidth_; }
    int InitialHeight() const { return initialHeight_; }
    bool SetXamlProperty(std::string_view name, std::string_view value) override;
protected:
    Size MeasureOverride(const Size& available) override;
    Size ArrangeOverride(const Size& finalSize) override;
private:
    std::string title_{"KeshUI"};
    int initialWidth_{1280};
    int initialHeight_{800};
};

int RunX11(const std::shared_ptr<Window>& window);

// Utility parsing used by XAML and app code.
float ParseFloat(std::string_view value, float fallback = 0);
bool ParseBool(std::string_view value, bool fallback = false);
Thickness ParseThickness(std::string_view value);
Color ParseColorOrResource(std::string_view value, Color fallback = {});
std::vector<std::string> SplitList(std::string_view value, char delimiter = ';');

} // namespace keshui
