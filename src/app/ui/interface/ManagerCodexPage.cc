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
#include <QFormLayout>
#include <QGridLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QResizeEvent>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStyleOptionFocusRect>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QSizePolicy>
#include <QScreen>

#include "ElaIcon.h"
#include "ElaComboBox.h"
#include "ElaDialog.h"
#include "ElaLineEdit.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaRadioButton.h"
#include "ElaScrollArea.h"
#include "ElaScrollBar.h"
#include "ElaText.h"
#include "ElaTheme.h"

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
                if (!actionText.isEmpty()) actionText += pageTr(": ");
                actionText += action.command.trimmed();
            }
            if (!actionText.isEmpty()) lines << pageTr("  %1").arg(actionText.left(4096));
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
            lines << pageTr("  %1 %2").arg(change.kind, change.path);
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

void drawCodexFocusFrame(QWidget *widget)
{
    if (!widget->hasFocus()) {
        return;
    }

    QStyleOptionFocusRect option;
    option.initFrom(widget);
    option.rect = widget->rect().adjusted(3, 3, -3, -3);
    QPainter painter(widget);
    widget->style()->drawPrimitive(
        QStyle::PE_FrameFocusRect, &option, &painter, widget);
}

class CodexPushButton final : public ElaPushButton {
public:
    using ElaPushButton::ElaPushButton;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ElaPushButton::paintEvent(event);
        drawCodexFocusFrame(this);
    }

private:
    Q_DISABLE_COPY_MOVE(CodexPushButton)
};

CodexPushButton *makeButton(QWidget *parent, QString const& text,
    QString const& description)
{
    auto *button = new CodexPushButton(text, parent);
    button->setAccessibleName(text);
    button->setAccessibleDescription(description);
    button->setFocusPolicy(Qt::StrongFocus);
    button->setToolTip(description);
    button->setMinimumHeight(38);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setAutoDefault(false);
    button->setDefault(false);
    return button;
}

void configurePrimaryButton(CodexPushButton *button)
{
    button->setLightDefaultColor(ElaThemeColor(ElaThemeType::Light, PrimaryNormal));
    button->setLightHoverColor(ElaThemeColor(ElaThemeType::Light, PrimaryHover));
    button->setLightPressColor(ElaThemeColor(ElaThemeType::Light, PrimaryPress));
    button->setLightTextColor(ElaThemeColor(ElaThemeType::Light, BasicTextInvert));
    button->setDarkDefaultColor(ElaThemeColor(ElaThemeType::Dark, PrimaryNormal));
    button->setDarkHoverColor(ElaThemeColor(ElaThemeType::Dark, PrimaryHover));
    button->setDarkPressColor(ElaThemeColor(ElaThemeType::Dark, PrimaryPress));
    button->setDarkTextColor(ElaThemeColor(ElaThemeType::Dark, BasicTextInvert));
}

void configureDangerButton(CodexPushButton *button)
{
    button->setLightDefaultColor(ElaThemeColor(ElaThemeType::Light, StatusDanger));
    button->setLightHoverColor(ElaThemeColor(ElaThemeType::Light, StatusDanger));
    button->setLightPressColor(ElaThemeColor(ElaThemeType::Light, StatusDanger));
    button->setLightTextColor(ElaThemeColor(ElaThemeType::Light, BasicTextInvert));
    button->setDarkDefaultColor(ElaThemeColor(ElaThemeType::Dark, StatusDanger));
    button->setDarkHoverColor(ElaThemeColor(ElaThemeType::Dark, StatusDanger));
    button->setDarkPressColor(ElaThemeColor(ElaThemeType::Dark, StatusDanger));
    button->setDarkTextColor(ElaThemeColor(ElaThemeType::Dark, BasicTextInvert));
}

QFrame *makeCodexCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("codexCard"));
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    return card;
}

ElaText *makeCodexSectionTitle(QWidget *parent, QString const& text)
{
    auto *title = new ElaText(text, parent);
    title->setObjectName(QStringLiteral("codexSectionTitle"));
    title->setTextPixelSize(17);
    title->setWordWrap(false);
    title->setStyleSheet(QStringLiteral(
        "#codexSectionTitle { background-color: transparent; border: none; }"));
    return title;
}

QLabel *makeCodexSectionDescription(QWidget *parent, QString const& text)
{
    auto *description = new QLabel(text, parent);
    description->setObjectName(QStringLiteral("codexSectionDescription"));
    description->setWordWrap(true);
    description->setTextFormat(Qt::PlainText);
    return description;
}

