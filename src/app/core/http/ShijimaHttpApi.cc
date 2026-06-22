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

#include "shijima-qt/ShijimaHttpApi.hpp"
#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/SecurityLimits.hpp"
#include <httplib.h>
#include "shijima-qt/ShijimaManager.hpp"
#include <sstream>
#include <thread>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <cctype>
#include <chrono>

using namespace httplib;

namespace {

void setLastError(std::mutex &mutex, std::string &target, std::string value) {
    std::lock_guard<std::mutex> lock(mutex);
    target = std::move(value);
}

std::string getLastError(std::mutex &mutex, std::string const& source) {
    std::lock_guard<std::mutex> lock(mutex);
    return source;
}

bool requestHasJsonContentType(Request const& req) {
    auto contentType = req.get_header_value("content-type");
    std::transform(contentType.begin(), contentType.end(), contentType.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    auto separator = contentType.find(';');
    if (separator != std::string::npos) {
        contentType.resize(separator);
    }
    while (!contentType.empty() &&
        std::isspace(static_cast<unsigned char>(contentType.back())))
    {
        contentType.pop_back();
    }
    return contentType == "application/json";
}

std::optional<QJsonObject> jsonForRequest(Request const& req) {
    if (!requestHasJsonContentType(req)) {
        return {};
    }
    if (req.body.size() > SecurityLimits::kHttpJsonBodyMaxBytes) {
        APP_LOG_WARN("http") << "Rejected oversized JSON body for " << req.method
            << " " << req.path << " bytes=" << req.body.size()
            << " limit=" << SecurityLimits::kHttpJsonBodyMaxBytes;
        return {};
    }
    QByteArray bytes { req.body.c_str(), (qsizetype)req.body.size() };
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError) {
        APP_LOG_WARN("http") << "Rejected JSON body for " << req.method << " " << req.path
            << ": " << error.errorString().toStdString();
        return {};
    }
    else if (!doc.isObject()) {
        // request bodies must contain objects
        return {};
    }
    else {
        return doc.object();
    }
}

RegisterCliLabelRequest cliLabelRequestFromJson(QJsonObject object) {
    RegisterCliLabelRequest request;
    auto mascotIdValue = object.take("mascot_id");
    if (mascotIdValue.isDouble()) {
        request.mascotId = mascotIdValue.toInt();
    }
    auto labelValue = object.take("label");
    if (labelValue.isDouble()) {
        request.label = labelValue.toInt();
    }
    return request;
}

void sendJson(Response &res, QJsonObject const& object) {
    QJsonDocument doc { object };
    auto bytes = doc.toJson(QJsonDocument::Compact);
    res.set_content(&bytes[0], bytes.size(), "application/json");
}

std::string requestTarget(Request const& req);

void badRequest(Request const& req, Response &res) {
    QJsonObject obj;
    obj["error"] = "400 Bad Request";
    obj["code"] = "bad_request";
    res.status = 400;
    APP_LOG_WARN("http") << "Rejected bad request " << req.method << " "
        << requestTarget(req);
    sendJson(res, obj);
}

std::string requestTarget(Request const& req) {
    std::ostringstream target;
    target << req.path;
    for (auto it = req.params.begin(); it != req.params.end(); ++it) {
        target << (it == req.params.begin() ? "?" : "&");
        target << it->first << "=" << it->second;
    }
    return target.str();
}

void sendError(Response &res, MascotCommandStatus const& status) {
    res.status = status.status;
    if (status.status >= 500) {
        APP_LOG_ERROR("http") << "Command failed status=" << status.status
            << " code=\"" << status.code.toStdString() << "\"";
    }
    else {
        APP_LOG_WARN("http") << "Command rejected status=" << status.status
            << " code=\"" << status.code.toStdString() << "\"";
    }
    sendJson(res, errorToJson(status));
}

bool parseSpawnRequest(QJsonObject const& object, SpawnMascotRequest &request) {
    QString parseError;
    bool ok = spawnMascotRequestFromJson(QJsonValue(object), request, &parseError);
    if (!ok) {
        APP_LOG_WARN("http") << "Invalid spawn request: "
            << parseError.toStdString();
    }
    return ok;
}

bool parsePatch(QJsonObject const& object, MascotPatch &patch) {
    QString parseError;
    bool ok = mascotPatchFromJson(QJsonValue(object), patch, &parseError);
    if (!ok) {
        APP_LOG_WARN("http") << "Invalid mascot patch: "
            << parseError.toStdString();
    }
    return ok;
}

}

