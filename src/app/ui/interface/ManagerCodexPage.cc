// NeurolingsCE - Codex app-server manager page

#include "shijima-qt/ShijimaManager.hpp"
#include "shijima-qt/CodexAppServerClient.hpp"
#include "shijima-qt/CodexAppServerModels.hpp"
#include "../../runtime/ManagerRuntimeState.hpp"
#include "../ManagerUiState.hpp"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QFileInfo>

#include "ElaIcon.h"

namespace {

QString pageTr(char const *source)
{
    return QCoreApplication::translate("ShijimaManager", source);
}

QString stateText(CodexServerState state)
{
    switch (state) {
        case CodexServerState::Stopped: return pageTr("Stopped");
        case CodexServerState::Starting: return pageTr("Starting");
        case CodexServerState::Initializing: return pageTr("Initializing");
        case CodexServerState::Ready: return pageTr("Ready");
        case CodexServerState::Running: return pageTr("Running");
        case CodexServerState::NeedsInput: return pageTr("Needs input");
        case CodexServerState::Blocked: return pageTr("Blocked");
        case CodexServerState::Stopping: return pageTr("Stopping");
    }
    return pageTr("Unknown");
}

QString approvalTypeText(CodexApprovalKind kind)
{
    switch (kind) {
        case CodexApprovalKind::CommandExecution: return pageTr("Command");
        case CodexApprovalKind::FileChange: return pageTr("File change");
        case CodexApprovalKind::Network: return pageTr("Network");
    }
    return pageTr("Approval");
}

QString approvalDetails(CodexApprovalRequest const& request)
{
    QStringList lines;
    lines << pageTr("Type: %1").arg(approvalTypeText(request.kind));
    if (request.kind == CodexApprovalKind::Network) {
        QString host = request.networkContext.value(QStringLiteral("host")).toString();
        QString protocol = request.networkContext.value(QStringLiteral("protocol")).toString();
        QString port = request.networkContext.value(QStringLiteral("port")).toVariant().toString();
        if (!host.isEmpty()) lines << pageTr("Host: %1").arg(host);
        if (!protocol.isEmpty()) lines << pageTr("Protocol: %1").arg(protocol);
        if (!port.isEmpty()) lines << pageTr("Port: %1").arg(port);
    }
    else {
        if (!request.reason.trimmed().isEmpty()) {
            lines << pageTr("Reason: %1").arg(request.reason.trimmed());
        }
        if (!request.command.trimmed().isEmpty()) {
            lines << pageTr("Command: %1").arg(request.command.trimmed());
        }
        if (!request.cwd.trimmed().isEmpty()) {
            lines << pageTr("Working directory: %1").arg(
                QFileInfo(request.cwd).fileName().isEmpty()
                    ? request.cwd : QFileInfo(request.cwd).fileName());
        }
    }
    if (request.kind == CodexApprovalKind::CommandExecution && !request.commandActions.isEmpty()) {
        lines << pageTr("Actions: %1").arg(request.commandActions.size());
        int actionCount = 0;
        for (auto const& action : request.commandActions) {
            if (++actionCount > 8) {
                lines << pageTr("Further actions are hidden.");
                break;
            }
            QString actionText = action.description.trimmed();
            if (!action.command.trimmed().isEmpty()) {
                if (!actionText.isEmpty()) actionText += QStringLiteral(": ");
                actionText += action.command.trimmed();
            }
            if (!actionText.isEmpty()) lines << QStringLiteral("  ") + actionText.left(4096);
        }
    }
    if (request.kind == CodexApprovalKind::FileChange && !request.changes.isEmpty()) {
        lines << pageTr("Changed files: %1").arg(request.changes.size());
        qsizetype totalDiff = 0;
        for (auto const& change : request.changes) {
            if (lines.size() >= 12) {
                lines << pageTr("Further details are hidden.");
                break;
            }
            lines << QStringLiteral("  ") + change.kind + QStringLiteral(" ") + change.path;
            if (!change.diff.isEmpty() && totalDiff < 256 * 1024) {
                qsizetype remaining = (256 * 1024) - totalDiff;
                qsizetype amount = qMin<qsizetype>(qMin<qsizetype>(128 * 1024,
                    change.diff.size()), remaining);
                lines << change.diff.left(amount);
                totalDiff += amount;
                if (amount < change.diff.size()) lines << pageTr("Content truncated.");
            }
        }
        if (totalDiff >= 256 * 1024) lines << pageTr("Content truncated.");
    }
    return lines.join(QLatin1Char('\n'));
}

QPushButton *makeButton(QWidget *parent, QString const& text,
    QString const& description)
{
    auto *button = new QPushButton(text, parent);
    button->setAccessibleName(text);
    button->setAccessibleDescription(description);
    button->setFocusPolicy(Qt::StrongFocus);
    return button;
}

QString requestKey(QJsonValue const& id)
{
    return codexApprovalRequestIdKey(id);
}

} // namespace

