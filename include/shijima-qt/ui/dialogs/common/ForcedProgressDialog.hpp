#pragma once

// 
// Shijima-Qt - Cross-platform shimeji simulation app for desktop
// Copyright (C) 2025 pixelomer
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
// 

#include <QDialog>
#include <QString>

class ElaProgressBar;
class QCloseEvent;
class QLabel;
class QHBoxLayout;
class QKeyEvent;
class QPushButton;

// A themed, non-dismissible progress dialog used by asynchronous workflows.
// The small QProgressDialog-compatible surface below keeps existing runtime
// callers source-compatible while allowing the implementation to use Ela
// controls and semantic theme colors.
class ForcedProgressDialog : public QDialog {
    Q_OBJECT
private:
    bool m_allowsClose = false;
    bool m_wasCanceled = false;
    bool m_autoReset = true;
    bool m_autoClose = true;
    int m_minimum = 0;
    int m_maximum = 100;
    int m_value = 0;
    QLabel *m_label = nullptr;
    ElaProgressBar *m_progressBar = nullptr;
    QHBoxLayout *m_buttonLayout = nullptr;
    QPushButton *m_cancelButton = nullptr;
    bool m_ownsCancelButton = false;
protected:
    void closeEvent(QCloseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
public:
    explicit ForcedProgressDialog(QWidget *parent = nullptr);

    void setRange(int minimum, int maximum);
    void setMinimum(int minimum);
    int minimum() const { return m_minimum; }
    void setMaximum(int maximum);
    int maximum() const { return m_maximum; }
    void setValue(int value);
    int value() const { return m_value; }

    void setLabelText(QString const &text);
    QString labelText() const;
    void setCancelButton(QPushButton *button);
    QPushButton *cancelButton() const { return m_cancelButton; }
    void setAutoReset(bool enabled) { m_autoReset = enabled; }
    bool autoReset() const { return m_autoReset; }
    void setAutoClose(bool enabled) { m_autoClose = enabled; }
    bool autoClose() const { return m_autoClose; }
    bool wasCanceled() const { return m_wasCanceled; }
    bool close();

public slots:
    void cancel();
    void reset();

signals:
    void canceled();
    void valueChanged(int value);
};