ShijimaHttpApi::ShijimaHttpApi(ShijimaManager *manager): m_server(new Server),
    m_thread(nullptr), m_manager(manager), m_service(manager), m_host(""),
    m_port(-1)
{
    m_server->Get("/shijima/api/v1/mascots", [this](Request const& req, Response &res) {
        ListMascotsRequest request;
        if (req.has_param("selector")) {
            request.selector = QString::fromStdString(req.get_param_value("selector"));
        }
        QList<MascotInfo> mascots;
        auto status = m_service.listMascots(request, mascots);
        if (!status.ok()) {
            sendError(res, status);
            return;
        }
        QJsonArray array;
        for (auto const& mascot : mascots) {
            array.append(mascotInfoToJson(mascot));
        }
        QJsonObject object;
        object["mascots"] = array;
        sendJson(res, object);
    });
    m_server->Post("/shijima/api/v1/mascots",
        [this](Request const& req, Response &res)
    {
        auto json = jsonForRequest(req);
        if (!json.has_value()) {
            badRequest(req, res);
            return;
        }
        SpawnMascotRequest request;
        if (!parseSpawnRequest(*json, request)) {
            badRequest(req, res);
            return;
        }
        if (request.name.has_value() && request.dataId.has_value()) {
            badRequest(req, res);
            return;
        }
        MascotInfo mascot;
        auto status = m_service.spawnMascot(request, mascot);
        if (!status.ok()) {
            sendError(res, status);
            return;
        }
        QJsonObject object;
        object["mascot"] = mascotInfoToJson(mascot);
        sendJson(res, object);
    });
    m_server->Put("/shijima/api/v1/mascots/([0-9]+)",
        [this](Request const& req, Response &res)
    {
        auto json = jsonForRequest(req);
        if (!json.has_value()) {
            badRequest(req, res);
            return;
        }
        auto id = std::stoi(req.matches[1].str());
        MascotPatch patch;
        if (!parsePatch(*json, patch)) {
            badRequest(req, res);
            return;
        }
        MascotInfo mascot;
        auto status = m_service.alterMascot(id, patch, mascot);
        if (!status.ok()) {
            QJsonObject object = errorToJson(status);
            object["mascot"] = QJsonValue::Null;
            res.status = status.status;
            sendJson(res, object);
            return;
        }
        QJsonObject object;
        object["mascot"] = mascotInfoToJson(mascot);
        sendJson(res, object);
    });
    m_server->Get("/shijima/api/v1/mascots/([0-9]+)",
        [this](Request const& req, Response &res)
    {
        auto id = std::stoi(req.matches[1].str());
        MascotInfo mascot;
        auto status = m_service.getMascot(id, mascot);
        if (!status.ok()) {
            QJsonObject object = errorToJson(status);
            object["mascot"] = QJsonValue::Null;
            res.status = status.status;
            sendJson(res, object);
            return;
        }
        QJsonObject object;
        object["mascot"] = mascotInfoToJson(mascot);
        sendJson(res, object);
    });
    m_server->Delete("/shijima/api/v1/mascots/([0-9]+)",
        [this](Request const& req, Response &res)
    {
        auto id = std::stoi(req.matches[1].str());
        auto status = m_service.dismissMascot(id);
        if (!status.ok()) {
            sendError(res, status);
            return;
        }
        sendJson(res, {});
    });
    m_server->Delete("/shijima/api/v1/mascots",
        [this](Request const& req, Response &res)
    {
        auto json = jsonForRequest(req);
        DismissAllMascotsRequest request;
        if (json.has_value() && json->contains("selector")) {
            auto value = json->take("selector");
            if (value.isString()) {
                request.selector = value.toString();
            }
        }
        auto status = m_service.dismissAllMascots(request);
        if (!status.ok()) {
            sendError(res, status);
            return;
        }
        sendJson(res, {});
    });
    m_server->Get("/shijima/api/v1/loadedMascots",
        [this](Request const&, Response &res)
    {
        QList<LoadedMascotInfo> mascots;
        auto status = m_service.listLoadedMascots(mascots);
        if (!status.ok()) {
            sendError(res, status);
            return;
        }
        QJsonArray array;
        for (auto const& mascot : mascots) {
            array.append(loadedMascotInfoToJson(mascot));
        }
        QJsonObject object;
        object["loaded_mascots"] = array;
        sendJson(res, object);
    });
    m_server->Get("/shijima/api/v1/ping",
        [this](Request const&, Response &res)
    {
        sendJson(res, pingInfoToJson(m_service.ping()));
    });
    m_server->Post("/shijima/api/v1/cli/labels",
        [this](Request const& req, Response &res)
    {
        auto json = jsonForRequest(req);
        if (!json.has_value()) {
            badRequest(req, res);
            return;
        }
        auto request = cliLabelRequestFromJson(*json);
        if (request.mascotId < 0) {
            badRequest(req, res);
            return;
        }
        CliLabelInfo labelInfo;
        auto status = m_service.registerCliLabel(request, labelInfo);
        if (!status.ok()) {
            sendError(res, status);
            return;
        }
        QJsonObject object;
        object["label"] = labelInfo.label;
        object["mascot_id"] = labelInfo.mascotId;
        sendJson(res, object);
    });
    m_server->Get("/shijima/api/v1/cli/labels/([0-9]+)",
        [this](Request const& req, Response &res)
    {
        auto label = std::stoi(req.matches[1].str());
        CliLabelInfo labelInfo;
        auto status = m_service.getCliLabel(label, labelInfo);
        if (!status.ok()) {
            sendError(res, status);
            return;
        }
        QJsonObject object;
        object["label"] = labelInfo.label;
        object["mascot_id"] = labelInfo.mascotId;
        sendJson(res, object);
    });
    m_server->Get("/shijima/api/v1/loadedMascots/([0-9]+)",
        [this](Request const& req, Response &res)
    {
        auto id = std::stoi(req.matches[1].str());
        LoadedMascotInfo mascot;
        auto status = m_service.getLoadedMascot(id, mascot);
        if (!status.ok()) {
            QJsonObject object = errorToJson(status);
            object["loaded_mascot"] = QJsonValue::Null;
            res.status = status.status;
            sendJson(res, object);
            return;
        }
        QJsonObject object;
        object["loaded_mascot"] = loadedMascotInfoToJson(mascot);
        sendJson(res, object);
    });
    m_server->Get("/shijima/api/v1/loadedMascots/([0-9]+)/preview.png",
        [this](Request const& req, Response &res)
    {
        auto id = std::stoi(req.matches[1].str());
        QByteArray bytes;
        auto status = m_service.getLoadedMascotPreviewPng(id, bytes);
        if (!status.ok()) {
            sendError(res, status);
            return;
        }
        res.set_content(bytes.constData(), bytes.size(), "image/png");
    });
    m_server->Get(".*", badRequest);
    m_server->Put(".*", badRequest);
    m_server->Post(".*", badRequest);
    m_server->Delete(".*", badRequest);
    m_server->Patch(".*", badRequest);
    m_server->set_payload_max_length(SecurityLimits::kHttpJsonBodyMaxBytes);
    m_server->set_logger([](const Request &req, const Response &res) {
        APP_LOG_INFO("http") << req.method << " " << requestTarget(req)
            << " -> status=" << res.status;
    });
}