void ShijimaManager::setupCodexPage()
{
    auto *page = new QWidget(this);
    m_ui->codexPage = page;
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    auto *title = new QLabel(tr("Codex"), page);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 3);
    title->setFont(titleFont);
    root->addWidget(title);
    auto *description = new QLabel(
        tr("Connect to a private Codex app-server session and review plans, replies, and approvals."), page);
    description->setWordWrap(true);
    root->addWidget(description);

    auto *statusBox = new QGroupBox(tr("Session"), page);
    auto *statusLayout = new QGridLayout(statusBox);
    statusLayout->setColumnStretch(1, 1);
    auto addStatus = [statusLayout, statusBox](QString const& label, QLabel **out) {
        int row = statusLayout->rowCount();
        statusLayout->addWidget(new QLabel(label, statusBox), row, 0);
        *out = new QLabel(statusBox);
        (*out)->setTextFormat(Qt::PlainText);
        (*out)->setTextInteractionFlags(Qt::TextSelectableByMouse);
        (*out)->setWordWrap(true);
        statusLayout->addWidget(*out, row, 1);
    };
    addStatus(tr("Status"), &m_ui->codexStateLabel);
    addStatus(tr("Thread"), &m_ui->codexThreadLabel);
    addStatus(tr("Turn"), &m_ui->codexTurnLabel);
    addStatus(tr("Mode"), &m_ui->codexPlanLabel);
    root->addWidget(statusBox);

    auto *sessionButtons = new QHBoxLayout;
    m_ui->codexConnectButton = makeButton(page, tr("Connect Codex"),
        tr("Start the Codex app-server after an explicit click."));
    m_ui->codexNewThreadButton = makeButton(page, tr("New session"),
        tr("Create a new app-server thread."));
    m_ui->codexResumeButton = makeButton(page, tr("Resume recent"),
        tr("Resume the explicitly saved recent thread."));
    sessionButtons->addWidget(m_ui->codexConnectButton);
    sessionButtons->addWidget(m_ui->codexNewThreadButton);
    sessionButtons->addWidget(m_ui->codexResumeButton);
    sessionButtons->addStretch();
    root->addLayout(sessionButtons);

    auto *approvalBox = new QGroupBox(tr("Approvals"), page);
    auto *approvalLayout = new QVBoxLayout(approvalBox);
    m_ui->codexApprovalList = new QListWidget(approvalBox);
    m_ui->codexApprovalList->setAccessibleName(tr("Pending Codex approvals"));
    m_ui->codexApprovalList->setAccessibleDescription(
        tr("Select a pending request before choosing a decision."));
    m_ui->codexApprovalList->setMinimumHeight(70);
    approvalLayout->addWidget(m_ui->codexApprovalList);
    m_ui->codexApprovalDetailLabel = new QLabel(approvalBox);
    m_ui->codexApprovalDetailLabel->setTextFormat(Qt::PlainText);
    m_ui->codexApprovalDetailLabel->setWordWrap(true);
    m_ui->codexApprovalDetailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    approvalLayout->addWidget(m_ui->codexApprovalDetailLabel);
    auto *approvalButtons = new QHBoxLayout;
    m_ui->codexApprovalDeclineButton = makeButton(approvalBox, tr("Decline"),
        tr("Reject this operation and let the agent continue."));
    m_ui->codexApprovalAcceptButton = makeButton(approvalBox, tr("Allow once"),
        tr("Allow only this operation."));
    m_ui->codexApprovalSessionButton = makeButton(approvalBox, tr("Allow for session"),
        tr("Allow this operation for the current app-server session."));
    m_ui->codexApprovalCancelButton = makeButton(approvalBox, tr("Decline and stop"),
        tr("Reject this operation and interrupt the current turn."));
    for (auto *button : { m_ui->codexApprovalDeclineButton,
        m_ui->codexApprovalAcceptButton, m_ui->codexApprovalSessionButton,
        m_ui->codexApprovalCancelButton })
    {
        button->setAutoDefault(false);
        button->setDefault(false);
    }
    approvalButtons->addWidget(m_ui->codexApprovalDeclineButton);
    approvalButtons->addWidget(m_ui->codexApprovalAcceptButton);
    approvalButtons->addWidget(m_ui->codexApprovalSessionButton);
    approvalButtons->addWidget(m_ui->codexApprovalCancelButton);
    approvalLayout->addLayout(approvalButtons);
    root->addWidget(approvalBox);

    auto *planBox = new QGroupBox(tr("Plan"), page);
    auto *planLayout = new QVBoxLayout(planBox);
    m_ui->codexPlanSteps = new QListWidget(planBox);
    m_ui->codexPlanSteps->setAccessibleName(tr("Codex plan steps"));
    m_ui->codexPlanSteps->setMinimumHeight(90);
    planLayout->addWidget(m_ui->codexPlanSteps);
    m_ui->codexFinalLabel = new QLabel(planBox);
    m_ui->codexFinalLabel->setTextFormat(Qt::PlainText);
    m_ui->codexFinalLabel->setWordWrap(true);
    m_ui->codexFinalLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    planLayout->addWidget(m_ui->codexFinalLabel);
    root->addWidget(planBox, 1);

    auto *replyBox = new QGroupBox(tr("Message"), page);
    auto *replyLayout = new QVBoxLayout(replyBox);
    auto *modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(tr("Mode:"), replyBox));
    m_ui->codexModeCombo = new QComboBox(replyBox);
    m_ui->codexModeCombo->addItem(tr("Default"), false);
    m_ui->codexModeCombo->addItem(tr("Plan"), true);
    m_ui->codexModeCombo->setAccessibleName(tr("Codex mode"));
    m_ui->codexModeCombo->setAccessibleDescription(tr("Choose Default or Plan mode for the next turn."));
    modeRow->addWidget(m_ui->codexModeCombo);
    modeRow->addStretch();
    replyLayout->addLayout(modeRow);
    m_ui->codexInputEdit = new QPlainTextEdit(replyBox);
    m_ui->codexInputEdit->setPlaceholderText(tr("Ask Codex something..."));
    m_ui->codexInputEdit->setAccessibleName(tr("Codex message"));
    m_ui->codexInputEdit->setAccessibleDescription(tr("Enter a message to send to the current thread."));
    m_ui->codexInputEdit->setMinimumHeight(70);
    replyLayout->addWidget(m_ui->codexInputEdit);
    auto *replyButtons = new QHBoxLayout;
    m_ui->codexSendButton = makeButton(replyBox, tr("Send"),
        tr("Send the message or steer the active turn."));
    m_ui->codexApplyPlanButton = makeButton(replyBox, tr("Implement this plan"),
        tr("Start a Default mode turn using the confirmed plan."));
    m_ui->codexModifyPlanButton = makeButton(replyBox, tr("Modify plan"),
        tr("Ask Codex to revise the current plan."));
    m_ui->codexInterruptButton = makeButton(replyBox, tr("Abort task"),
        tr("Interrupt the active Codex turn."));
    replyButtons->addWidget(m_ui->codexSendButton);
    replyButtons->addWidget(m_ui->codexApplyPlanButton);
    replyButtons->addWidget(m_ui->codexModifyPlanButton);
    replyButtons->addWidget(m_ui->codexInterruptButton);
    replyButtons->addStretch();
    replyLayout->addLayout(replyButtons);
    root->addWidget(replyBox);

    m_ui->codexDiagnosticLabel = new QLabel(page);
    m_ui->codexDiagnosticLabel->setTextFormat(Qt::PlainText);
    m_ui->codexDiagnosticLabel->setWordWrap(true);
    m_ui->codexDiagnosticLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_ui->codexDiagnosticLabel);

    auto *client = m_runtime->codexClient.get();
    connect(m_ui->codexConnectButton, &QPushButton::clicked, this, [this, client]() {
        if (client->state() == CodexServerState::Stopped ||
            client->state() == CodexServerState::Blocked)
        {
            client->start();
        }
        else {
            client->stop();
        }
        updateCodexPageState();
    });
    connect(m_ui->codexNewThreadButton, &QPushButton::clicked, this, [this, client]() {
        client->startNewThread(m_settings->value(QStringLiteral("codex/lastWorkspace")).toString());
    });
    connect(m_ui->codexResumeButton, &QPushButton::clicked, this, [this, client]() {
        QString id = m_settings->value(QStringLiteral("codex/lastThreadId")).toString().trimmed();
        if (id.isEmpty()) {
            showCodexDiagnostic(tr("No recent Codex thread is saved."));
            return;
        }
        client->resumeThread(id);
    });
    auto sendText = [this, client](bool forcePlan, bool applyPlan) {
        QString text = m_ui->codexInputEdit->toPlainText().trimmed();
        if (applyPlan) {
            text = tr("Please begin implementing the confirmed plan.");
        }
        if (text.isEmpty()) {
            showCodexDiagnostic(tr("Enter a message before sending."));
            return;
        }
        if (client->state() == CodexServerState::Running) {
            client->steerTurn(text);
        }
        else {
            bool plan = forcePlan || m_ui->codexModeCombo->currentData().toBool();
            client->startTurn(text, plan);
        }
        // Keep the draft until the user explicitly removes it.  The client
        // API is asynchronous and deliberately has no optimistic success
        // result; preserving it makes retrying a failed send safe.
    };
    connect(m_ui->codexSendButton, &QPushButton::clicked, this,
        [sendText]() mutable { sendText(false, false); });
    connect(m_ui->codexModifyPlanButton, &QPushButton::clicked, this,
        [sendText]() mutable { sendText(true, false); });
    connect(m_ui->codexApplyPlanButton, &QPushButton::clicked, this,
        [sendText]() mutable { sendText(false, true); });
    connect(m_ui->codexInterruptButton, &QPushButton::clicked, client,
        &CodexAppServerClient::interruptTurn);
    connect(m_ui->codexApprovalList, &QListWidget::currentRowChanged, this,
        [this](int row) {
            if (row < 0 || m_ui->codexApprovalList->item(row) == nullptr) {
                m_ui->codexSelectedApprovalKey.clear();
                m_ui->codexApprovalDetailLabel->clear();
                updateCodexPageApprovals();
                return;
            }
            auto *item = m_ui->codexApprovalList->item(row);
            m_ui->codexSelectedApprovalKey = item->data(Qt::UserRole).toString();
            for (auto const& request : m_runtime->codexClient->pendingApprovals()) {
                if (requestKey(request.requestId) == m_ui->codexSelectedApprovalKey) {
                    m_ui->codexApprovalDetailLabel->setText(approvalDetails(request));
                    break;
                }
            }
            updateCodexPageApprovals();
        });
    auto resolveSelected = [this, client](CodexApprovalDecision decision) {
        if (m_ui->codexSelectedApprovalKey.isEmpty()) return;
        for (auto const& request : client->pendingApprovals()) {
            if (requestKey(request.requestId) == m_ui->codexSelectedApprovalKey) {
                if (!client->resolveApproval(request.requestId, decision)) {
                    showCodexDiagnostic(tr("This approval is no longer available."));
                }
                break;
            }
        }
    };
    connect(m_ui->codexApprovalDeclineButton, &QPushButton::clicked, this,
        [resolveSelected]() mutable { resolveSelected(CodexApprovalDecision::Decline); });
    connect(m_ui->codexApprovalAcceptButton, &QPushButton::clicked, this,
        [resolveSelected]() mutable { resolveSelected(CodexApprovalDecision::Accept); });
    connect(m_ui->codexApprovalSessionButton, &QPushButton::clicked, this,
        [resolveSelected]() mutable { resolveSelected(CodexApprovalDecision::AcceptForSession); });
    connect(m_ui->codexApprovalCancelButton, &QPushButton::clicked, this,
        [resolveSelected]() mutable { resolveSelected(CodexApprovalDecision::Cancel); });

    connect(client, &CodexAppServerClient::stateChanged, this,
        [this](CodexServerState) { updateCodexPageState(); updateCodexPageApprovals(); },
        Qt::QueuedConnection);
    connect(client, &CodexAppServerClient::diagnostic, this,
        [this](QString const& message) { showCodexDiagnostic(message); },
        Qt::QueuedConnection);
    connect(client, &CodexAppServerClient::protocolError, this,
        [this](QString const& message) { showCodexDiagnostic(message); },
        Qt::QueuedConnection);
    connect(client, &CodexAppServerClient::threadChanged, this,
        [this](QString const& threadId, QString const& workspace) {
            m_settings->setValue(QStringLiteral("codex/lastThreadId"), threadId);
            if (!workspace.isEmpty()) m_settings->setValue(QStringLiteral("codex/lastWorkspace"), workspace);
            updateCodexPageState();
        }, Qt::QueuedConnection);
    connect(client, &CodexAppServerClient::planUpdated, this,
        [this](CodexPlanSnapshot const& snapshot) { updateCodexPagePlan(snapshot); },
        Qt::QueuedConnection);
    connect(client, &CodexAppServerClient::finalPlanReady, this,
        [this](CodexPlanSnapshot const& snapshot) {
            updateCodexPagePlan(snapshot);
            updateCodexPageState();
            if (m_settings->value(QStringLiteral("codex/planBubbleEnabled"), true).toBool()) {
                QStringList summary;
                if (!snapshot.explanation.trimmed().isEmpty()) summary << snapshot.explanation.trimmed();
                for (auto const& step : snapshot.steps.mid(0, 6)) summary << QStringLiteral("• ") + step.step;
                QString key = snapshot.threadId + QLatin1Char('\x1f') +
                    snapshot.turnId + QLatin1Char('\x1f') + snapshot.itemId;
                showCodexAppServerBubble(tr("Codex · Plan completed"),
                    summary.join(QLatin1Char('\n')), key);
            }
        }, Qt::QueuedConnection);
    connect(client, &CodexAppServerClient::finalMessageReady, this,
        [this, client](QString const& text) {
            m_ui->codexFinalLabel->setText(text.trimmed());
            updateCodexPageState();
            if (m_settings->value(QStringLiteral("codex/planBubbleEnabled"), true).toBool()) {
                showCodexAppServerBubble(tr("Codex · Completed"), text.left(4096),
                    client->threadId() + QLatin1Char('\x1f') + client->turnId());
            }
        }, Qt::QueuedConnection);
    connect(client, &CodexAppServerClient::turnFinished, this,
        [this](QString const&, bool) { updateCodexPageState(); },
        Qt::QueuedConnection);
    connect(client, &CodexAppServerClient::approvalRequested, this,
        [this](CodexApprovalRequest const& request) {
            updateCodexPageApprovals(); updateCodexPageState();
            if (m_settings->value(QStringLiteral("codex/approvalBubbleEnabled"), true).toBool()) {
                showCodexAppServerBubble(tr("Codex · Confirmation needed"),
                    tr("%1\nPending approvals: %2")
                        .arg(approvalTypeText(request.kind))
                        .arg(m_runtime->codexClient->pendingApprovals().size()),
                    request.threadId + QLatin1Char('\x1f') + request.turnId +
                        QLatin1Char('\x1f') + QStringLiteral("approval"));
            }
        }, Qt::QueuedConnection);
    connect(client, &CodexAppServerClient::approvalResolved, this,
        [this](QJsonValue const&) { updateCodexPageApprovals(); updateCodexPageState(); },
        Qt::QueuedConnection);
    connect(client, &CodexAppServerClient::userInputResolved, this,
        [this](QJsonValue const&) { updateCodexPageState(); },
        Qt::QueuedConnection);
    connect(client, &CodexAppServerClient::userInputRequested, this,
        [this](CodexUserInputRequest const& request) {
            if (m_settings->value(QStringLiteral("codex/approvalBubbleEnabled"), true).toBool()) {
                showCodexAppServerBubble(tr("Codex · Input needed"),
                    tr("Please answer the question in the Codex page."),
                    request.threadId + QLatin1Char('\x1f') + request.turnId +
                        QLatin1Char('\x1f') + QStringLiteral("input"));
            }
            QDialog dialog(this);
            dialog.setWindowTitle(tr("Codex needs input"));
            dialog.setMinimumWidth(480);
            auto *layout = new QVBoxLayout(&dialog);
            QList<QPair<CodexUserInputQuestion, QObject*>> editors;
            for (auto const& question : request.questions.mid(0, 3)) {
                auto *group = new QGroupBox(question.header.isEmpty()
                    ? tr("Question") : question.header, &dialog);
                auto *groupLayout = new QVBoxLayout(group);
                auto *label = new QLabel(question.question, group);
                label->setTextFormat(Qt::PlainText);
                label->setWordWrap(true);
                groupLayout->addWidget(label);
                QButtonGroup *buttons = new QButtonGroup(group);
                for (auto const& option : question.options.mid(0, 3)) {
                    auto *radio = new QRadioButton(option.label, group);
                    radio->setToolTip(option.description);
                    buttons->addButton(radio);
                    groupLayout->addWidget(radio);
                }
                QObject *editor = buttons;
                if (question.isOther) {
                    auto *other = new QLineEdit(group);
                    other->setPlaceholderText(tr("Other..."));
                    if (question.isSecret) other->setEchoMode(QLineEdit::Password);
                    groupLayout->addWidget(other);
                    editor = other;
                }
                layout->addWidget(group);
                editors.append({ question, editor });
            }
            auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            layout->addWidget(box);
            bool answered = false;
            auto submit = [this, &dialog, &answered, request, editors]() {
                if (answered) return;
                answered = true;
                QJsonObject answerObject;
                for (auto const& pair : editors) {
                    QString answer;
                    if (auto *line = qobject_cast<QLineEdit*>(pair.second)) {
                        answer = line->text();
                    }
                    else if (auto *group = qobject_cast<QButtonGroup*>(pair.second)) {
                        if (group->checkedButton() != nullptr) answer = group->checkedButton()->text();
                    }
                    if (!answer.isEmpty()) answerObject.insert(pair.first.id,
                        QJsonObject {{ QStringLiteral("answers"), QJsonArray { answer } }});
                }
                m_runtime->codexClient->resolveUserInput(request.requestId, answerObject);
                dialog.accept();
            };
            connect(box, &QDialogButtonBox::accepted, &dialog, submit);
            connect(box, &QDialogButtonBox::rejected, &dialog, [this, &dialog, &answered, request]() {
                if (answered) return;
                answered = true;
                m_runtime->codexClient->resolveUserInput(request.requestId, {});
                dialog.reject();
            });
            connect(&dialog, &QDialog::finished, &dialog,
                [this, &answered, request](int result) {
                    if (result == QDialog::Accepted || answered) return;
                    answered = true;
                    m_runtime->codexClient->resolveUserInput(request.requestId, {});
            });
            if (request.autoResolutionMs > 0) {
                QTimer::singleShot(request.autoResolutionMs, &dialog,
                    [this, &dialog, &answered, request]() {
                        if (answered) return;
                        answered = true;
                        m_runtime->codexClient->resolveUserInput(request.requestId, {});
                        dialog.reject();
                    });
            }
            dialog.exec();
            updateCodexPageState();
        }, Qt::QueuedConnection);

    m_ui->codexPage->setProperty("neurolingsce.codexPage", true);
    addPageNode(tr("Codex"), m_ui->codexPage, 0, ElaIconType::Message);
    m_ui->codexKey = m_ui->codexPage->property("ElaPageKey").toString();
    updateCodexPageState();
    updateCodexPagePlan({});
    updateCodexPageApprovals();
}

