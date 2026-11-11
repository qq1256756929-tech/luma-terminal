#pragma once

#include <QColor>
#include <QObject>
#include <QVariantList>
#include <array>

class Theme : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY themeChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY themeChanged)

    // Window surfaces -------------------------------------------------------
    Q_PROPERTY(QColor bgTop MEMBER m_bgTop NOTIFY themeChanged)
    Q_PROPERTY(QColor bgBottom MEMBER m_bgBottom NOTIFY themeChanged)
    Q_PROPERTY(QColor sidebarBg MEMBER m_sidebarBg NOTIFY themeChanged)
    Q_PROPERTY(QColor sidebarBorder MEMBER m_sidebarBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor paneTop MEMBER m_paneTop NOTIFY themeChanged)
    Q_PROPERTY(QColor paneBottom MEMBER m_paneBottom NOTIFY themeChanged)
    Q_PROPERTY(QColor paneBorder MEMBER m_paneBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor paneChrome MEMBER m_paneChrome NOTIFY themeChanged)
    Q_PROPERTY(QColor paneHighlight MEMBER m_paneHighlight NOTIFY themeChanged)
    Q_PROPERTY(QColor popoverBg MEMBER m_popoverBg NOTIFY themeChanged)
    Q_PROPERTY(QColor popoverBorder MEMBER m_popoverBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor exitedOverlay MEMBER m_exitedOverlay NOTIFY themeChanged)

    // Interaction tints ----------------------------------------------------
    Q_PROPERTY(QColor hoverTint MEMBER m_hoverTint NOTIFY themeChanged)
    Q_PROPERTY(QColor softTint MEMBER m_softTint NOTIFY themeChanged)
    Q_PROPERTY(QColor faintTint MEMBER m_faintTint NOTIFY themeChanged)
    Q_PROPERTY(QColor subtleBorder MEMBER m_subtleBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor windowBorder MEMBER m_windowBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor inactiveOverlay MEMBER m_inactiveOverlay NOTIFY themeChanged)
    Q_PROPERTY(QColor dimOverlay MEMBER m_dimOverlay NOTIFY themeChanged)

    // Text -----------------------------------------------------------------
    Q_PROPERTY(QColor textPrimary MEMBER m_textPrimary NOTIFY themeChanged)
    Q_PROPERTY(QColor textSecondary MEMBER m_textSecondary NOTIFY themeChanged)
    Q_PROPERTY(QColor textTertiary MEMBER m_textTertiary NOTIFY themeChanged)

    // Accents --------------------------------------------------------------
    Q_PROPERTY(QColor accent MEMBER m_accent NOTIFY themeChanged)
    Q_PROPERTY(QColor accentSoft MEMBER m_accentSoft NOTIFY themeChanged)
    Q_PROPERTY(QColor accentGlow MEMBER m_accentGlow NOTIFY themeChanged)
    Q_PROPERTY(QColor danger MEMBER m_danger NOTIFY themeChanged)
    Q_PROPERTY(QColor warning MEMBER m_warning NOTIFY themeChanged)
    Q_PROPERTY(QColor success MEMBER m_success NOTIFY themeChanged)
    Q_PROPERTY(QColor selection MEMBER m_selection NOTIFY themeChanged)

    // Typography -----------------------------------------------------------
    Q_PROPERTY(QString uiFont MEMBER m_uiFont NOTIFY themeChanged)
    Q_PROPERTY(QString monoFont MEMBER m_monoFont NOTIFY themeChanged)
    Q_PROPERTY(int radius MEMBER m_radius NOTIFY themeChanged)

    // Terminal palette -----------------------------------------------------
    Q_PROPERTY(QColor terminalForeground MEMBER m_terminalForeground NOTIFY themeChanged)
    Q_PROPERTY(QColor terminalBackground MEMBER m_terminalBackground NOTIFY themeChanged)
    Q_PROPERTY(QColor terminalCursor MEMBER m_terminalCursor NOTIFY themeChanged)
    Q_PROPERTY(QColor terminalSelection MEMBER m_terminalSelection NOTIFY themeChanged)
    Q_PROPERTY(QVariantList terminalPalette MEMBER m_terminalPalette NOTIFY themeChanged)

public:
    explicit Theme(QObject *parent = nullptr);

    static Theme *instance();

    QString mode() const { return m_mode; }
    bool dark() const { return m_mode == QLatin1String("dark"); }
    Q_INVOKABLE void setMode(const QString &mode);
    Q_INVOKABLE void toggleMode();

    QColor paletteColor(int index) const;
    QColor terminalFg() const { return m_terminalForeground; }
    QColor terminalBgColor() const { return m_terminalBackground; }
    QColor cursorColor() const { return m_terminalCursor; }
    QColor selectionColor() const { return m_terminalSelection; }
    QColor textPrimary() const { return m_textPrimary; }
    QColor textSecondary() const { return m_textSecondary; }
    QColor accent() const { return m_accent; }
    QColor warningColor() const { return m_warning; }
    QColor dangerColor() const { return m_danger; }
    QColor terminalCursor() const { return m_terminalCursor; }
    QString monoFont() const { return m_monoFont; }
    QString uiFont() const { return m_uiFont; }

signals:
    void themeChanged();

private:
    void applyTheme();
    void rebuildPalette();

    QString m_mode{QStringLiteral("dark")};

    QColor m_bgTop;
    QColor m_bgBottom;
    QColor m_sidebarBg;
    QColor m_sidebarBorder;
    QColor m_paneTop;
    QColor m_paneBottom;
    QColor m_paneBorder;
    QColor m_paneChrome;
    QColor m_paneHighlight;
    QColor m_popoverBg;
    QColor m_popoverBorder;
    QColor m_exitedOverlay;

    QColor m_hoverTint;
    QColor m_softTint;
    QColor m_faintTint;
    QColor m_subtleBorder;
    QColor m_windowBorder;
    QColor m_inactiveOverlay;
    QColor m_dimOverlay;

    QColor m_textPrimary;
    QColor m_textSecondary;
    QColor m_textTertiary;

    QColor m_accent;
    QColor m_accentSoft;
    QColor m_accentGlow;
    QColor m_danger;
    QColor m_warning;
    QColor m_success;
    QColor m_selection;

    QString m_uiFont{QStringLiteral("Noto Sans")};
    QString m_monoFont{QStringLiteral("JetBrains Mono")};
    int m_radius = 12;

    QColor m_terminalForeground;
    QColor m_terminalBackground;
    QColor m_terminalCursor;
    QColor m_terminalSelection;
    QVariantList m_terminalPalette;

    std::array<QColor, 16> m_palette;
};
