#pragma once

class QApplication;

namespace ui {

/// Central theming helper: applies a premium light or dark palette + global
/// stylesheet to the whole application. Kept free of any resource files so it
/// links cleanly into every target (app + headless tests).
class Theme {
public:
    enum class Mode { Light, Dark };

    /// Applies the given mode (palette + QSS) to the running application.
    static void apply(QApplication *app, Mode mode);

    /// Currently active mode (tracked here so toggles are stateless elsewhere).
    static Mode current();

    /// Flips between light and dark and applies the result.
    static void toggle(QApplication *app);
};

} // namespace ui