QString shortIdentifier(QString const& value)
{
    QString id = value.trimmed();
    if (id.size() <= 16) {
        return id;
    }
    return id.left(8) + QStringLiteral("…") + id.right(6);
}

void applyCodexTheme(QWidget *page)
{
    auto mode = eTheme->getThemeMode();
    QColor panel = ElaThemeColor(mode, BasicBase);
    QColor inset = ElaThemeColor(mode, DialogLayoutArea);
    QColor border = ElaThemeColor(mode, BasicBorder);
    QColor text = ElaThemeColor(mode, BasicText);
    QColor muted = ElaThemeColor(mode, BasicDetailsText);
    QColor disabled = ElaThemeColor(mode, BasicTextDisable);
    QColor accent = ElaThemeColor(mode, PrimaryNormal);
    QColor selected = accent;
    selected.setAlpha(38);
    page->setStyleSheet(QString(
        "QScrollArea#codexPage, QScrollArea#codexPage > QWidget > QWidget {"
        " background: transparent; border: none; }"
        "#codexContent { background: transparent; }"
        "#codexCard { background-color: %1; border: 1px solid %2; border-radius: 12px; }"
        "#codexInset { background-color: %3; border: 1px solid %2; border-radius: 8px; }"
        "#codexSectionDescription, #codexHeaderDescription, #codexStatusCaption,"
        "#codexDiagnostic { color: %5; background: transparent; border: none; }"
        "#codexStatusLabel { color: %4; font-weight: 600; }"
        "#codexStatusValue { color: %4; background: transparent; border: none; }"
        "QListWidget#codexApprovalList, QListWidget#codexPlanSteps,"
        "QPlainTextEdit#codexInputEdit { background-color: %3; color: %4;"
        " border: 1px solid %2; border-radius: 8px; }"
        "QPlainTextEdit#codexInputEdit:disabled { color: %5; }"
        "QListWidget#codexApprovalList::item:selected, QListWidget#codexPlanSteps::item:selected"
        " { background-color: %8; }"
        "QListWidget#codexApprovalList::item, QListWidget#codexPlanSteps::item"
        " { padding: 7px 8px; }"
        "QListWidget#codexApprovalList, QListWidget#codexPlanSteps { color: %4; }"
        "QListWidget#codexApprovalList:disabled, QListWidget#codexPlanSteps:disabled"
        " { color: %6; }"
        "QLabel#codexEmptyState { color: %4; background: transparent; border: none; }"
        "QLabel#codexEmptyState[emptyState=\"true\"] { color: %5; }"
    ).arg(panel.name(QColor::HexArgb), border.name(QColor::HexArgb),
        inset.name(QColor::HexArgb), text.name(QColor::HexArgb),
        muted.name(QColor::HexArgb), disabled.name(QColor::HexArgb),
        accent.name(QColor::HexArgb),
        selected.name(QColor::HexArgb)));

    auto applyPalette = [inset, text, muted, disabled, accent](QWidget *widget) {
        QPalette palette = widget->palette();
        palette.setColor(QPalette::Base, inset);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::PlaceholderText, muted);
        palette.setColor(QPalette::Highlight, accent);
        palette.setColor(QPalette::HighlightedText, text);
        palette.setColor(QPalette::Disabled, QPalette::Base, inset);
        palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
        palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, disabled);
        widget->setPalette(palette);
    };
    for (auto *widget : page->findChildren<QPlainTextEdit *>()) {
        applyPalette(widget);
    }
    for (auto *widget : page->findChildren<QListWidget *>()) {
        applyPalette(widget);
    }
    for (auto *widget : page->findChildren<QComboBox *>()) {
        applyPalette(widget);
    }
}

class ResponsiveCodexActionRow final : public QWidget {
public:
    explicit ResponsiveCodexActionRow(QWidget *parent = nullptr,
        int compactWidth = 680):
        QWidget(parent),
        m_compactWidth(compactWidth),
        m_layout(new QBoxLayout(QBoxLayout::LeftToRight, this))
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(8);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    }

    void addButton(QWidget *button)
    {
        m_layout->addWidget(button);
    }

    void addStretch()
    {
        m_layout->addStretch();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        bool compact = width() > 0 && width() < m_compactWidth;
        m_layout->setDirection(compact
            ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    }

private:
    int m_compactWidth;
    QBoxLayout *m_layout;

    Q_DISABLE_COPY_MOVE(ResponsiveCodexActionRow)
};

