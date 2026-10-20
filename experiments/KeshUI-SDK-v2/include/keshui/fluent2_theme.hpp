#pragma once

#include <keshui/keshui.hpp>

namespace keshui::fluent2 {

// Applies a Fluent 2 reference palette to the existing KeshUI theme engine.
// Kesh keeps its orange brand accent, while neutral surfaces, strokes,
// typography colors, focus treatment and interaction contrast follow the
// public Fluent 2 token model closely.
void ApplyReferenceTheme(ThemeKind theme);

} // namespace keshui::fluent2
