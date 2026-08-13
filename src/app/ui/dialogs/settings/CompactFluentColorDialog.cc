#include "shijima-qt/ui/dialogs/settings/CompactFluentColorDialog.hpp"

#include <QColor>
#include <QCoreApplication>
#include <QCursor>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QScreen>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>
#include <functional>
#include <utility>

#include "ElaLineEdit.h"
#include "ElaPushButton.h"
#include "ElaSlider.h"
#include "ElaSpinBox.h"
#include "ElaTheme.h"

namespace {

QColor readableTextColor(QColor const& color)
{
    return color.lightnessF() > 0.58 ? QColor(Qt::black) : QColor(Qt::white);
}

QColor clampColor(QColor color)
{
    return color.isValid() ? color.toRgb() : QColor(Qt::red);
}

QString colorDialogTr(char const *source)
{
    return QCoreApplication::translate("CompactFluentColorDialog", source);
}

class ColorMapWidget final : public QWidget {
public:
    explicit ColorMapWidget(QWidget *parent = nullptr): QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(270, 170);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleName(colorDialogTr("Color map"));
    }

    void setColorChangedCallback(std::function<void(QColor)> callback)
    {
        m_callback = std::move(callback);
    }

    void setColor(QColor color)
    {
        m_color = clampColor(color);
        update();
    }