void ShijimaManager::showCodexPage()
{
    if (m_runtime == nullptr || m_runtime->shuttingDown.load() ||
        m_ui == nullptr || m_ui->codexPage == nullptr) return;
    setManagerVisible(true);
    if (!m_ui->codexKey.isEmpty()) navigation(m_ui->codexKey);
    m_ui->codexPage->setFocus(Qt::OtherFocusReason);
    if (m_runtime->codexClient != nullptr &&
        !m_runtime->codexClient->pendingApprovals().isEmpty())
    {
        // Keep the safe action as the initial focus; allowing an approval is
        // never the default keyboard action.
        m_ui->codexApprovalDeclineButton->setFocus(Qt::OtherFocusReason);
    }
}

void ShijimaManager::showCodexDiagnostic(QString const& message)
{
    if (m_ui->codexDiagnosticLabel != nullptr) {
        m_ui->codexDiagnosticLabel->setText(message.trimmed());
    }
}

void ShijimaManager::updateCodexPageState()
{
    if (m_runtime == nullptr || m_runtime->codexClient == nullptr || m_ui->codexPage == nullptr) return;
    auto *client = m_runtime->codexClient.get();
    CodexServerState state = client->state();
    bool featureEnabled = m_settings->value(QStringLiteral("codex/appServerEnabled"), false).toBool();
    m_ui->codexStateLabel->setText(stateText(state));
    m_ui->codexThreadLabel->setText(client->threadId().isEmpty()
        ? tr("No active thread") : client->threadId());
    m_ui->codexTurnLabel->setText(client->turnId().isEmpty()
        ? tr("No active turn") : client->turnId());
    m_ui->codexPlanLabel->setText(client->planSupported() ? tr("Plan supported") : tr("Default only"));
    m_ui->codexConnectButton->setText(state == CodexServerState::Stopped || state == CodexServerState::Blocked
        ? tr("Connect Codex") : tr("Disconnect"));
    m_ui->codexConnectButton->setEnabled(featureEnabled);
    if (!featureEnabled) {
        m_ui->codexDiagnosticLabel->setText(tr("Enable Codex interaction in Settings before connecting."));
    }
    else if (m_ui->codexDiagnosticLabel->text() ==
        tr("Enable Codex interaction in Settings before connecting.")) {
        m_ui->codexDiagnosticLabel->clear();
    }
    bool connected = state == CodexServerState::Starting ||
        state == CodexServerState::Initializing || state == CodexServerState::Ready ||
        state == CodexServerState::Running || state == CodexServerState::NeedsInput;
    bool ready = state == CodexServerState::Ready;
    bool hasThread = !client->threadId().isEmpty();
    bool running = state == CodexServerState::Running;
    m_ui->codexNewThreadButton->setEnabled(featureEnabled && ready && !running);
    m_ui->codexResumeButton->setEnabled(featureEnabled && ready && !running &&
        !m_settings->value(QStringLiteral("codex/lastThreadId")).toString().trimmed().isEmpty());
    m_ui->codexSendButton->setEnabled(featureEnabled && connected && hasThread && (running || state == CodexServerState::Ready));
    m_ui->codexModifyPlanButton->setEnabled(featureEnabled && hasThread &&
        (ready || running) && client->planSupported());
    m_ui->codexApplyPlanButton->setEnabled(featureEnabled && state == CodexServerState::Ready &&
        client->planSnapshot().final && !client->planSnapshot().finalText.trimmed().isEmpty());
    m_ui->codexInterruptButton->setEnabled(featureEnabled && running);
    m_ui->codexModeCombo->setEnabled(featureEnabled && ready && client->planSupported());
    if (auto *model = qobject_cast<QStandardItemModel *>(m_ui->codexModeCombo->model());
        model != nullptr && model->item(1) != nullptr)
    {
        model->item(1)->setEnabled(client->planSupported());
    }
}

