#include "shijima-qt/CodexAppServerClient.hpp"
#include "shijima-qt/CodexAppServerProtocol.hpp"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

class CodexAppServerTests final : public QObject {
    Q_OBJECT
private slots:
    void parsesStringAndIntegerIds();
    void rejectsConflictingResponse();
    void requiresRequestParams();
    void serializesDecisions();
    void parsesApprovalsAndQuestions();
    void parsesPlanSnapshots();
    void rejectsMalformedErrors();
    void boundsApprovalStore();
};

void CodexAppServerTests::parsesStringAndIntegerIds() {
    for (QJsonValue id : { QJsonValue(QStringLiteral("request-7")), QJsonValue(7) }) {
        QByteArray payload = codexJsonRpcRequest(id, QStringLiteral("ping"), {});
        CodexJsonRpcMessage message;
        QVERIFY2(parseCodexJsonRpcMessage(payload, message), "request should parse");
        QCOMPARE(message.kind, CodexJsonRpcMessageKind::Request);
        QCOMPARE(message.id, id);
    }
}

void CodexAppServerTests::rejectsConflictingResponse() {
    QJsonObject object {
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("id"), QStringLiteral("x") },
        { QStringLiteral("result"), QJsonObject() },
        { QStringLiteral("error"), QJsonObject() }
    };
    CodexJsonRpcMessage message;
    QVERIFY(!parseCodexJsonRpcMessage(QJsonDocument(object).toJson(QJsonDocument::Compact), message));
}

void CodexAppServerTests::requiresRequestParams() {
    CodexJsonRpcMessage message;
    QVERIFY(!parseCodexJsonRpcMessage(
        QByteArrayLiteral("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}"), message));
}

void CodexAppServerTests::serializesDecisions() {
    QCOMPARE(codexApprovalDecisionName(CodexApprovalDecision::Accept), QStringLiteral("accept"));
    QCOMPARE(codexApprovalDecisionName(CodexApprovalDecision::AcceptForSession), QStringLiteral("acceptForSession"));
    QCOMPARE(codexApprovalDecisionName(CodexApprovalDecision::Decline), QStringLiteral("decline"));
    QCOMPARE(codexApprovalDecisionName(CodexApprovalDecision::Cancel), QStringLiteral("cancel"));
    QJsonDocument document = QJsonDocument::fromJson(
        codexJsonRpcResult(QStringLiteral("id"),
            QJsonObject {{ QStringLiteral("decision"), QStringLiteral("cancel") }}));
    QCOMPARE(document.object().value(QStringLiteral("id")).toString(), QStringLiteral("id"));
    QCOMPARE(document.object().value(QStringLiteral("result")).toObject().value(QStringLiteral("decision")).toString(), QStringLiteral("cancel"));
}

void CodexAppServerTests::parsesApprovalsAndQuestions() {
    CodexApprovalRequest approval;
    QVERIFY(parseCodexApprovalRequest(
        QStringLiteral("item/commandExecution/requestApproval"), QStringLiteral("a"),
        {{ QStringLiteral("threadId"), QStringLiteral("t") },
         { QStringLiteral("turnId"), QStringLiteral("u") },
         { QStringLiteral("command"), QStringLiteral("echo hi") },
         { QStringLiteral("networkApprovalContext"), QJsonObject {{ QStringLiteral("host"), QStringLiteral("example.test") }} }}, approval));
    QCOMPARE(approval.threadId, QStringLiteral("t"));
    QCOMPARE(approval.command, QStringLiteral("echo hi"));
    QVERIFY(!approval.networkContext.isEmpty());
    QCOMPARE(approval.kind, CodexApprovalKind::Network);

    CodexUserInputRequest input;
    QVERIFY(parseCodexUserInputRequest(
        QStringLiteral("item/tool/requestUserInput"), 3,
        {{ QStringLiteral("questions"), QJsonArray {
            QJsonObject {{ QStringLiteral("id"), QStringLiteral("choice") },
                { QStringLiteral("question"), QStringLiteral("Pick one") },
                { QStringLiteral("isOther"), true },
                { QStringLiteral("isSecret"), true },
                { QStringLiteral("options"), QJsonArray { QJsonObject {{ QStringLiteral("label"), QStringLiteral("A") }} } }}
        } }}, input));
    QCOMPARE(input.questions.size(), 1);
    QVERIFY(input.questions.first().isOther);
    QVERIFY(input.questions.first().isSecret);
}

