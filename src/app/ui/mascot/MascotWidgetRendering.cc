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

#include "shijima-qt/ui/mascot/ShijimaWidget.hpp"

#include <QApplication>
#include <QBitmap>
#include <QDir>
#include <QPainter>
#include <QScreen>

#include <algorithm>
#include <cctype>
#include <cmath>

#include "Platform/Platform.hpp"
#include "shijima-qt/AppLog.hpp"
#include "shijima-qt/AssetLoader.hpp"
#include "shijima-qt/SafePath.hpp"

namespace {

constexpr double kMinRenderScale = 0.05;
constexpr double kMaxRenderScale = 20.0;
constexpr int kMaxMascotWindowExtent = 8192;

int boundedDimension(double value, int fallback = 1) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    if (value <= 0) {
        return fallback;
    }
    if (value > kMaxMascotWindowExtent) {
        return kMaxMascotWindowExtent;
    }
    return std::max(1, (int)value);
}

}

Asset const& ShijimaWidget::getActiveAsset() {
    auto &name = m_mascot->state->active_frame.get_name(m_mascot->state->looking_right);
    if (name.empty()) {
        APP_LOG_ERROR("mascot") << "Active frame name is empty for mascotId="
            << m_mascotId << " name=\"" << m_data->name().toStdString()
            << "\"; using transparent placeholder";
        return AssetLoader::defaultLoader()->loadAsset(QStringLiteral("@/img/__missing__.png"));
    }
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    auto imagePath = SafePath::safeChildPath(m_data->imgRoot(),
        QString::fromStdString(lowerName));
    if (!imagePath.has_value() || !imagePath->endsWith(
        QStringLiteral(".png"), Qt::CaseInsensitive))
    {
        APP_LOG_WARN("mascot") << "Rejected unsafe active frame name for mascotId="
            << m_mascotId;
        return AssetLoader::defaultLoader()->loadAsset(
            QStringLiteral("@/img/__missing__.png"));
    }
    return AssetLoader::defaultLoader()->loadAsset(imagePath.value());
}

bool ShijimaWidget::isMirroredRender() const {
    return m_mascot->state->active_frame.right_name.empty() &&
        m_mascot->state->looking_right;
}

void ShijimaWidget::paintEvent(QPaintEvent *event) {
    try {
        if (!m_visible) {
            return;
        }
        auto &asset = getActiveAsset();
        auto &image = asset.image(isMirroredRender());
        auto scaledSize = image.size() / m_drawScale;
        QPainter painter(this);
        if (m_drawScale >= 4.0) {
            // High shrink ratio: use area-averaging pre-scale for better quality
            auto preScaled = image.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            painter.drawImage(m_drawOrigin, preScaled);
        }
        else {
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.drawImage(QRect { m_drawOrigin, scaledSize }, image);
        }
#ifdef __linux__
        if (Platform::useWindowMasks()) {
            m_windowMask = QBitmap::fromPixmap(asset.mask(isMirroredRender())
                .scaled(scaledSize));
            m_windowMask.translate(m_drawOrigin);
            auto bounding = m_windowMask.boundingRect();
            bounding.setTop(0);
            bounding.setLeft(0);
            if (bounding.width() > 0 && bounding.height() > 0) {
                setMask(m_windowMask);
            }
            else {
                setMask(QRect { m_windowWidth - 2, m_windowHeight - 2, 1, 1 });
            }
        }
#endif
    }
    catch (std::exception &ex) {
        APP_LOG_ERROR("mascot") << "Paint failed for mascotId=" << m_mascotId
            << " name=\"" << m_data->name().toStdString() << "\": " << ex.what();
        markForDeletion();
        close();
    }
    catch (...) {
        APP_LOG_ERROR("mascot") << "Paint failed for mascotId=" << m_mascotId
            << " name=\"" << m_data->name().toStdString()
            << "\" with unknown exception";
        markForDeletion();
        close();
    }
}

