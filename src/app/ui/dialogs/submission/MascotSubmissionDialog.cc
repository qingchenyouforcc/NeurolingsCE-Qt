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

#include "MascotSubmissionDialog.hpp"

#include "shijima-qt/GitHubAuthManager.hpp"
#include "shijima-qt/MascotPackage.hpp"
#include "shijima-qt/MascotSubmissionClient.hpp"
#include "shijima-qt/Secrets.hpp"

#include <QBoxLayout>
#include <QCheckBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>

#include <QUuid>

MascotSubmissionDialog::MascotSubmissionDialog(GitHubAuthManager *auth,
    MascotSubmissionClient *client, QWidget *parent):
    QDialog(parent),
    m_auth(auth),
    m_client(client)
{
    setWindowTitle(tr("Submit a Mascot"));
    setMinimumWidth(520);
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto addRow = [&](QString const& labelText, QWidget *widget) {
        auto *row = new QHBoxLayout;
        auto *label = new QLabel(labelText, this);
        label->setMinimumWidth(110);
        row->addWidget(label);
        row->addWidget(widget, 1);
        layout->addLayout(row);
    };

    m_packagePath = new QLineEdit(this);
    auto *pickButton = new QPushButton(tr("Choose .mascot..."), this);
    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(m_packagePath, 1);
    pathRow->addWidget(pickButton);
    layout->addLayout(pathRow);
    connect(pickButton, &QPushButton::clicked, this,
        &MascotSubmissionDialog::pickPackage);

    m_id = new QLineEdit(this);
    m_id->setPlaceholderText(tr("lowercase-id (first publication is permanent)"));
    addRow(tr("ID"), m_id);
    m_name = new QLineEdit(this);
    addRow(tr("Name"), m_name);
    m_version = new QLineEdit(this);
    m_version->setPlaceholderText(QStringLiteral("1.0.0"));
    addRow(tr("Version"), m_version);
    m_summary = new QLineEdit(this);
    addRow(tr("Summary"), m_summary);
    m_description = new QPlainTextEdit(this);
    m_description->setPlaceholderText(tr("Detailed description"));
    m_description->setMaximumHeight(120);
    layout->addWidget(m_description);
    m_license = new QLineEdit(this);
    m_license->setPlaceholderText(QStringLiteral("MIT"));
    addRow(tr("License (SPDX)"), m_license);
    m_maintainers = new QLineEdit(this);
    m_maintainers->setPlaceholderText(tr("github logins, comma separated"));
    addRow(tr("Maintainers"), m_maintainers);
    m_rightsConfirmed = new QCheckBox(
        tr("I confirm I have the right to publish this work under the "
           "declared license."), this);
    layout->addWidget(m_rightsConfirmed);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);
    m_prLink = new QLabel(this);
    m_prLink->setOpenExternalLinks(true);
    m_prLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    layout->addWidget(m_prLink);

    m_submitButton = new QPushButton(tr("Submit"), this);
    auto *closeButton = new QPushButton(tr("Close"), this);
    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    buttonRow->addWidget(closeButton);
    buttonRow->addWidget(m_submitButton);
    layout->addLayout(buttonRow);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_submitButton, &QPushButton::clicked, this,
        &MascotSubmissionDialog::submit);
    connect(m_client, &MascotSubmissionClient::submissionFinished, this,
        [this](MascotSubmissionClient::SubmissionResult result) {
            setBusy(false);
            if (result.ok) {
                m_statusLabel->setText(tr("Submitted. Review PR %1.")
                    .arg(result.prNumber));
                m_prLink->setText(
                    QStringLiteral("<a href=\"%1\">%2</a>")
                        .arg(result.prUrl.toString().toHtmlEscaped(),
                             result.prUrl.toDisplayString().toHtmlEscaped()));
            }
            else {
                m_statusLabel->setText(tr("Submission failed (%1): %2")
                    .arg(result.errorCode,
                         redactSensitiveText(result.error)));
            }
        });
    connect(m_client, &MascotSubmissionClient::uploadProgress, this,
        [this](qint64 sent, qint64 total) {
            if (total > 0) {
                m_statusLabel->setText(tr("Uploading... %1 / %2")
                    .arg(QLocale().formattedDataSize(sent),
                         QLocale().formattedDataSize(total)));
            }
        });
}

void MascotSubmissionDialog::pickPackage() {
    QString path = QFileDialog::getOpenFileName(this,
        tr("Choose a .mascot package"), {},
        QStringLiteral("NeurolingsCE packages (*.mascot)"));
    if (!path.isEmpty()) {
        m_packagePath->setText(path);
    }
}

void MascotSubmissionDialog::submit() {
    MascotPackageReport report;
    if (!MascotPackage::validatePackage(m_packagePath->text(), report)) {
        m_statusLabel->setText(tr("Local validation failed:\n%1")
            .arg(report.errors.join(QStringLiteral("\n"))));
        return;
    }
    if (!m_rightsConfirmed->isChecked()) {
        m_statusLabel->setText(tr("Confirm your publication rights first."));
        return;
    }
    QStringList maintainers = m_maintainers->text().split(
        QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    QJsonObject metadata;
    metadata[QStringLiteral("id")] = m_id->text().trimmed();
    metadata[QStringLiteral("name")] = m_name->text().trimmed();
    metadata[QStringLiteral("version")] = m_version->text().trimmed();
    metadata[QStringLiteral("summary")] = m_summary->text().trimmed();
    metadata[QStringLiteral("description")] = m_description->toPlainText();
    metadata[QStringLiteral("license")] = m_license->text().trimmed();
    metadata[QStringLiteral("maintainers")] = QJsonArray::fromStringList(
        maintainers);
    metadata[QStringLiteral("authors")] = QJsonArray {
        QJsonObject {
            { QStringLiteral("githubLogin"),
                m_auth->userInfo().login },
            { QStringLiteral("githubUserId"),
                m_auth->userInfo().userId },
            { QStringLiteral("displayName"),
                m_auth->userInfo().displayName.isEmpty()
                    ? m_auth->userInfo().login
                    : m_auth->userInfo().displayName },
        },
    };
    metadata[QStringLiteral("isDerivative")] = false;
    metadata[QStringLiteral("minimumNeurolingsCEVersion")] =
        QStringLiteral(NEUROLINGSCE_VERSION);
    QByteArray metadataJson = QJsonDocument(metadata).toJson(
        QJsonDocument::Compact);
    QString idempotencyKey = QUuid::createUuid().toString(
        QUuid::Id128);
    setBusy(true);
    m_client->setAccessToken(m_auth->accessToken());
    m_client->submit(m_packagePath->text(), metadataJson, idempotencyKey);
}

void MascotSubmissionDialog::setBusy(bool busy) {
    m_submitButton->setEnabled(!busy);
    m_packagePath->setEnabled(!busy);
}