void CodexAppServerTests::parsesPlanSnapshots() {
    CodexPlanSnapshot update;
    QVERIFY(parseCodexPlanSnapshot(
        QStringLiteral("turn/plan/updated"),
        {{ QStringLiteral("threadId"), QStringLiteral("thread") },
         { QStringLiteral("turnId"), QStringLiteral("turn") },
         { QStringLiteral("itemId"), QStringLiteral("item") },
         { QStringLiteral("explanation"), QStringLiteral("Review changes") },
         { QStringLiteral("plan"), QJsonArray {
             QJsonObject {{ QStringLiteral("step"), QStringLiteral("Inspect") },
                 { QStringLiteral("status"), QStringLiteral("inProgress") }}
         } }}, update));
    QCOMPARE(update.threadId, QStringLiteral("thread"));
    QCOMPARE(update.turnId, QStringLiteral("turn"));
    QCOMPARE(update.itemId, QStringLiteral("item"));
    QCOMPARE(update.steps.size(), 1);
    QCOMPARE(update.steps.first().status, QStringLiteral("inProgress"));

    CodexPlanSnapshot completed;
    QVERIFY(parseCodexPlanSnapshot(
        QStringLiteral("item/completed"),
        {{ QStringLiteral("threadId"), QStringLiteral("thread") },
         { QStringLiteral("turnId"), QStringLiteral("turn") },
         { QStringLiteral("item"), QJsonObject {
             { QStringLiteral("id"), QStringLiteral("item") },
             { QStringLiteral("type"), QStringLiteral("plan") },
             { QStringLiteral("text"), QStringLiteral("Final plan") }
         } }}, completed));
    QVERIFY(completed.final);
    QCOMPARE(completed.finalText, QStringLiteral("Final plan"));
}

void CodexAppServerTests::rejectsMalformedErrors() {
    CodexJsonRpcMessage message;
    QVERIFY(!parseCodexJsonRpcMessage(
        QByteArrayLiteral("{\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{}}"), message));
    QVERIFY(parseCodexJsonRpcMessage(
        codexJsonRpcError(1, -32601, QStringLiteral("unsupported")), message));
    QCOMPARE(message.error.value(QStringLiteral("code")).toInt(), -32601);
}

void CodexAppServerTests::boundsApprovalStore() {
    CodexApprovalStore store;
    for (int i = 0; i < CodexApprovalStore::kMaxPending; ++i) {
        CodexApprovalRequest request;
        request.requestId = i;
        QVERIFY(store.insert(request));
    }
    CodexApprovalRequest duplicate;
    duplicate.requestId = 0;
    QVERIFY(!store.insert(duplicate));
    CodexApprovalRequest overflow;
    overflow.requestId = CodexApprovalStore::kMaxPending;
    QVERIFY(!store.insert(overflow));
    QVERIFY(store.take(0));
    QVERIFY(!store.contains(0));
    store.clear();

    CodexApprovalRequest first;
    first.requestId = QStringLiteral("first");
    CodexApprovalRequest second;
    second.requestId = QStringLiteral("second");
    QVERIFY(store.insert(first));
    QVERIFY(store.insert(second));
    QCOMPARE(store.values().first().requestId.toString(), QStringLiteral("first"));
}

QTEST_GUILESS_MAIN(CodexAppServerTests)
#include "CodexAppServerTests.moc"