QString requestKey(QJsonValue const& id)
{
    return codexApprovalRequestIdKey(id);
}

} // namespace

void ShijimaManager::setupCodexPage()
{
    auto *page = new ElaScrollArea(this);
    page->setObjectName(QStringLiteral("codexPage"));
    page->setWidgetResizable(true);
    page->setFrameShape(QFrame::NoFrame);
    page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    page->setFocusPolicy(Qt::StrongFocus);
    m_ui->codexPage = page;

    auto *content = new QWidget(page);
    content->setObjectName(QStringLiteral("codexContent"));
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *root = new QVBoxLayout(content);
    root->setContentsMargins(20, 18, 20, 24);
    root->setSpacing(14);
    root->setAlignment(Qt::AlignTop);
    page->setWidget(content);

    auto *title = new ElaText(tr("Codex"), content);
    title->setTextPixelSize(23);
    title->setWordWrap(false);
    title->setStyleSheet(QStringLiteral(
        "#ElaText { background-color: transparent; border: none; }"));
    root->addWidget(title);
    auto *description = new QLabel(
        tr("Connect to a private Codex app-server session and review plans, replies, and approvals."),
        content);
    description->setObjectName(QStringLiteral("codexHeaderDescription"));
    description->setWordWrap(true);
    description->setTextFormat(Qt::PlainText);
    root->addWidget(description);

    auto *statusBox = makeCodexCard(content);
    auto *statusLayout = new QGridLayout(statusBox);
    statusLayout->setContentsMargins(16, 14, 16, 14);
    statusLayout->setHorizontalSpacing(18);
    statusLayout->setVerticalSpacing(8);
    statusLayout->setColumnStretch(1, 1);
    statusLayout->addWidget(makeCodexSectionTitle(statusBox, tr("Session")), 0, 0, 1, 2);
    auto *statusCaption = makeCodexSectionDescription(statusBox,
        tr("One explicitly managed thread at a time. Nothing starts until you choose Connect."));
    statusCaption->setObjectName(QStringLiteral("codexStatusCaption"));
    statusLayout->addWidget(statusCaption, 1, 0, 1, 2);
    auto addStatus = [statusLayout, statusBox](int row, QString const& label, QLabel **out) {
        auto *labelWidget = new QLabel(label, statusBox);
        labelWidget->setObjectName(QStringLiteral("codexStatusLabel"));
        statusLayout->addWidget(labelWidget, row, 0, Qt::AlignTop);
        *out = new QLabel(statusBox);
        (*out)->setObjectName(QStringLiteral("codexStatusValue"));
        (*out)->setTextFormat(Qt::PlainText);
        (*out)->setTextInteractionFlags(Qt::TextSelectableByMouse);
        (*out)->setWordWrap(true);
        (*out)->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        statusLayout->addWidget(*out, row, 1);
    };
    addStatus(2, tr("Status"), &m_ui->codexStateLabel);
    addStatus(3, tr("Thread"), &m_ui->codexThreadLabel);
    addStatus(4, tr("Workspace"), &m_ui->codexWorkspaceLabel);
    addStatus(5, tr("Turn"), &m_ui->codexTurnLabel);
    addStatus(6, tr("Mode"), &m_ui->codexPlanLabel);
    root->addWidget(statusBox);

    auto *sessionBox = makeCodexCard(content);
    auto *sessionLayout = new QVBoxLayout(sessionBox);
    sessionLayout->setContentsMargins(16, 14, 16, 14);
    sessionLayout->setSpacing(8);
    sessionLayout->addWidget(makeCodexSectionTitle(sessionBox, tr("Connection")));
    sessionLayout->addWidget(makeCodexSectionDescription(sessionBox,
        tr("Start, stop, or explicitly choose which saved thread to use.")));
    auto *sessionButtons = new ResponsiveCodexActionRow(sessionBox);
    m_ui->codexConnectButton = makeButton(sessionBox, tr("Connect Codex"),
        tr("Start the Codex app-server after an explicit click."));
    m_ui->codexNewThreadButton = makeButton(sessionBox, tr("New session"),
        tr("Create a new app-server thread."));
    m_ui->codexResumeButton = makeButton(sessionBox, tr("Resume recent"),
        tr("Resume the explicitly saved recent thread."));
    configurePrimaryButton(static_cast<CodexPushButton *>(m_ui->codexConnectButton));
    sessionButtons->addButton(m_ui->codexConnectButton);
    sessionButtons->addButton(m_ui->codexNewThreadButton);
    sessionButtons->addButton(m_ui->codexResumeButton);
    sessionButtons->addStretch();
    sessionLayout->addWidget(sessionButtons);
    root->addWidget(sessionBox);

    auto *approvalBox = makeCodexCard(content);
    auto *approvalLayout = new QVBoxLayout(approvalBox);
    approvalLayout->setContentsMargins(16, 14, 16, 14);
    approvalLayout->setSpacing(8);
    approvalLayout->addWidget(makeCodexSectionTitle(approvalBox, tr("Approvals")));
    approvalLayout->addWidget(makeCodexSectionDescription(approvalBox,
        tr("Review each request before choosing a decision. Nothing is approved automatically.")));
    m_ui->codexApprovalList = new QListWidget(approvalBox);
    m_ui->codexApprovalList->setObjectName(QStringLiteral("codexApprovalList"));
    m_ui->codexApprovalList->setAccessibleName(tr("Pending Codex approvals"));
    m_ui->codexApprovalList->setAccessibleDescription(
        tr("Select a pending request before choosing a decision."));
    m_ui->codexApprovalList->setMinimumHeight(74);
    m_ui->codexApprovalList->setMaximumHeight(170);
    m_ui->codexApprovalList->setFrameShape(QFrame::NoFrame);
    m_ui->codexApprovalList->setVerticalScrollBar(
        new ElaScrollBar(m_ui->codexApprovalList));
    m_ui->codexApprovalList->setHorizontalScrollBar(
        new ElaScrollBar(m_ui->codexApprovalList));
    m_ui->codexApprovalList->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);
    approvalLayout->addWidget(m_ui->codexApprovalList);
    auto *approvalDetail = new QFrame(approvalBox);
    approvalDetail->setObjectName(QStringLiteral("codexInset"));
    auto *approvalDetailLayout = new QVBoxLayout(approvalDetail);
    approvalDetailLayout->setContentsMargins(12, 10, 12, 10);
    m_ui->codexApprovalDetailLabel = new QLabel(approvalDetail);
    m_ui->codexApprovalDetailLabel->setObjectName(QStringLiteral("codexEmptyState"));
    m_ui->codexApprovalDetailLabel->setTextFormat(Qt::PlainText);
    m_ui->codexApprovalDetailLabel->setWordWrap(true);
    m_ui->codexApprovalDetailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    approvalDetailLayout->addWidget(m_ui->codexApprovalDetailLabel);
    approvalLayout->addWidget(approvalDetail);
    auto *approvalButtons = new ResponsiveCodexActionRow(approvalBox);
    m_ui->codexApprovalDeclineButton = makeButton(approvalBox, tr("Decline"),
        tr("Reject this operation and let the agent continue."));
    m_ui->codexApprovalAcceptButton = makeButton(approvalBox, tr("Allow once"),
        tr("Allow only this operation."));
    m_ui->codexApprovalSessionButton = makeButton(approvalBox, tr("Allow for session"),
        tr("Allow this operation for the current app-server session."));
    m_ui->codexApprovalCancelButton = makeButton(approvalBox, tr("Decline and stop"),
        tr("Reject this operation and interrupt the current turn."));
    configureDangerButton(static_cast<CodexPushButton *>(m_ui->codexApprovalCancelButton));
    approvalButtons->addButton(m_ui->codexApprovalDeclineButton);
    approvalButtons->addButton(m_ui->codexApprovalAcceptButton);
    approvalButtons->addButton(m_ui->codexApprovalSessionButton);
    approvalButtons->addButton(m_ui->codexApprovalCancelButton);
    approvalButtons->addStretch();
    approvalLayout->addWidget(approvalButtons);
    root->addWidget(approvalBox);

    auto *planBox = makeCodexCard(content);
    auto *planLayout = new QVBoxLayout(planBox);
    planLayout->setContentsMargins(16, 14, 16, 14);
    planLayout->setSpacing(8);
    planLayout->addWidget(makeCodexSectionTitle(planBox, tr("Plan and response")));
    planLayout->addWidget(makeCodexSectionDescription(planBox,
        tr("Plan steps and the latest final response stay here for review.")));
    m_ui->codexPlanSteps = new QListWidget(planBox);
    m_ui->codexPlanSteps->setObjectName(QStringLiteral("codexPlanSteps"));
    m_ui->codexPlanSteps->setAccessibleName(tr("Codex plan steps"));
    m_ui->codexPlanSteps->setMinimumHeight(96);
    m_ui->codexPlanSteps->setMaximumHeight(220);
    m_ui->codexPlanSteps->setFrameShape(QFrame::NoFrame);
    m_ui->codexPlanSteps->setVerticalScrollBar(
        new ElaScrollBar(m_ui->codexPlanSteps));
    m_ui->codexPlanSteps->setHorizontalScrollBar(
        new ElaScrollBar(m_ui->codexPlanSteps));
    m_ui->codexPlanSteps->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);
    planLayout->addWidget(m_ui->codexPlanSteps);
    auto *finalFrame = new QFrame(planBox);
    finalFrame->setObjectName(QStringLiteral("codexInset"));
    auto *finalLayout = new QVBoxLayout(finalFrame);
    finalLayout->setContentsMargins(12, 10, 12, 10);
    m_ui->codexFinalLabel = new QLabel(finalFrame);
    m_ui->codexFinalLabel->setObjectName(QStringLiteral("codexEmptyState"));
    m_ui->codexFinalLabel->setTextFormat(Qt::PlainText);
    m_ui->codexFinalLabel->setWordWrap(true);
    m_ui->codexFinalLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_ui->codexFinalLabel->setMinimumHeight(58);
    finalLayout->addWidget(m_ui->codexFinalLabel);
    planLayout->addWidget(finalFrame);
    root->addWidget(planBox);

    auto *replyBox = makeCodexCard(content);
    auto *replyLayout = new QVBoxLayout(replyBox);
    replyLayout->setContentsMargins(16, 14, 16, 14);
    replyLayout->setSpacing(8);
    replyLayout->addWidget(makeCodexSectionTitle(replyBox, tr("Message")));
    replyLayout->addWidget(makeCodexSectionDescription(replyBox,
        tr("Send a message to the current thread, or steer an active turn.")));
    auto *modeRow = new QHBoxLayout;
    auto *modeLabel = new QLabel(tr("Mode"), replyBox);
    modeLabel->setObjectName(QStringLiteral("codexStatusLabel"));
    modeRow->addWidget(modeLabel);
    m_ui->codexModeCombo = new ElaComboBox(replyBox);
    m_ui->codexModeCombo->setObjectName(QStringLiteral("codexModeCombo"));
    m_ui->codexModeCombo->addItem(tr("Default"), false);
    m_ui->codexModeCombo->addItem(tr("Plan"), true);
    m_ui->codexModeCombo->setAccessibleName(tr("Codex mode"));
    m_ui->codexModeCombo->setAccessibleDescription(tr("Choose Default or Plan mode for the next turn."));
    m_ui->codexModeCombo->setMinimumWidth(150);
    modeRow->addWidget(m_ui->codexModeCombo);
    modeRow->addStretch();
    replyLayout->addLayout(modeRow);
    m_ui->codexInputEdit = new ElaPlainTextEdit(replyBox);
    m_ui->codexInputEdit->setObjectName(QStringLiteral("codexInputEdit"));
    m_ui->codexInputEdit->setPlaceholderText(tr("Ask Codex something..."));
    m_ui->codexInputEdit->setAccessibleName(tr("Codex message"));
    m_ui->codexInputEdit->setAccessibleDescription(tr("Enter a message to send to the current thread."));
    m_ui->codexInputEdit->setMinimumHeight(82);
    m_ui->codexInputEdit->setMaximumHeight(180);
    replyLayout->addWidget(m_ui->codexInputEdit);
    auto *replyButtons = new ResponsiveCodexActionRow(replyBox);
    m_ui->codexSendButton = makeButton(replyBox, tr("Send"),
        tr("Send the message or steer the active turn."));
    m_ui->codexApplyPlanButton = makeButton(replyBox, tr("Implement this plan"),
        tr("Start a Default mode turn using the confirmed plan."));
    m_ui->codexModifyPlanButton = makeButton(replyBox, tr("Modify plan"),
        tr("Ask Codex to revise the current plan."));
    m_ui->codexInterruptButton = makeButton(replyBox, tr("Abort task"),
        tr("Interrupt the active Codex turn."));
    configurePrimaryButton(static_cast<CodexPushButton *>(m_ui->codexSendButton));
    configureDangerButton(static_cast<CodexPushButton *>(m_ui->codexInterruptButton));
    replyButtons->addButton(m_ui->codexSendButton);
    replyButtons->addButton(m_ui->codexApplyPlanButton);
    replyButtons->addButton(m_ui->codexModifyPlanButton);
    replyButtons->addButton(m_ui->codexInterruptButton);
    replyButtons->addStretch();
    replyLayout->addWidget(replyButtons);
    root->addWidget(replyBox);

    auto *diagnosticBox = makeCodexCard(content);
    diagnosticBox->setObjectName(QStringLiteral("codexDiagnosticCard"));
    m_ui->codexDiagnosticCard = diagnosticBox;
    auto *diagnosticLayout = new QVBoxLayout(diagnosticBox);
    diagnosticLayout->setContentsMargins(14, 10, 14, 10);
    m_ui->codexDiagnosticLabel = new QLabel(diagnosticBox);
    m_ui->codexDiagnosticLabel->setObjectName(QStringLiteral("codexDiagnostic"));
    m_ui->codexDiagnosticLabel->setTextFormat(Qt::PlainText);
    m_ui->codexDiagnosticLabel->setWordWrap(true);
    m_ui->codexDiagnosticLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    diagnosticLayout->addWidget(m_ui->codexDiagnosticLabel);
    diagnosticBox->setVisible(false);
    root->addWidget(diagnosticBox);
    root->addStretch();

    setTabOrder(m_ui->codexConnectButton, m_ui->codexNewThreadButton);
    setTabOrder(m_ui->codexNewThreadButton, m_ui->codexResumeButton);
    setTabOrder(m_ui->codexResumeButton, m_ui->codexApprovalList);
    setTabOrder(m_ui->codexApprovalList, m_ui->codexApprovalDeclineButton);
    setTabOrder(m_ui->codexApprovalDeclineButton, m_ui->codexApprovalAcceptButton);
    setTabOrder(m_ui->codexApprovalAcceptButton, m_ui->codexApprovalSessionButton);
    setTabOrder(m_ui->codexApprovalSessionButton, m_ui->codexApprovalCancelButton);
    setTabOrder(m_ui->codexApprovalCancelButton, m_ui->codexPlanSteps);
    setTabOrder(m_ui->codexPlanSteps, m_ui->codexModeCombo);
    setTabOrder(m_ui->codexModeCombo, m_ui->codexInputEdit);
    setTabOrder(m_ui->codexInputEdit, m_ui->codexSendButton);
    setTabOrder(m_ui->codexSendButton, m_ui->codexApplyPlanButton);
    setTabOrder(m_ui->codexApplyPlanButton, m_ui->codexModifyPlanButton);
    setTabOrder(m_ui->codexModifyPlanButton, m_ui->codexInterruptButton);

    applyCodexTheme(page);
    connect(eTheme, &ElaTheme::themeModeChanged, page, [page]() {
        applyCodexTheme(page);
    });

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
                for (auto const& step : snapshot.steps.mid(0, 6)) {
                    summary << tr("• %1").arg(step.step);
                }
                QString key = snapshot.threadId + QLatin1Char('\x1f') +
                    snapshot.turnId + QLatin1Char('\x1f') + snapshot.itemId;
                showCodexAppServerBubble(tr("Codex · Plan completed"),
                    summary.join(QLatin1Char('\n')), key);
            }
        }, Qt::QueuedConnection);
    connect(client, &CodexAppServerClient::finalMessageReady, this,
        [this, client](QString const& text) {
            QString finalText = text.trimmed();
            m_ui->codexFinalLabel->setText(finalText.isEmpty()
                ? tr("The task completed without a reply to display.") : finalText);
            updateCodexPageState();
            if (m_settings->value(QStringLiteral("codex/planBubbleEnabled"), true).toBool()) {
                showCodexAppServerBubble(tr("Codex · Completed"), finalText.left(4096),
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
            ElaDialog dialog(this);
            dialog.setWindowTitle(tr("Codex needs input"));
            dialog.setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
            dialog.setIsFixedSize(false);
            dialog.setMinimumSize(420, 220);
            dialog.setModal(true);
            auto *dialogLayout = new QVBoxLayout(&dialog);
            dialogLayout->setContentsMargins(20, 16, 20, 14);
            dialogLayout->setSpacing(8);
            auto *dialogTitle = new QLabel(tr("Codex needs input"), &dialog);
            QFont titleFont = dialogTitle->font();
            titleFont.setPointSizeF(qMax(11.0, titleFont.pointSizeF() + 2.0));
            titleFont.setWeight(QFont::DemiBold);
            dialogTitle->setFont(titleFont);
            dialogTitle->setWordWrap(true);
            dialogLayout->addWidget(dialogTitle);
            auto *contentWidget = new QWidget(&dialog);
            auto *contentLayout = new QVBoxLayout(contentWidget);
            contentLayout->setContentsMargins(0, 0, 0, 0);
            contentLayout->setSpacing(10);
            QList<QPair<CodexUserInputQuestion, QObject*>> editors;
            QList<QWidget *> focusOrder;
            for (auto const& question : request.questions.mid(0, 3)) {
                auto *group = new QFrame(&dialog);
                group->setObjectName(QStringLiteral("codexInputQuestionCard"));
                auto applyQuestionTheme = [group](ElaThemeType::ThemeMode mode) {
                    group->setStyleSheet(QString(
                        "#codexInputQuestionCard { background-color: %1;"
                        " border: 1px solid %2; border-radius: 8px; }")
                        .arg(ElaThemeColor(mode, BasicBase).name(QColor::HexArgb),
                            ElaThemeColor(mode, BasicBorder).name(QColor::HexArgb)));
                };
                applyQuestionTheme(eTheme->getThemeMode());
                connect(eTheme, &ElaTheme::themeModeChanged, group,
                    applyQuestionTheme);
                auto *groupLayout = new QVBoxLayout(group);
                groupLayout->setContentsMargins(14, 12, 14, 12);
                groupLayout->setSpacing(8);
                auto *header = new ElaText(question.header.isEmpty()
                    ? tr("Question") : question.header, group);
                header->setTextStyle(ElaTextType::Subtitle);
                groupLayout->addWidget(header);
                auto *label = new QLabel(question.question, group);
                label->setTextFormat(Qt::PlainText);
                label->setWordWrap(true);
                groupLayout->addWidget(label);
                QButtonGroup *buttons = new QButtonGroup(group);
                for (auto const& option : question.options.mid(0, 3)) {
                    auto *radio = new ElaRadioButton(option.label, group);
                    radio->setToolTip(option.description);
                    buttons->addButton(radio);
                    groupLayout->addWidget(radio);
                    focusOrder.append(radio);
                }
                QObject *editor = buttons;
                if (question.isOther) {
                    auto *other = new ElaLineEdit(group);
                    other->setPlaceholderText(tr("Other..."));
                    if (question.isSecret) other->setEchoMode(QLineEdit::Password);
                    other->setMinimumHeight(36);
                    groupLayout->addWidget(other);
                    focusOrder.append(other);
                    editor = other;
                }
                contentLayout->addWidget(group);
                editors.append({ question, editor });
            }
            contentWidget->setSizePolicy(QSizePolicy::Expanding,
                QSizePolicy::Preferred);
            auto *questionScroll = new ElaScrollArea(&dialog);
            questionScroll->setFrameShape(QFrame::NoFrame);
            questionScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            questionScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            questionScroll->setWidgetResizable(true);
            questionScroll->setSizePolicy(QSizePolicy::Expanding,
                QSizePolicy::Preferred);
            questionScroll->setWidget(contentWidget);
            dialogLayout->addWidget(questionScroll);
            auto *actionRow = new QHBoxLayout;
            actionRow->setContentsMargins(0, 6, 0, 0);
            actionRow->setSpacing(8);
            actionRow->addStretch();
            auto *cancelButton = makeButton(&dialog, tr("Cancel"),
                tr("Cancel answering this question."));
            auto *submitButton = makeButton(&dialog, tr("Submit"),
                tr("Submit the selected answers."));
            configurePrimaryButton(submitButton);
            actionRow->addWidget(cancelButton);
            actionRow->addWidget(submitButton);
            dialogLayout->addLayout(actionRow);
            focusOrder.append(cancelButton);
            focusOrder.append(submitButton);
            for (int i = 1; i < focusOrder.size(); ++i) {
                dialog.setTabOrder(focusOrder.at(i - 1), focusOrder.at(i));
            }
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
            connect(submitButton, &QPushButton::clicked, &dialog, submit);
            connect(cancelButton, &QPushButton::clicked, &dialog,
                [this, &dialog, &answered, request]() {
                if (answered) return;
                answered = true;
                m_runtime->codexClient->resolveUserInput(request.requestId, {});
                dialog.reject();
            });
            connect(&dialog, &ElaDialog::closeButtonClicked, &dialog,
                [this, &dialog, &answered, request]() {
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
            auto applyDialogTheme = [&dialog, dialogTitle](
                ElaThemeType::ThemeMode mode) {
                QPalette palette = dialog.palette();
                palette.setColor(QPalette::Window,
                    ElaThemeColor(mode, DialogBase));
                palette.setColor(QPalette::WindowText,
                    ElaThemeColor(mode, BasicText));
                palette.setColor(QPalette::Text,
                    ElaThemeColor(mode, BasicText));
                palette.setColor(QPalette::Base,
                    ElaThemeColor(mode, DialogBase));
                dialog.setPalette(palette);
                dialogTitle->setPalette(palette);
            };
            applyDialogTheme(eTheme->getThemeMode());
            connect(eTheme, &ElaTheme::themeModeChanged, &dialog,
                applyDialogTheme);
            contentWidget->adjustSize();
            dialogLayout->activate();
            dialog.adjustSize();
            QRect available;
            if (auto *screen = dialog.screen(); screen != nullptr) {
                available = screen->availableGeometry();
            }
            else if (auto *screen = QGuiApplication::primaryScreen(); screen != nullptr) {
                available = screen->availableGeometry();
            }
            if (available.isEmpty()) {
                available = QRect(0, 0, 1280, 720);
            }
            int maxWidth = qMax(420, qMin(700, available.width() - 48));
            int maxHeight = qMax(220, qMin(560,
                qRound(available.height() * 0.72)));
            dialog.setMaximumSize(maxWidth, maxHeight);
            QSize contentHint = contentWidget->sizeHint();
            int maxQuestionHeight = qMax(96, qMin(360,
                qRound(available.height() * 0.48)));
            int naturalQuestionHeight = qBound(96, contentHint.height(),
                maxQuestionHeight);
            questionScroll->setMinimumHeight(naturalQuestionHeight);
            questionScroll->setMaximumHeight(naturalQuestionHeight);
            int naturalHeight = naturalQuestionHeight +
                actionRow->sizeHint().height() + dialogTitle->sizeHint().height() + 58;
            int naturalWidth = qMax(420, contentHint.width() + 40);
            dialog.resize(qBound(420, naturalWidth, maxWidth),
                qBound(220, naturalHeight, maxHeight));
            cancelButton->setFocus(Qt::OtherFocusReason);
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
        QString text = message.trimmed();
        m_ui->codexDiagnosticLabel->setText(text);
        if (m_ui->codexDiagnosticCard != nullptr) {
            m_ui->codexDiagnosticCard->setVisible(!text.isEmpty());
        }
    }
}

void ShijimaManager::updateCodexPageState()
{
    if (m_runtime == nullptr || m_runtime->codexClient == nullptr || m_ui->codexPage == nullptr) return;
    auto *client = m_runtime->codexClient.get();
    CodexServerState state = client->state();
    bool featureEnabled = m_settings->value(QStringLiteral("codex/appServerEnabled"), false).toBool();
    m_ui->codexStateLabel->setText(stateText(state));
    QString threadId = client->threadId().trimmed();
    m_ui->codexThreadLabel->setText(threadId.isEmpty()
        ? tr("No active thread") : shortIdentifier(threadId));
    m_ui->codexThreadLabel->setToolTip(threadId);
    QString workspace = client->workspace().trimmed();
    QString workspaceName = QFileInfo(workspace).fileName();
    if (workspaceName.isEmpty()) workspaceName = workspace;
    m_ui->codexWorkspaceLabel->setText(workspace.isEmpty()
        ? tr("No workspace selected") : workspaceName);
    m_ui->codexWorkspaceLabel->setToolTip(workspace);
    m_ui->codexTurnLabel->setText(client->turnId().isEmpty()
        ? tr("No active turn") : client->turnId());
    m_ui->codexPlanLabel->setText(client->planSupported() ? tr("Plan supported") : tr("Default only"));
    m_ui->codexConnectButton->setText(state == CodexServerState::Stopped || state == CodexServerState::Blocked
        ? tr("Connect Codex") : tr("Disconnect"));
    m_ui->codexConnectButton->setEnabled(featureEnabled);
    if (!featureEnabled) {
        showCodexDiagnostic(tr("Enable Codex interaction in Settings before connecting."));
    }
    else if (m_ui->codexDiagnosticLabel->text() ==
        tr("Enable Codex interaction in Settings before connecting.")) {
        showCodexDiagnostic({});
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
            tr("[%1] %2").arg(step.status, step.step), m_ui->codexPlanSteps);
        item->setToolTip(step.step);
    }
    QString text = snapshot.finalText.trimmed();
    if (text.isEmpty()) text = snapshot.explanation.trimmed();
    m_ui->codexFinalLabel->setText(text.isEmpty()
        ? tr("No plan or final response yet. Start a turn to see it here.") : text);
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
        ? tr("No pending approvals. Requests that need your decision will appear here.")
        : approvalDetails(*selectedRequest));
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
