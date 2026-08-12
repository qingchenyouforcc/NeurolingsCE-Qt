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
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>

#include <QUuid>

namespace {

QString submissionTr(char const *source)
{
    return QCoreApplication::translate("MascotSubmissionDialog", source);
}

QString withSubmissionDetail(QString message, QString const& detail)
{
    if (!detail.trimmed().isEmpty()) {
        message += QLatin1Char('\n') + submissionTr("Submission details: %1")
            .arg(redactSensitiveText(detail));
    }
    return message;
}

QString localizedSubmissionError(QString const& code, QString const& detail)
{
    char const *source = "Submission failed.";
    if (code == QStringLiteral("submission.file_missing")) {
        source = "The selected mascot package does not exist.";
    }
    else if (code == QStringLiteral("submission.not_configured")) {
        source = "The submission service is not configured by the maintainer.";
    }
    else if (code == QStringLiteral("submission.not_signed_in")) {
        source = "Sign in with GitHub before submitting a mascot.";
    }
    else if (code == QStringLiteral("submission.auth_failed")) {
        source = "The submission service could not authenticate GitHub.";
    }
    else if (code == QStringLiteral("submission.auth_invalid")) {
        source = "The submission service returned no session token.";
    }
    else if (code == QStringLiteral("submission.file_unreadable")) {
        source = "The selected mascot package could not be read.";
    }
    else if (code == QStringLiteral("submission.canceled")) {
        source = "The upload was canceled.";
    }
    else if (code == QStringLiteral("submission.service_error")) {
        source = "The submission service rejected the upload.";
    }
    else if (code == QStringLiteral("submission.network_error")) {
        source = "Could not reach the submission service.";
    }
    return withSubmissionDetail(submissionTr(source), detail);
}

QString localizedValidationError(QString const& error)
{
    if (error == QStringLiteral("Mascot package does not exist")) {
        return submissionTr("Mascot package does not exist.");
    }
    if (error == QStringLiteral("Package is not a valid ZIP archive")) {
        return submissionTr("Package is not a valid ZIP archive.");
    }
    if (error.startsWith(QStringLiteral("Package is missing "))) {
        return submissionTr("Package is missing %1").arg(
            error.mid(QStringLiteral("Package is missing ").size()));
    }
    if (error == QStringLiteral(
            "Package must contain actions.xml, behaviors.xml, and img/*.png"))
    {
        return submissionTr(
            "Package must contain actions.xml, behaviors.xml, and img/*.png.");
    }
    if (error.startsWith(QStringLiteral("Could not read "))) {
        return submissionTr("Could not read %1").arg(
            error.mid(QStringLiteral("Could not read ").size()));
    }
    if (error.startsWith(QStringLiteral("Missing "))) {
        return submissionTr("Missing %1").arg(
            error.mid(QStringLiteral("Missing ").size()));
    }
    if (error.startsWith(QStringLiteral("Unsupported or unsafe package entry: "))) {
        return submissionTr("Unsupported or unsafe package entry: %1").arg(
            error.mid(QStringLiteral("Unsupported or unsafe package entry: ").size()));
    }
    if (error.startsWith(QStringLiteral("Package contains a forbidden payload entry: "))) {
        return submissionTr("Package contains a forbidden payload entry: %1").arg(
            error.mid(QStringLiteral("Package contains a forbidden payload entry: ").size()));
    }
    if (error.startsWith(QStringLiteral("Package entry ")) &&
        error.endsWith(QStringLiteral(" exceeds size limits"))) {
        return submissionTr("Package entry %1 exceeds size limits").arg(
            error.mid(QStringLiteral("Package entry ").size(),
                error.size() - QStringLiteral("Package entry ").size() -
                QStringLiteral(" exceeds size limits").size()));
    }
    if (error.startsWith(QStringLiteral("Package must contain "))) {
        return submissionTr("Package must contain %1").arg(
            error.mid(QStringLiteral("Package must contain ").size()));
    }
    if (error.startsWith(QStringLiteral("Image ")) &&
        error.endsWith(QStringLiteral(" is not a valid PNG"))) {
        return submissionTr("Image %1 is not a valid PNG").arg(
            error.mid(QStringLiteral("Image ").size(),
                error.size() - QStringLiteral("Image ").size() -
                QStringLiteral(" is not a valid PNG").size()));
    }
    static QRegularExpression const imageBudgetPattern(
        QStringLiteral("^Image (.+) exceeds the maximum pixel count of ([0-9]+)$"));
    QRegularExpressionMatch imageBudgetMatch = imageBudgetPattern.match(error);
    if (imageBudgetMatch.hasMatch()) {
        return submissionTr("Image %1 exceeds the maximum pixel count of %2")
            .arg(imageBudgetMatch.captured(1), imageBudgetMatch.captured(2));
    }
    if (error == QStringLiteral("Package extracted data is too large")) {
        return submissionTr("Package extracted data is too large.");
    }
    if (error.startsWith(QStringLiteral(
            "Package image data exceeds the total pixel budget of "))) {
        return submissionTr(
            "Package image data exceeds the total pixel budget of %1")
            .arg(error.mid(QStringLiteral(
                "Package image data exceeds the total pixel budget of ").size()));
    }
    if (error == QStringLiteral("Could not create temporary extraction directory")) {
        return submissionTr("Could not create temporary extraction directory.");
    }
    if (error == QStringLiteral("Package does not contain any supported files")) {
        return submissionTr("Package does not contain any supported files.");
    }
    if (error == QStringLiteral("Archive contains symbolic links")) {
        return submissionTr("Archive contains symbolic links.");
    }
    if (error == QStringLiteral("Archive extracted an unsafe path")) {
        return submissionTr("Archive extracted an unsafe path.");
    }
    return error;
}

}

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
        label->setBuddy(widget);
        label->setMinimumWidth(110);
        widget->setAccessibleName(labelText);
        row->addWidget(label);
        row->addWidget(widget, 1);
        layout->addLayout(row);
    };

    m_packagePath = new QLineEdit(this);
    m_packagePath->setAccessibleName(tr("Mascot package path"));
    m_packagePath->setAccessibleDescription(
        tr("Path to the .mascot package to submit."));
    auto *pickButton = new QPushButton(tr("Choose .mascot..."), this);
    pickButton->setAccessibleName(pickButton->text());
    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(m_packagePath, 1);
    pathRow->addWidget(pickButton);
    auto *pathLabel = new QLabel(tr("Mascot package"), this);
    pathLabel->setBuddy(m_packagePath);
    layout->addWidget(pathLabel);
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
    addRow(tr("Description"), m_description);
    m_license = new QLineEdit(this);
    m_license->setPlaceholderText(QStringLiteral("MIT"));
    addRow(tr("License (SPDX)"), m_license);
    m_maintainers = new QLineEdit(this);
    m_maintainers->setPlaceholderText(tr("github logins, comma separated"));
    addRow(tr("Maintainers"), m_maintainers);
    m_rightsConfirmed = new QCheckBox(
        tr("I confirm I have the right to publish this work under the "
           "declared license."), this);
    m_rightsConfirmed->setAccessibleName(m_rightsConfirmed->text());
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
    m_submitButton->setAccessibleName(m_submitButton->text());
    closeButton->setAccessibleName(closeButton->text());
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
                         localizedSubmissionError(result.errorCode,
                             result.error)));
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
        QStringList localizedErrors;
        for (QString const& error : report.errors) {
            localizedErrors.append(localizedValidationError(error));
        }
        m_statusLabel->setText(tr("Local validation failed:\n%1")
            .arg(localizedErrors.join(QStringLiteral("\n"))));
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
