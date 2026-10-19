#include <keshui/keshui.hpp>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo/cairo-xlib.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>

namespace keshui {

namespace {

class CairoRenderer final : public Renderer {
public:
    CairoRenderer(Display* display, ::Window window, Visual* visual, int width, int height)
        : display_(display), window_(window) {
        surface_ = cairo_xlib_surface_create(display_, window_, visual, width, height);
        cr_ = cairo_create(surface_);
    }

    ~CairoRenderer() override {
        if (cr_) cairo_destroy(cr_);
        if (surface_) cairo_surface_destroy(surface_);
    }

    void Resize(int width, int height) {
        cairo_xlib_surface_set_size(surface_, width, height);
    }

    void BeginFrame(int width, int height, Color clear) override {
        width_ = width; height_ = height;
        cairo_save(cr_);
        SetSource(clear);
        cairo_rectangle(cr_, 0, 0, width, height);
        cairo_fill(cr_);
    }

    void EndFrame() override {
        cairo_restore(cr_);
        cairo_surface_flush(surface_);
        XFlush(display_);
    }

    void FillRect(const Rect& r, Color c) override {
        SetSource(c); cairo_rectangle(cr_, r.x, r.y, r.width, r.height); cairo_fill(cr_);
    }

    void FillRoundedRect(const Rect& r, const CornerRadius& radius, Color c) override {
        RoundedPath(r, radius); SetSource(c); cairo_fill(cr_);
    }

    void StrokeRoundedRect(const Rect& r, const CornerRadius& radius, Color c, float width) override {
        RoundedPath({r.x + width/2, r.y + width/2, std::max(0.0f,r.width-width), std::max(0.0f,r.height-width)}, radius);
        SetSource(c); cairo_set_line_width(cr_, width); cairo_stroke(cr_);
    }

    void DrawLine(Point a, Point b, Color c, float width) override {
        SetSource(c); cairo_set_line_width(cr_, width); cairo_move_to(cr_, a.x, a.y); cairo_line_to(cr_, b.x, b.y); cairo_stroke(cr_);
    }

    void DrawText(std::string_view text, const Rect& rect, Color color, float size,
                  FontWeight weight, TextAlignment alignment) override {
        PangoLayout* layout = pango_cairo_create_layout(cr_);
        PangoFontDescription* desc = pango_font_description_new();
        pango_font_description_set_family(desc, "Inter, Noto Sans, sans-serif");
        pango_font_description_set_absolute_size(desc, size * PANGO_SCALE);
        auto pw = PANGO_WEIGHT_NORMAL;
        if (weight == FontWeight::Medium) pw = PANGO_WEIGHT_MEDIUM;
        else if (weight == FontWeight::SemiBold) pw = PANGO_WEIGHT_SEMIBOLD;
        else if (weight == FontWeight::Bold) pw = PANGO_WEIGHT_BOLD;
        pango_font_description_set_weight(desc, pw);
        pango_layout_set_font_description(layout, desc);
        pango_layout_set_text(layout, text.data(), static_cast<int>(text.size()));
        pango_layout_set_width(layout, static_cast<int>(std::max(0.0f,rect.width) * PANGO_SCALE));
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
        pango_layout_set_single_paragraph_mode(layout, TRUE);
        pango_layout_set_alignment(layout, alignment == TextAlignment::Center ? PANGO_ALIGN_CENTER : alignment == TextAlignment::Right ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT);
        int tw=0, th=0; pango_layout_get_pixel_size(layout, &tw, &th);
        (void)tw;
        cairo_save(cr_);
        cairo_rectangle(cr_, rect.x, rect.y, rect.width, rect.height); cairo_clip(cr_);
        SetSource(color);
        cairo_move_to(cr_, rect.x, rect.y + std::max(0.0f, (rect.height - th) / 2.0f));
        pango_cairo_show_layout(cr_, layout);
        cairo_restore(cr_);
        pango_font_description_free(desc);
        g_object_unref(layout);
    }

    Size MeasureText(std::string_view text, float size, FontWeight weight) override {
        PangoLayout* layout = pango_cairo_create_layout(cr_);
        PangoFontDescription* desc = pango_font_description_new();
        pango_font_description_set_family(desc, "Inter, Noto Sans, sans-serif");
        pango_font_description_set_absolute_size(desc, size * PANGO_SCALE);
        pango_font_description_set_weight(desc, weight == FontWeight::Bold ? PANGO_WEIGHT_BOLD : weight == FontWeight::SemiBold ? PANGO_WEIGHT_SEMIBOLD : weight == FontWeight::Medium ? PANGO_WEIGHT_MEDIUM : PANGO_WEIGHT_NORMAL);
        pango_layout_set_font_description(layout, desc);
        pango_layout_set_text(layout, text.data(), static_cast<int>(text.size()));
        int w=0,h=0; pango_layout_get_pixel_size(layout,&w,&h);
        pango_font_description_free(desc); g_object_unref(layout);
        return {float(w),float(h)};
    }

    void PushClip(const Rect& r) override {
        cairo_save(cr_); cairo_rectangle(cr_, r.x, r.y, r.width, r.height); cairo_clip(cr_);
    }
    void PopClip() override { cairo_restore(cr_); }

private:
    void SetSource(Color c) { cairo_set_source_rgba(cr_, c.r, c.g, c.b, c.a); }

