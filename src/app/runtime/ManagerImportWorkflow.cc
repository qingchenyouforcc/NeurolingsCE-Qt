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

#include "shijima-qt/ShijimaManager.hpp"
#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/MascotPackage.hpp"
#include "ManagerRuntimeState.hpp"
#include "../ui/ManagerUiHelpers.hpp"
#include "ManagerRuntimeHelpers.hpp"
#include <exception>
#include <QDragEnterEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QDropEvent>
#include <QPushButton>
#include <QUrl>
#include <QtConcurrent>
#include "shijima-qt/ui/dialogs/common/ForcedProgressDialog.hpp"


std::set<std::string> ShijimaManager::import(QString const& path) noexcept {
    try {
        APP_LOG_INFO("import") << "Importing mascot archive path=\"" << path.toStdString() << "\"";
        auto shimejis = MascotPackage::importArchive(path, m_runtime->mascotsPath);
        APP_LOG_INFO("import") << "Imported archive path=\"" << path.toStdString()
            << "\" templates=" << shimejis.size();
        return shimejis;
    }
    catch (std::exception &ex) {
        APP_LOG_ERROR("import") << "Import failed for path=\"" << path.toStdString()
            << "\": " << ex.what();
        return {};
    }
}

void ShijimaManager::importWithDialog(QList<QString> const& paths) {
    APP_LOG_INFO("import") << "Starting import workflow for archives=" << paths.size();
    ForcedProgressDialog *dialog = new ForcedProgressDialog { this };
    dialog->setRange(0, 0);
    QPushButton *cancelButton = new QPushButton;
    cancelButton->setEnabled(false);
    cancelButton->setText(tr("Cancel"));
    dialog->setModal(true);
    dialog->setCancelButton(cancelButton);
    dialog->setLabelText(tr("Importing shimeji..."));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();

    QtConcurrent::run([this, paths]() {
        std::set<std::string> changed;
        for (auto &path : paths) {
            auto newChanged = import(path);
            changed.insert(newChanged.begin(), newChanged.end());
        }
        return changed;
    }).then([this, dialog](std::set<std::string> changed) {
    ShijimaManagerRuntimeInternal::dispatchToMainThread([this, changed, dialog]() {
            APP_LOG_INFO("import") << "Import workflow completed; changed_templates=" << changed.size();
            reloadMascots(changed);
            this->show();
            dialog->close();

            QString msg;
            QMessageBox::Icon icon;
            if (changed.size() > 0) {
                msg = tr("Imported %n mascot(s).", "", (int)changed.size());
                icon = QMessageBox::Icon::Information;
            }
            else {
                msg = tr("Could not import any mascots from the specified archive(s).");
                icon = QMessageBox::Icon::Warning;
            }

            QMessageBox msgBox { icon, tr("Import"), msg,
                QMessageBox::StandardButton::Ok, this };
            msgBox.exec();
        });
    });
}

void ShijimaManager::showEvent(QShowEvent *event) {
    PlatformWidget::showEvent(event);
    if (!m_firstShow) {
        return;
    }

    m_firstShow = false;
    if (!m_runtime->importOnShowPath.isEmpty()) {
        QString path = m_runtime->importOnShowPath;
        m_runtime->importOnShowPath = {};
        APP_LOG_INFO("import") << "Running deferred import path=\""
            << path.toStdString() << "\"";
        importWithDialog({ path });
    }
    else if (m_runtime->templates.loadedMascots().size() == 1) {
        auto msgBox = new QMessageBox { this };
        msgBox->setText(tr("Welcome to NeurolingsCE! Get started by dragging and dropping a "
            "shimeji archive to the manager window. You can also import archives "
            "by selecting File > Import."));
        msgBox->addButton(QMessageBox::StandardButton::Ok);
        msgBox->setAttribute(Qt::WA_DeleteOnClose);
        msgBox->show();
    }
}

void ShijimaManager::importOnShow(QString const& path) {
    APP_LOG_INFO("import") << "Queued deferred import path=\""
        << path.toStdString() << "\"";
    m_runtime->importOnShowPath = path;
}

void ShijimaManager::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        APP_LOG_DEBUG("import") << "Drag enter accepted for URL payload";
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void ShijimaManager::dropEvent(QDropEvent *event) {
    QList<QString> paths;
    for (auto &url : event->mimeData()->urls()) {
        QString localPath = url.toLocalFile();
        if (!localPath.isEmpty()) {
            paths.append(localPath);
        }
    }
    if (paths.isEmpty()) {
        APP_LOG_WARN("import") << "Ignored drag-and-drop import request without local files";
        event->ignore();
        return;
    }
    APP_LOG_INFO("import") << "Received drag-and-drop import request with files=" << paths.size();
    event->acceptProposedAction();
    importWithDialog(paths);
}
