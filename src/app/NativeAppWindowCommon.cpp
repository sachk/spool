#include "NativeAppWindow.h"

#include <QMutexLocker>
#include <QQuickImageProvider>

namespace JellyfinNative {

namespace {

class OverlayImageProvider final : public QQuickImageProvider {
public:
  explicit OverlayImageProvider(const NativeAppWindow *window)
      : QQuickImageProvider(QQuickImageProvider::Image), m_window(window) {}

  QImage requestImage(const QString &, QSize *size,
                      const QSize &requestedSize) override {
    QImage image = m_window->copyOverlayImage();
    if (image.isNull()) {
      image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
    }
    if (requestedSize.isValid())
      image = image.scaled(requestedSize, Qt::IgnoreAspectRatio,
                           Qt::FastTransformation);
    if (size)
      *size = image.size();
    return image;
  }

private:
  const NativeAppWindow *m_window = nullptr;
};

} // namespace

int NativeAppWindow::overlayRevision() const { return m_overlayRevision; }

void NativeAppWindow::clearOverlay() {
  bool changed = false;
  {
    QMutexLocker locker(&m_overlayMutex);
    m_pendingOverlayImage = QImage();
    if (m_overlayImage.isNull())
      return;
    m_overlayImage = QImage();
    ++m_overlayRevision;
    changed = true;
  }
  if (changed)
    emit overlayRevisionChanged();
}

QQuickImageProvider *NativeAppWindow::createOverlayImageProvider() {
  return new OverlayImageProvider(this);
}

QImage NativeAppWindow::copyOverlayImage() const {
  QMutexLocker locker(&m_overlayMutex);
  return m_overlayImage;
}

} // namespace JellyfinNative
