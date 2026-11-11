#include "Theme.h"

#include <QSettings>

Theme::Theme(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    const QString saved = settings.value(QStringLiteral("appearance/theme"),
                                         QStringLiteral("dark")).toString();
    m_mode = saved == QLatin1String("light") ? QStringLiteral("light") : QStringLiteral("dark");
    applyTheme();
}

Theme *Theme::instance()
{
    static Theme theme;
    return &theme;
}

void Theme::setMode(const QString &mode)
{
    const QString next = mode == QLatin1String("light")
        ? QStringLiteral("light")
        : QStringLiteral("dark");
    if (next == m_mode)
        return;
    m_mode = next;
    applyTheme();
    QSettings().setValue(QStringLiteral("appearance/theme"), m_mode);
    emit themeChanged();
}

void Theme::toggleMode()
{
    setMode(dark() ? QStringLiteral("light") : QStringLiteral("dark"));
}

void Theme::applyTheme()
{
    if (m_mode == QLatin1String("light")) {
        m_bgTop = QColor(0xf4, 0xf6, 0xfb);
        m_bgBottom = QColor(0xe7, 0xeb, 0xf3);
        m_sidebarBg = QColor(255, 255, 255, 148);
        m_sidebarBorder = QColor(15, 23, 42, 22);
        m_paneTop = QColor(255, 255, 255, 232);
        m_paneBottom = QColor(246, 248, 252, 218);
        m_paneBorder = QColor(15, 23, 42, 28);
        m_paneChrome = QColor(255, 255, 255, 200);
        m_paneHighlight = QColor(255, 255, 255, 220);
        m_popoverBg = QColor(252, 253, 255, 252);
        m_popoverBorder = QColor(15, 23, 42, 26);
        m_exitedOverlay = QColor(248, 250, 253, 244);

        m_hoverTint = QColor(15, 23, 42, 16);
        m_softTint = QColor(15, 23, 42, 10);
        m_faintTint = QColor(15, 23, 42, 6);
        m_subtleBorder = QColor(15, 23, 42, 22);
        m_windowBorder = QColor(15, 23, 42, 34);
        m_inactiveOverlay = QColor(15, 23, 42, 18);
        m_dimOverlay = QColor(18, 24, 38, 84);

        m_textPrimary = QColor(0x1c, 0x24, 0x33);
        m_textSecondary = QColor(0x59, 0x64, 0x77);
        m_textTertiary = QColor(0x89, 0x92, 0xa4);

        m_accent = QColor(0x0a, 0x84, 0xff);
        m_accentSoft = QColor(10, 132, 255, 36);
        m_accentGlow = QColor(10, 132, 255, 80);
        m_danger = QColor(0xd9, 0x2d, 0x20);
        m_warning = QColor(0xb7, 0x79, 0x1f);
        m_success = QColor(0x1c, 0x9b, 0x5e);
        m_selection = QColor(10, 132, 255, 54);

        m_terminalForeground = QColor(0x25, 0x30, 0x44);
        m_terminalBackground = QColor(0xf7, 0xf9, 0xfc);
        m_terminalCursor = QColor(0x0a, 0x84, 0xff);
        m_terminalSelection = QColor(10, 132, 255, 50);

        m_palette = {
            QColor(0x2b, 0x30, 0x3a), QColor(0xd7, 0x00, 0x15),
            QColor(0x1f, 0x8a, 0x3b), QColor(0xb2, 0x50, 0x00),
            QColor(0x00, 0x66, 0xcc), QColor(0x89, 0x44, 0xab),
            QColor(0x00, 0x7a, 0x8c), QColor(0xc9, 0xd1, 0xdc),
            QColor(0x6e, 0x77, 0x87), QColor(0xff, 0x3b, 0x30),
            QColor(0x34, 0xc7, 0x59), QColor(0xb7, 0x79, 0x1f),
            QColor(0x0a, 0x84, 0xff), QColor(0xbf, 0x5a, 0xf2),
            QColor(0x32, 0xad, 0xe6), QColor(0xe9, 0xed, 0xf3),
        };
    } else {
        m_bgTop = QColor(0x16, 0x18, 0x20);
        m_bgBottom = QColor(0x0a, 0x0b, 0x10);
        m_sidebarBg = QColor(255, 255, 255, 9);
        m_sidebarBorder = QColor(255, 255, 255, 14);
        m_paneTop = QColor(255, 255, 255, 13);
        m_paneBottom = QColor(255, 255, 255, 4);
        m_paneBorder = QColor(255, 255, 255, 18);
        m_paneChrome = QColor(255, 255, 255, 10);
        m_paneHighlight = QColor(255, 255, 255, 26);
        m_popoverBg = QColor(27, 30, 39, 250);
        m_popoverBorder = QColor(255, 255, 255, 24);
        m_exitedOverlay = QColor(12, 14, 20, 240);

        m_hoverTint = QColor(255, 255, 255, 19);
        m_softTint = QColor(255, 255, 255, 10);
        m_faintTint = QColor(255, 255, 255, 6);
        m_subtleBorder = QColor(255, 255, 255, 18);
        m_windowBorder = QColor(255, 255, 255, 33);
        m_inactiveOverlay = QColor(5, 7, 11, 42);
        m_dimOverlay = QColor(4, 5, 8, 158);

        m_textPrimary = QColor(0xed, 0xf1, 0xf8);
        m_textSecondary = QColor(0x9b, 0xa5, 0xb8);
        m_textTertiary = QColor(0x64, 0x6e, 0x82);

        m_accent = QColor(0x5b, 0x9b, 0xff);
        m_accentSoft = QColor(92, 155, 255, 38);
        m_accentGlow = QColor(92, 155, 255, 90);
        m_danger = QColor(0xff, 0x6b, 0x72);
        m_warning = QColor(0xf5, 0xc4, 0x51);
        m_success = QColor(0x63, 0xd6, 0x8b);
        m_selection = QColor(64, 111, 180, 140);

        m_terminalForeground = QColor(0xd6, 0xdc, 0xe8);
        m_terminalBackground = QColor(0x0b, 0x0d, 0x12);
        m_terminalCursor = QColor(0x6e, 0xa8, 0xfe);
        m_terminalSelection = QColor(47, 79, 128, 165);

        m_palette = {
            QColor(0x1b, 0x1e, 0x27), QColor(0xff, 0x6b, 0x72),
            QColor(0x7a, 0xdb, 0x8f), QColor(0xf2, 0xcc, 0x60),
            QColor(0x6e, 0xa8, 0xfe), QColor(0xc7, 0x92, 0xea),
            QColor(0x6f, 0xd3, 0xe8), QColor(0xc9, 0xd1, 0xdf),
            QColor(0x5a, 0x62, 0x75), QColor(0xff, 0x8a, 0x90),
            QColor(0x9a, 0xe8, 0xac), QColor(0xf7, 0xd9, 0x7f),
            QColor(0x93, 0xbe, 0xff), QColor(0xd7, 0xae, 0xf2),
            QColor(0x95, 0xe4, 0xf3), QColor(0xf1, 0xf5, 0xfc),
        };
    }

    rebuildPalette();
}

void Theme::rebuildPalette()
{
    m_terminalPalette.clear();
    m_terminalPalette.reserve(16);
    for (const auto &color : m_palette)
        m_terminalPalette.append(color);
}

QColor Theme::paletteColor(int index) const
{
    if (index < 0)
        return m_terminalForeground;
    if (index < 16)
        return m_palette[static_cast<size_t>(index)];

    if (index < 232) {
        const int i = index - 16;
        const int r = i / 36;
        const int g = (i / 6) % 6;
        const int b = i % 6;
        auto channel = [](int v) { return v == 0 ? 0 : 55 + v * 40; };
        return QColor(channel(r), channel(g), channel(b));
    }

    if (index < 256) {
        const int v = 8 + (index - 232) * 10;
        return QColor(v, v, v);
    }

    return m_terminalForeground;
}
