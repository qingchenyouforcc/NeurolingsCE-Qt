#pragma once

#include <QColor>
#include <QList>

#include "ElaDialog.h"

class ElaLineEdit;
class ElaPushButton;
class ElaSlider;
class QFrame;
class QGridLayout;
class QSpinBox;
class QString;
class QWidget;

class CompactFluentColorDialog final : public ElaDialog {
    Q_OBJECT
public:
    explicit CompactFluentColorDialog(QWidget *parent = nullptr);

    QColor currentColor() const;
    void setCurrentColor(QColor const& color);

private:
    QColor m_color;
    QWidget *m_colorMap = nullptr;
    ElaSlider *m_hueSlider = nullptr;
    QFrame *m_preview = nullptr;
    ElaLineEdit *m_hexEdit = nullptr;
    QSpinBox *m_redSpin = nullptr;
    QSpinBox *m_greenSpin = nullptr;
    QSpinBox *m_blueSpin = nullptr;
    QGridLayout *m_customGrid = nullptr;
    QWidget *m_customContainer = nullptr;
    QList<QColor> m_customColors;

    void rebuildCustomSwatches();
    void updateColor(QColor color, bool updateHue = true);
    void updateEditorValues();
    ElaPushButton *makeSwatch(QColor color, QString const& name,
        QWidget *parent, bool custom);
    void applyTheme();
};