    QSize sizeHint() const override
    {
        return QSize(280, 180);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QRect area = rect().adjusted(1, 1, -1, -1);
        int hue = m_color.hsvHue();
        if (hue < 0) {
            hue = 0;
        }

        QLinearGradient saturation(area.topLeft(), area.topRight());
        saturation.setColorAt(0.0, Qt::white);
        saturation.setColorAt(1.0, QColor::fromHsv(hue, 255, 255));
        painter.fillRect(area, saturation);

        QLinearGradient value(area.topLeft(), area.bottomLeft());
        value.setColorAt(0.0, QColor(0, 0, 0, 0));
        value.setColorAt(1.0, QColor(0, 0, 0, 255));
        painter.fillRect(area, value);

        painter.setPen(QPen(palette().color(QPalette::Mid), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(area, 6, 6);

        int saturationValue = m_color.hsvSaturation();
        int valueValue = m_color.value();
        QPoint marker(
            area.left() + (saturationValue * area.width()) / 255,
            area.bottom() - (valueValue * area.height()) / 255);
        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(marker, 6, 6);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawEllipse(marker, 7, 7);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            setFromPoint(event->position().toPoint());
            setFocus(Qt::MouseFocusReason);
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (event->buttons() & Qt::LeftButton) {
            setFromPoint(event->position().toPoint());
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        int saturation = m_color.hsvSaturation();
        int value = m_color.value();
        int step = event->modifiers() & Qt::ShiftModifier ? 16 : 4;
        switch (event->key()) {
        case Qt::Key_Left:
            saturation -= step;
            break;
        case Qt::Key_Right:
            saturation += step;
            break;
        case Qt::Key_Up:
            value += step;
            break;
        case Qt::Key_Down:
            value -= step;
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
        }
        QColor next = QColor::fromHsv(
            qMax(0, m_color.hsvHue()), qBound(0, saturation, 255),
            qBound(0, value, 255));
        if (m_callback) {
            m_callback(next);
        }
        event->accept();
    }

private:
    void setFromPoint(QPoint point)
    {
        QRect area = rect().adjusted(1, 1, -1, -1);
        if (area.width() <= 0 || area.height() <= 0) {
            return;
        }
        int saturation = qBound(0,
            ((point.x() - area.left()) * 255) / area.width(), 255);
        int value = qBound(0,
            ((area.bottom() - point.y()) * 255) / area.height(), 255);
        QColor next = QColor::fromHsv(
            qMax(0, m_color.hsvHue()), saturation, value);
        if (m_callback) {
            m_callback(next);
        }
    }

    QColor m_color = QColor(Qt::red);
    std::function<void(QColor)> m_callback;
};

QList<QColor> basicColors()
{
    return {
        QColor("#F44336"), QColor("#E91E63"), QColor("#9C27B0"),
        QColor("#673AB7"), QColor("#3F51B5"), QColor("#2196F3"),
        QColor("#03A9F4"), QColor("#00BCD4"), QColor("#009688"),
        QColor("#4CAF50"), QColor("#8BC34A"), QColor("#CDDC39"),
        QColor("#FFEB3B"), QColor("#FFC107"), QColor("#FF9800"),
        QColor("#FF5722"), QColor("#795548"), QColor("#607D8B"),
        QColor("#111111"), QColor("#444444"), QColor("#888888"),
        QColor("#BBBBBB"), QColor("#EEEEEE"), QColor("#FFFFFF")
    };
}

}

CompactFluentColorDialog::CompactFluentColorDialog(QWidget *parent):
    ElaDialog(parent), m_color(Qt::red)
{
    setWindowTitle(tr("Select Color"));
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    setIsFixedSize(false);
    setModal(true);
    setMinimumSize(0, 0);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    auto *top = new QHBoxLayout;
    top->setSpacing(14);
    m_colorMap = new ColorMapWidget(this);
    top->addWidget(m_colorMap, 1);

    auto *values = new QVBoxLayout;
    values->setSpacing(8);
    m_preview = new QFrame(this);
    m_preview->setObjectName(QStringLiteral("compactColorPreview"));
    m_preview->setMinimumSize(72, 44);
    m_preview->setMaximumHeight(58);
    m_preview->setAutoFillBackground(true);
    m_preview->setAccessibleName(tr("Selected color preview"));
    values->addWidget(m_preview);

    auto *hexLabel = new QLabel(tr("HEX"), this);
    m_hexEdit = new ElaLineEdit(this);
    m_hexEdit->setPlaceholderText(tr("#RRGGBB"));
    m_hexEdit->setAccessibleName(tr("Hex color value"));
    auto *hexRow = new QHBoxLayout;
    hexRow->setSpacing(8);
    hexRow->addWidget(hexLabel);
    hexRow->addWidget(m_hexEdit, 1);
    values->addLayout(hexRow);

    auto addRgbRow = [this, values](char const *labelText, QSpinBox **out) {
        auto *label = new QLabel(tr(labelText), this);
        auto *spin = new ElaSpinBox(this);
        spin->setRange(0, 255);
        spin->setButtonMode(ElaSpinBoxType::PMSide);
        spin->setFocusPolicy(Qt::StrongFocus);
        spin->setAccessibleName(label->text());
        auto *row = new QHBoxLayout;
        row->setSpacing(8);
        row->addWidget(label);
        row->addWidget(spin, 1);
        values->addLayout(row);
        *out = spin;
    };
    addRgbRow("Red", &m_redSpin);
    addRgbRow("Green", &m_greenSpin);
    addRgbRow("Blue", &m_blueSpin);
    top->addLayout(values, 0);
    root->addLayout(top);

    auto *hueRow = new QHBoxLayout;
    hueRow->setSpacing(8);
    hueRow->addWidget(new QLabel(tr("Hue"), this));
    m_hueSlider = new ElaSlider(Qt::Horizontal, this);
    m_hueSlider->setRange(0, 359);
    m_hueSlider->setAccessibleName(tr("Hue"));
    hueRow->addWidget(m_hueSlider, 1);
    root->addLayout(hueRow);

    auto addPalette = [this, root](QString const& title, bool custom) {
        auto *header = new QHBoxLayout;
        auto *label = new QLabel(title, this);
        header->addWidget(label);
        header->addStretch();
        if (custom) {
            auto *add = new ElaPushButton(tr("Add current"), this);
            auto *clear = new ElaPushButton(tr("Clear"), this);
            add->setFocusPolicy(Qt::StrongFocus);
            clear->setFocusPolicy(Qt::StrongFocus);
            add->setAccessibleName(tr("Add current color"));
            clear->setAccessibleName(tr("Clear custom colors"));
            add->setMinimumHeight(30);
            clear->setMinimumHeight(30);
            connect(add, &ElaPushButton::clicked, this, [this]() {
                if (!m_customColors.contains(m_color)) {
                    m_customColors.prepend(m_color);
                    while (m_customColors.size() > 12) {
                        m_customColors.removeLast();
                    }
                    rebuildCustomSwatches();
                }
            });
            connect(clear, &ElaPushButton::clicked, this, [this]() {
                m_customColors.clear();
                rebuildCustomSwatches();
            });
            header->addWidget(add);
            header->addWidget(clear);
        }
        root->addLayout(header);
        auto *container = new QWidget(this);
        auto *grid = new QGridLayout(container);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(6);
        grid->setVerticalSpacing(6);
        if (custom) {
            m_customContainer = container;
            m_customGrid = grid;
        }
        else {
            QList<QColor> colors = basicColors();
            for (int i = 0; i < colors.size(); ++i) {
                grid->addWidget(makeSwatch(colors.at(i),
                    tr("Basic color %1").arg(i + 1), container, false),
                    i / 12, i % 12);
            }
        }
        root->addWidget(container);
    };
    addPalette(tr("Basic colors"), false);
    addPalette(tr("Custom colors"), true);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    buttons->addStretch();
    auto *cancel = new ElaPushButton(tr("Cancel"), this);
    auto *ok = new ElaPushButton(tr("OK"), this);
    cancel->setFocusPolicy(Qt::StrongFocus);
    ok->setFocusPolicy(Qt::StrongFocus);
    cancel->setMinimumHeight(34);
    ok->setMinimumHeight(34);
    ok->setLightDefaultColor(ElaThemeColor(ElaThemeType::Light, PrimaryNormal));
    ok->setLightHoverColor(ElaThemeColor(ElaThemeType::Light, PrimaryHover));
    ok->setLightPressColor(ElaThemeColor(ElaThemeType::Light, PrimaryPress));
    ok->setLightTextColor(ElaThemeColor(ElaThemeType::Light, BasicTextInvert));
    ok->setDarkDefaultColor(ElaThemeColor(ElaThemeType::Dark, PrimaryNormal));
    ok->setDarkHoverColor(ElaThemeColor(ElaThemeType::Dark, PrimaryHover));
    ok->setDarkPressColor(ElaThemeColor(ElaThemeType::Dark, PrimaryPress));
    ok->setDarkTextColor(ElaThemeColor(ElaThemeType::Dark, BasicTextInvert));
    connect(cancel, &ElaPushButton::clicked, this, &QDialog::reject);
    connect(ok, &ElaPushButton::clicked, this, &QDialog::accept);
    connect(this, &ElaDialog::closeButtonClicked, this, &QDialog::reject);
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    root->addLayout(buttons);

    static_cast<ColorMapWidget *>(m_colorMap)->setColorChangedCallback([this](QColor color) {
        updateColor(color, false);
    });
    connect(m_hueSlider, &ElaSlider::valueChanged, this, [this](int hue) {
        QColor hsv = m_color.toHsv();
        updateColor(QColor::fromHsv(hue, hsv.hsvSaturation(), hsv.value()), false);
    });
    connect(m_hexEdit, &QLineEdit::editingFinished, this, [this]() {
        QColor parsed(m_hexEdit->text().trimmed());
        if (parsed.isValid()) {
            updateColor(parsed);
        }
        else {
            updateEditorValues();
        }
    });
    auto updateFromRgb = [this]() {
        updateColor(QColor(m_redSpin->value(), m_greenSpin->value(), m_blueSpin->value()));
    };
    connect(m_redSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [updateFromRgb](int) { updateFromRgb(); });
    connect(m_greenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [updateFromRgb](int) { updateFromRgb(); });
    connect(m_blueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [updateFromRgb](int) { updateFromRgb(); });

    setCurrentColor(m_color);
    applyTheme();
    QObject::connect(eTheme, &ElaTheme::themeModeChanged, this,
        [this](ElaThemeType::ThemeMode) {
            applyTheme();
            update();
        });
    rebuildCustomSwatches();
    adjustSize();
    QSize natural = sizeHint();
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    QRect available = screen == nullptr
        ? QRect(0, 0, 560, 500) : screen->availableGeometry().adjusted(16, 16, -16, -16);
    int maxWidth = qMin(560, available.width());
    int maxHeight = qMin(500, available.height());
    int minWidth = qMin(500, maxWidth);
    int minHeight = qMin(420, maxHeight);
    resize(qBound(minWidth, natural.width(), maxWidth),
        qBound(minHeight, natural.height(), maxHeight));
}

QColor CompactFluentColorDialog::currentColor() const
{
    return m_color;
}

void CompactFluentColorDialog::setCurrentColor(QColor const& color)
{
    updateColor(clampColor(color));
}

void CompactFluentColorDialog::updateColor(QColor color, bool updateHue)
{
    m_color = clampColor(color);
    if (updateHue && m_color.hsvHue() >= 0) {
        QSignalBlocker blocker(m_hueSlider);
        m_hueSlider->setValue(m_color.hsvHue());
    }
    if (m_colorMap != nullptr) {
        static_cast<ColorMapWidget *>(m_colorMap)->setColor(m_color);
    }
    updateEditorValues();
    if (m_preview != nullptr) {
        QPalette palette = m_preview->palette();
        palette.setColor(QPalette::Window, m_color);
        m_preview->setPalette(palette);
    }
}

void CompactFluentColorDialog::updateEditorValues()
{
    if (m_hexEdit == nullptr) {
        return;
    }
    QSignalBlocker hexBlocker(m_hexEdit);
    m_hexEdit->setText(m_color.name(QColor::HexRgb).toUpper());
    QSignalBlocker redBlocker(m_redSpin);
    QSignalBlocker greenBlocker(m_greenSpin);
    QSignalBlocker blueBlocker(m_blueSpin);
    m_redSpin->setValue(m_color.red());
    m_greenSpin->setValue(m_color.green());
    m_blueSpin->setValue(m_color.blue());
}

ElaPushButton *CompactFluentColorDialog::makeSwatch(QColor color,
    QString const& name, QWidget *parent, bool)
{
    auto *swatch = new ElaPushButton(parent);
    swatch->setFixedSize(26, 26);
    swatch->setBorderRadius(13);
    swatch->setAccessibleName(name);
    swatch->setFocusPolicy(Qt::StrongFocus);
    QColor hover = color.lighter(115);
    QColor press = color.darker(115);
    QColor text = readableTextColor(color);
    swatch->setLightDefaultColor(color);
    swatch->setLightHoverColor(hover);
    swatch->setLightPressColor(press);
    swatch->setLightTextColor(text);
    swatch->setDarkDefaultColor(color);
    swatch->setDarkHoverColor(hover);
    swatch->setDarkPressColor(press);
    swatch->setDarkTextColor(text);
    connect(swatch, &ElaPushButton::clicked, this, [this, color]() {
        updateColor(color);
    });
    return swatch;
}

void CompactFluentColorDialog::rebuildCustomSwatches()
{
    if (m_customGrid == nullptr || m_customContainer == nullptr) {
        return;
    }
    while (QLayoutItem *item = m_customGrid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    for (int i = 0; i < m_customColors.size(); ++i) {
        m_customGrid->addWidget(makeSwatch(m_customColors.at(i),
            tr("Custom color %1").arg(i + 1), m_customContainer, true),
            i / 12, i % 12);
    }
    if (m_customColors.isEmpty()) {
        auto *empty = new QLabel(tr("No custom colors yet"), m_customContainer);
        empty->setObjectName(QStringLiteral("compactColorEmpty"));
        m_customGrid->addWidget(empty, 0, 0, 1, 12);
    }
}

void CompactFluentColorDialog::applyTheme()
{
    auto mode = eTheme->getThemeMode();
    QColor surface = ElaThemeColor(mode, DialogBase);
    QColor text = ElaThemeColor(mode, BasicText);
    QColor field = ElaThemeColor(mode, DialogLayoutArea);
    QColor border = ElaThemeColor(mode, BasicBorder);
    QColor details = ElaThemeColor(mode, BasicDetailsText);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, surface);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, field);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, field);
    palette.setColor(QPalette::ButtonText, text);
    this->setPalette(palette);
    if (m_preview != nullptr) {
        m_preview->setStyleSheet(QString(
            "#compactColorPreview { border: 1px solid %1; border-radius: 8px; }")
            .arg(border.name(QColor::HexArgb)));
    }
    if (m_customContainer != nullptr) {
        m_customContainer->setStyleSheet(QString(
            "#compactColorEmpty { color: %1; background: transparent; }")
            .arg(details.name(QColor::HexArgb)));
    }
}
