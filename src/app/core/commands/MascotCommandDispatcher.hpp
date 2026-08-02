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

#include "shijima-qt/MascotApi.hpp"
#include "shijima-qt/CodexActivity.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace MascotCommandDispatcher {

namespace Detail {

inline QString const kCommandKey = QStringLiteral("command");
inline QString const kRequestKey = QStringLiteral("request");
inline QString const kPatchKey = QStringLiteral("patch");
inline QString const kSelectorKey = QStringLiteral("selector");
inline QString const kMascotIdKey = QStringLiteral("mascot_id");
inline QString const kLabelKey = QStringLiteral("label");
inline QString const kArchivePathKey = QStringLiteral("archive_path");
inline QString const kMascotNameKey = QStringLiteral("mascot_name");
inline QString const kPayloadKey = QStringLiteral("payload");

inline QJsonObject invalidRequest(QString const& message) {
    QJsonObject object;
    object["error"] = message;
    object["code"] = QStringLiteral("bad_request");
    object["status"] = 400;
    return object;
}

inline bool jsonInt(QJsonObject const& object, QString const& key, int &value) {
    auto token = object.value(key);
    if (!token.isDouble()) {
        return false;
    }
    value = token.toInt();
    return true;
}

}

// Transport-independent command dispatch keeps HTTP, Local IPC, and tests from
// depending on each other's socket/server details.
template<typename Service>
QJsonObject dispatchRequest(QJsonObject const& request, Service &service)
{
    auto commandValue = request.value(Detail::kCommandKey);
    if (!commandValue.isString()) {
        return Detail::invalidRequest(QStringLiteral("Missing command"));
    }
    auto command = commandValue.toString();

    if (command == QStringLiteral("ping")) {
        return pingInfoToJson(service.ping());
    }

    if (command == QStringLiteral("show_codex_notification")) {
        auto payload = request.value(Detail::kPayloadKey);
        if (!payload.isObject()) {
            return Detail::invalidRequest(QStringLiteral("payload must be an object"));
        }
        CodexActivity activity;
        QString parseError;
        bool recognized = false;
        if (!codexActivityFromJson(payload.toObject(), activity, &recognized,
            &parseError))
        {
            return Detail::invalidRequest(parseError);
        }
        if (!recognized) {
            return QJsonObject {
                { QStringLiteral("handled"), false },
                { QStringLiteral("event_type"), activity.type },
            };
        }
        auto status = service.showCodexNotification(activity);
        if (!status.ok()) {
            return errorToJson(status);
        }
        return QJsonObject {
            { QStringLiteral("handled"), true },
            { QStringLiteral("event_type"), activity.type },
            { QStringLiteral("state"), codexActivityStateName(activity.state) },
        };
    }

    if (command == QStringLiteral("list_mascots")) {
        ListMascotsRequest listRequest;
        auto selectorValue = request.value(Detail::kSelectorKey);
        if (selectorValue.isString()) {
            listRequest.selector = selectorValue.toString();
        }
        QList<MascotInfo> mascots;
        auto status = service.listMascots(listRequest, mascots);
        if (!status.ok()) {
            return errorToJson(status);
        }
        QJsonArray array;
        for (auto const& mascot : mascots) {
            array.append(mascotInfoToJson(mascot));
        }
        QJsonObject object;
        object["mascots"] = array;
        return object;
    }

    if (command == QStringLiteral("list_loaded_mascots")) {
        QList<LoadedMascotInfo> mascots;
        auto status = service.listLoadedMascots(mascots);
        if (!status.ok()) {
            return errorToJson(status);
        }
        QJsonArray array;
        for (auto const& mascot : mascots) {
            array.append(loadedMascotInfoToJson(mascot));
        }
        QJsonObject object;
        object["loaded_mascots"] = array;
        return object;
    }

    if (command == QStringLiteral("import_mascot_template")) {
        auto archivePathValue = request.value(Detail::kArchivePathKey);
        if (!archivePathValue.isString()) {
            return Detail::invalidRequest(QStringLiteral("archive_path must be a string"));
        }
        QList<LoadedMascotInfo> mascots;
        auto status = service.importMascotTemplate(archivePathValue.toString(),
            mascots);
        if (!status.ok()) {
            return errorToJson(status);
        }
        QJsonArray array;
        for (auto const& mascot : mascots) {
            array.append(loadedMascotInfoToJson(mascot));
        }
        QJsonObject object;
        object["loaded_mascots"] = array;
        return object;
    }

    if (command == QStringLiteral("remove_mascot_template")) {
        auto mascotNameValue = request.value(Detail::kMascotNameKey);
        if (!mascotNameValue.isString()) {
            return Detail::invalidRequest(QStringLiteral("mascot_name must be a string"));
        }
        auto status = service.removeMascotTemplate(mascotNameValue.toString());
        return status.ok() ? QJsonObject {} : errorToJson(status);
    }

    if (command == QStringLiteral("spawn_mascot")) {
        SpawnMascotRequest spawnRequest;
        QString parseError;
        if (!spawnMascotRequestFromJson(request.value(Detail::kRequestKey),
            spawnRequest, &parseError))
        {
            return Detail::invalidRequest(parseError);
        }
        MascotInfo mascot;
        auto status = service.spawnMascot(spawnRequest, mascot);
        if (!status.ok()) {
            return errorToJson(status);
        }
        QJsonObject object;
        object["mascot"] = mascotInfoToJson(mascot);
        return object;
    }

    if (command == QStringLiteral("register_cli_label")) {
        RegisterCliLabelRequest labelRequest;
        if (!Detail::jsonInt(request, Detail::kMascotIdKey, labelRequest.mascotId) ||
            labelRequest.mascotId < 0)
        {
            return Detail::invalidRequest(QStringLiteral("mascot_id must be a non-negative integer"));
        }
        auto labelValue = request.value(Detail::kLabelKey);
        if (labelValue.isDouble()) {
            labelRequest.label = labelValue.toInt();
        }
        CliLabelInfo labelInfo;
        auto status = service.registerCliLabel(labelRequest, labelInfo);
        if (!status.ok()) {
            return errorToJson(status);
        }
        return cliLabelInfoToJson(labelInfo);
    }

    if (command == QStringLiteral("get_cli_label")) {
        int label = -1;
        if (!Detail::jsonInt(request, Detail::kLabelKey, label) || label < 0) {
            return Detail::invalidRequest(QStringLiteral("label must be a non-negative integer"));
        }
        CliLabelInfo labelInfo;
        auto status = service.getCliLabel(label, labelInfo);
        if (!status.ok()) {
            return errorToJson(status);
        }
        return cliLabelInfoToJson(labelInfo);
    }

    if (command == QStringLiteral("alter_mascot")) {
        int mascotId = -1;
        if (!Detail::jsonInt(request, Detail::kMascotIdKey, mascotId) ||
            mascotId < 0)
        {
            return Detail::invalidRequest(QStringLiteral("mascot_id must be a non-negative integer"));
        }
        MascotPatch patch;
        QString parseError;
        if (!mascotPatchFromJson(request.value(Detail::kPatchKey), patch,
            &parseError))
        {
            return Detail::invalidRequest(parseError);
        }
        MascotInfo mascot;
        auto status = service.alterMascot(mascotId, patch, mascot);
        if (!status.ok()) {
            QJsonObject object = errorToJson(status);
            object["mascot"] = QJsonValue::Null;
            return object;
        }
        QJsonObject object;
        object["mascot"] = mascotInfoToJson(mascot);
        return object;
    }

    if (command == QStringLiteral("dismiss_mascot")) {
        int mascotId = -1;
        if (!Detail::jsonInt(request, Detail::kMascotIdKey, mascotId) ||
            mascotId < 0)
        {
            return Detail::invalidRequest(QStringLiteral("mascot_id must be a non-negative integer"));
        }
        auto status = service.dismissMascot(mascotId);
        return status.ok() ? QJsonObject {} : errorToJson(status);
    }

    if (command == QStringLiteral("dismiss_all_mascots")) {
        DismissAllMascotsRequest dismissRequest;
        auto selectorValue = request.value(Detail::kSelectorKey);
        if (selectorValue.isString()) {
            dismissRequest.selector = selectorValue.toString();
        }
        auto status = service.dismissAllMascots(dismissRequest);
        return status.ok() ? QJsonObject {} : errorToJson(status);
    }

    if (command == QStringLiteral("stop_runtime")) {
        auto status = service.stopRuntime();
        return status.ok() ? QJsonObject {
            { QStringLiteral("stopped"), true },
        } : errorToJson(status);
    }

    if (command == QStringLiteral("show_manager")) {
        auto status = service.showManagerWindow();
        return status.ok() ? QJsonObject {
            { QStringLiteral("shown"), true },
        } : errorToJson(status);
    }

    return Detail::invalidRequest(QStringLiteral("Unknown command"));
}

}