bool ShijimaWidget::updateOffsets() {
    bool needsRepaint = false;
    auto &frame = m_mascot->state->active_frame;
    auto &asset = getActiveAsset();

    // Does the image go outside of the minimum boundary? If so,
    // extend the window boundary
    int originalWidth = asset.originalSize().width();
    int originalHeight = asset.originalSize().height();
    double scale = m_mascot->state->env->get_scale();
    if (!std::isfinite(scale) || scale <= 0) {
        APP_LOG_WARN("mascot") << "Invalid render scale for mascotId="
            << m_mascotId << " name=\"" << m_data->name().toStdString()
            << "\"; using fallback scale";
        scale = 1.0;
    }
    scale = std::clamp(scale, kMinRenderScale, kMaxRenderScale);
    int screenWidth = boundedDimension(
        m_mascot->state->env->screen.width() / scale,
        QApplication::primaryScreen() == nullptr
            ? kMaxMascotWindowExtent
            : QApplication::primaryScreen()->geometry().width());
    int screenHeight = boundedDimension(
        m_mascot->state->env->screen.height() / scale,
        QApplication::primaryScreen() == nullptr
            ? kMaxMascotWindowExtent
            : QApplication::primaryScreen()->geometry().height());
    int windowWidth = boundedDimension(originalWidth / scale);
    int windowHeight = boundedDimension(originalHeight / scale);

    if (windowWidth != m_windowWidth) {
        m_windowWidth = windowWidth;
        setFixedWidth(m_windowWidth);
        needsRepaint = true;
    }
    if (windowHeight != m_windowHeight) {
        m_windowHeight = windowHeight;
        setFixedHeight(m_windowHeight);
        needsRepaint = true;
    }

    // Determine the frame anchor within the window
    if (isMirroredRender()) {
        m_anchorInWindow = {
            (int)((originalWidth - frame.anchor.x) / scale),
            (int)(frame.anchor.y / scale) };
    }
    else {
        m_anchorInWindow = { (int)(frame.anchor.x / scale),
            (int)(frame.anchor.y / scale) };
    }

    // Detemine draw offsets and window positions
    QPoint drawOffset;
    m_visible = true;
    int winX = (int)m_mascot->state->anchor.x - m_anchorInWindow.x()
        - (int)env()->screen.left;
    int winY = (int)m_mascot->state->anchor.y - m_anchorInWindow.y()
        - (int)env()->screen.top;
    if (winX < 0) {
        drawOffset.setX(winX);
        winX = 0;
    }
    else if (winX + windowWidth > screenWidth) {
        drawOffset.setX(winX - screenWidth + windowWidth);
        winX = screenWidth - windowWidth;
    }
    if (winY < 0) {
        drawOffset.setY(winY);
        winY = 0;
    }
    else if (winY + windowHeight > screenHeight) {
        drawOffset.setY(winY - screenHeight + windowHeight);
        winY = screenHeight - windowHeight;
    }
    winX += (int)env()->screen.left;
    winY += (int)env()->screen.top;

    if (isMirroredRender()) {
        drawOffset += QPoint {
            (int)((originalWidth - asset.offset().topRight().x()) / scale),
            (int)(asset.offset().topLeft().y() / scale) };
    }
    else {
        drawOffset += asset.offset().topLeft() / scale;
    }
    if (drawOffset != m_drawOrigin) {
        needsRepaint = true;
        m_drawOrigin = drawOffset;
    }
    if (scale != m_drawScale) {
        needsRepaint = true;
        m_drawScale = scale;
    }
    move(winX, winY);

    return needsRepaint;
}

bool ShijimaWidget::pointInside(QPoint const& point) {
    if (!m_visible) {
        return false;
    }
    auto &asset = getActiveAsset();
    auto image = asset.image(isMirroredRender());
    int drawnWidth = (int)(image.width() / m_drawScale);
    int drawnHeight = (int)(image.height() / m_drawScale);
    auto imagePos = point - m_drawOrigin;
    if (imagePos.x() < 0 || imagePos.y() < 0 ||
        imagePos.x() > drawnWidth || imagePos.y() > drawnHeight)
    {
        return false;
    }
    //FIXME: is this position correct?
    auto color = image.pixelColor(imagePos * m_drawScale);
    if (color.alpha() == 0) {
        return false;
    }
    return true;
}