bool ShijimaHttpApi::start(std::string const& host, int port) {
    stop();
    m_host = host;
    m_port = port;
    setLastError(m_lastErrorMutex, m_lastError, {});
    m_startAttemptFinished.store(false);
    m_startSucceeded.store(false);
    APP_LOG_INFO("http") << "Starting local HTTP API on " << host << ":" << port;
    m_thread = new std::thread { [this, host, port](){
        APP_LOG_INFO("http") << "HTTP API listen loop entered on " << host << ":" << port;
        bool ok = m_server->listen(host, port);
        m_startSucceeded.store(ok);
        m_startAttemptFinished.store(true);
        if (!ok) {
            auto error = "HTTP API bind failed on " + host + ":"
                + std::to_string(port) + " (port may be in use)";
            setLastError(m_lastErrorMutex, m_lastError, error);
            APP_LOG_ERROR("http") << error;
        }
        APP_LOG_INFO("http") << "HTTP API listen loop exited";
    } };
    using namespace std::chrono_literals;
    auto deadline = std::chrono::steady_clock::now() + 1500ms;
    while (!m_startAttemptFinished.load() &&
        std::chrono::steady_clock::now() < deadline)
    {
        if (m_server->is_running()) {
            m_startSucceeded.store(true);
            m_startAttemptFinished.store(true);
            break;
        }
        std::this_thread::sleep_for(10ms);
    }
    if (!m_startAttemptFinished.load() && m_server->is_running()) {
        m_startSucceeded.store(true);
        m_startAttemptFinished.store(true);
    }
    if (!m_startSucceeded.load()) {
        stop();
        auto error = getLastError(m_lastErrorMutex, m_lastError);
        if (error.empty()) {
            error = "HTTP API bind failed on " + host + ":"
                + std::to_string(port);
            setLastError(m_lastErrorMutex, m_lastError, error);
        }
        APP_LOG_ERROR("http") << error;
        return false;
    }
    APP_LOG_INFO("http") << "Local HTTP API listening on " << host << ":" << port;
    return true;
}

bool ShijimaHttpApi::running() {
    return m_server->is_running();
}

int ShijimaHttpApi::port() {
    return m_port;
}

std::string const& ShijimaHttpApi::host() {
    return m_host;
}

std::string ShijimaHttpApi::lastError() const {
    return getLastError(m_lastErrorMutex, m_lastError);
}

void ShijimaHttpApi::stop() {
    if (m_server->is_running()) {
        APP_LOG_INFO("http") << "Stopping local HTTP API on " << m_host << ":" << m_port;
        m_server->stop();
    }
    if (m_thread != nullptr) {
        m_thread->join();
        delete m_thread;
        m_thread = nullptr;
    }
}

ShijimaHttpApi::~ShijimaHttpApi() {
    stop();
    delete m_server;
}
