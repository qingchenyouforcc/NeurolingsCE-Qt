#pragma once

#include <QString>

#include <optional>

namespace SafePath {

// Resolves an untrusted relative name below root, or rejects it if it escapes.
std::optional<QString> safeChildPath(QString const& root, QString const& name);

}
