#include "shijima-qt/SafePath.hpp"

#include <QDir>
#include <QFileInfo>

namespace SafePath {

namespace {

bool isContained(QString const& root, QString const& path) {
    QString relative = QDir(root).relativeFilePath(path);
    return relative != QStringLiteral("..") &&
        !relative.startsWith(QStringLiteral("../")) &&
        !QFileInfo(relative).isAbsolute();
}

}

std::optional<QString> safeChildPath(QString const& root, QString const& name) {
    if (root.isEmpty() || name.isEmpty() || QFileInfo(name).isAbsolute()) {
        return std::nullopt;
    }

    QString normalizedName = name;
    normalizedName.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (normalizedName.startsWith(QLatin1Char('/')) ||
        normalizedName.contains(QLatin1Char(':')))
    {
        return std::nullopt;
    }

    auto parts = normalizedName.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    for (auto const& part : parts) {
        if (part.isEmpty() || part == QStringLiteral(".") ||
            part == QStringLiteral(".."))
        {
            return std::nullopt;
        }
    }

    if (root.startsWith(QLatin1Char('@'))) {
        return QDir::cleanPath(root + QLatin1Char('/') + normalizedName);
    }

    QFileInfo rootInfo(root);
    QString absoluteRoot = QDir::cleanPath(rootInfo.absoluteFilePath());
    QString canonicalRoot = rootInfo.canonicalFilePath();
    if (canonicalRoot.isEmpty()) {
        canonicalRoot = absoluteRoot;
    }
    QString candidate = QDir::cleanPath(QDir(absoluteRoot).absoluteFilePath(normalizedName));
    if (!isContained(absoluteRoot, candidate)) {
        return std::nullopt;
    }

    QFileInfo existing(candidate);
    while (!existing.exists() && existing.absoluteFilePath() != absoluteRoot) {
        existing.setFile(existing.absolutePath());
    }
    QString canonicalExisting = existing.canonicalFilePath();
    if (!canonicalExisting.isEmpty() &&
        !isContained(canonicalRoot, canonicalExisting) &&
        canonicalExisting != canonicalRoot)
    {
        return std::nullopt;
    }
    return candidate;
}

}