    void RoundedPath(const Rect& r, const CornerRadius& radius) {
        constexpr double pi = 3.14159265358979323846;
        const double maxRadius = std::max(0.0, std::min(double(r.width), double(r.height)) / 2.0);
        double tl = std::min<double>(radius.topLeft, maxRadius);
        double tr = std::min<double>(radius.topRight, maxRadius);
        double br = std::min<double>(radius.bottomRight, maxRadius);
        double bl = std::min<double>(radius.bottomLeft, maxRadius);
        cairo_new_sub_path(cr_);
        cairo_move_to(cr_, r.x + tl, r.y);
        cairo_line_to(cr_, r.x + r.width - tr, r.y);
        if (tr > 0) cairo_arc(cr_, r.x+r.width-tr, r.y+tr, tr, -pi/2, 0);
        cairo_line_to(cr_, r.x+r.width, r.y+r.height-br);
        if (br > 0) cairo_arc(cr_, r.x+r.width-br, r.y+r.height-br, br, 0, pi/2);
        cairo_line_to(cr_, r.x+bl, r.y+r.height);
        if (bl > 0) cairo_arc(cr_, r.x+bl, r.y+r.height-bl, bl, pi/2, pi);
        cairo_line_to(cr_, r.x, r.y+tl);
        if (tl > 0) cairo_arc(cr_, r.x+tl, r.y+tl, tl, pi, 3*pi/2);
        cairo_close_path(cr_);
    }

    Display* display_{};
    ::Window window_{};
    cairo_surface_t* surface_{};
    cairo_t* cr_{};
    int width_{}, height_{};
};

void SendPointer(const std::shared_ptr<UIElement>& target, Point p, int button, bool pressed, int kind) {
    if (!target) return;
    PointerEventArgs args; args.position=p; args.button=button; args.pressed=pressed; args.originalSource=target.get();
    if (kind == 0) target->OnPointerMove(args);
    else if (kind == 1) target->OnPointerPressed(args);
    else target->OnPointerReleased(args);
}

} // namespace

int RunX11(const std::shared_ptr<Window>& root) {
    if (!root) return 2;
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "KeshUI: cannot open X11 display\n";
        return 3;
    }

    int screen = DefaultScreen(display);
    int width = root->InitialWidth();
    int height = root->InitialHeight();
    ::Window native = XCreateSimpleWindow(display, RootWindow(display,screen), 120, 80,
                                          static_cast<unsigned>(width), static_cast<unsigned>(height),
                                          0, BlackPixel(display,screen), BlackPixel(display,screen));
    XStoreName(display, native, root->Title().c_str());
    XSelectInput(display, native, ExposureMask | StructureNotifyMask | PointerMotionMask |
                 ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask);

    Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, native, &wmDelete, 1);
    XMapWindow(display, native);

    CairoRenderer renderer(display, native, DefaultVisual(display,screen), width, height);
    std::shared_ptr<UIElement> hovered;
    std::shared_ptr<UIElement> captured;
    std::shared_ptr<UIElement> focused;
    bool dirty = true;

    auto layoutAndPaint = [&] {
        root->Measure({float(width),float(height)});
        root->Arrange({0,0,float(width),float(height)});
        renderer.BeginFrame(width,height,ThemeManager::Current().Palette().applicationBackground);
        root->Render(renderer);
        renderer.EndFrame();
        dirty = false;
    };

    while (true) {
        if (dirty) layoutAndPaint();
        XEvent event{};
        XNextEvent(display, &event);
        if (event.type == ClientMessage && static_cast<Atom>(event.xclient.data.l[0]) == wmDelete) break;

        if (event.type == ConfigureNotify) {
            if (event.xconfigure.width != width || event.xconfigure.height != height) {
                width = event.xconfigure.width; height = event.xconfigure.height;
                renderer.Resize(width,height); dirty = true;
            }
        } else if (event.type == Expose) {
            dirty = true;
        } else if (event.type == MotionNotify) {
            Point p{float(event.xmotion.x),float(event.xmotion.y)};
            auto hit = root->HitTest(p);
            if (hovered && hovered != hit) SendPointer(hovered,p,0,false,0);
            if (captured) SendPointer(captured,p,0,true,0);
            else if (hit) SendPointer(hit,p,0,false,0);
            hovered = hit;
            dirty = true;
        } else if (event.type == ButtonPress) {
            Point p{float(event.xbutton.x),float(event.xbutton.y)};
            auto hit = root->HitTest(p);
            captured = hit;
            focused = hit;
            if (auto control = std::dynamic_pointer_cast<Control>(focused)) control->SetFocusState(FocusState::Pointer);
            SendPointer(hit,p,event.xbutton.button,true,1);
            dirty = true;
        } else if (event.type == ButtonRelease) {
            Point p{float(event.xbutton.x),float(event.xbutton.y)};
            auto target = captured ? captured : root->HitTest(p);
            SendPointer(target,p,event.xbutton.button,false,2);
            captured.reset();
            dirty = true;
        } else if (event.type == KeyPress || event.type == KeyRelease) {
            if (focused) {
                char buffer[32]{}; KeySym sym{}; XComposeStatus status{};
                int len = XLookupString(&event.xkey, buffer, sizeof(buffer)-1, &sym, &status);
                KeyEventArgs args; args.key=static_cast<std::uint32_t>(sym); args.pressed=event.type==KeyPress; args.originalSource=focused.get();
                if (args.pressed && len>0 && static_cast<unsigned char>(buffer[0]) >= 0x20) args.text.assign(buffer,buffer+len);
                if (auto control = std::dynamic_pointer_cast<Control>(focused)) control->SetFocusState(FocusState::Keyboard);
                focused->OnKey(args);
                dirty = true;
            }
        }
    }

    XDestroyWindow(display,native);
    XCloseDisplay(display);
    return 0;
}

} // namespace keshui