void ShijimaManager::updateCodexPagePlan(CodexPlanSnapshot const& snapshot)
{
    if (m_ui->codexPlanSteps == nullptr) return;
    m_ui->codexPlanSteps->clear();
    for (auto const& step : snapshot.steps) {
        auto *item = new QListWidgetItem(
            QStringLiteral("[%1] %2").arg(step.status, step.step), m_ui->codexPlanSteps);
        item->setToolTip(step.step);
    }
    QString text = snapshot.finalText.trimmed();
    if (text.isEmpty()) text = snapshot.explanation.trimmed();
    m_ui->codexFinalLabel->setText(text);
    updateCodexPageState();
}

void ShijimaManager::updateCodexPageApprovals()
{
    if (m_ui->codexApprovalList == nullptr || m_runtime->codexClient == nullptr) return;
    auto pending = m_runtime->codexClient->pendingApprovals();
    if (!m_ui->codexKey.isEmpty()) {
        setNodeKeyPoints(m_ui->codexKey, pending.size());
    }
    m_ui->codexApprovalList->blockSignals(true);
    m_ui->codexApprovalList->clear();
    for (auto const& request : pending) {
        auto *item = new QListWidgetItem(
            tr("%1 — %2").arg(approvalTypeText(request.kind),
                request.reason.trimmed().isEmpty() ? tr("Confirmation required") : request.reason.trimmed()),
            m_ui->codexApprovalList);
        item->setData(Qt::UserRole, requestKey(request.requestId));
    }
    m_ui->codexApprovalList->blockSignals(false);
    int selected = -1;
    for (int i = 0; i < m_ui->codexApprovalList->count(); ++i) {
        if (m_ui->codexApprovalList->item(i)->data(Qt::UserRole).toString() == m_ui->codexSelectedApprovalKey) {
            selected = i; break;
        }
    }
    if (selected < 0 && m_ui->codexApprovalList->count() > 0) selected = 0;
    if (selected >= 0) m_ui->codexApprovalList->setCurrentRow(selected);
    bool hasSelection = selected >= 0 && !m_ui->codexSelectedApprovalKey.isEmpty();
    CodexApprovalRequest const* selectedRequest = nullptr;
    for (auto const& request : pending) {
        if (requestKey(request.requestId) == m_ui->codexSelectedApprovalKey) {
            selectedRequest = &request; break;
        }
    }
    if (selectedRequest == nullptr && !pending.isEmpty()) {
        selectedRequest = &pending.first();
        m_ui->codexSelectedApprovalKey = requestKey(selectedRequest->requestId);
        hasSelection = true;
    }
    m_ui->codexApprovalDetailLabel->setText(selectedRequest == nullptr
        ? tr("No pending approval.") : approvalDetails(*selectedRequest));
    auto enabled = [selectedRequest](CodexApprovalDecision decision) {
        if (selectedRequest == nullptr) return false;
        return selectedRequest->availableDecisions.isEmpty() ||
            selectedRequest->availableDecisions.contains(codexApprovalDecisionName(decision));
    };
    m_ui->codexApprovalDeclineButton->setEnabled(hasSelection && enabled(CodexApprovalDecision::Decline));
    m_ui->codexApprovalAcceptButton->setEnabled(hasSelection && enabled(CodexApprovalDecision::Accept));
    m_ui->codexApprovalSessionButton->setEnabled(hasSelection && enabled(CodexApprovalDecision::AcceptForSession));
    m_ui->codexApprovalCancelButton->setEnabled(hasSelection && enabled(CodexApprovalDecision::Cancel));
}

void ShijimaManager::updateCodexPageInputs()
{
    // requestUserInput is displayed as a focused dialog from the signal
    // handler; this helper remains a lifecycle hook for queued updates.
    updateCodexPageState();
}
