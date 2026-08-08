#include "player/MpvVideoItem.h"

#include "TestMain.h"

#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTemporaryFile>
#include <QThread>

#include <clocale>
#include <cstdio>

extern "C" {
#include <mpv/client.h>
}

namespace {

bool writeVideo(QTemporaryFile& file)
{
    static const QByteArray encoded(
        "GkXfo6NChoEBQveBAULygQRC84EIQoKIbWF0cm9za2FCh4EEQoWBAhhTgGcBAAAAAAADLBFNm3TAv4RapygiTbuLU6uEFUmpZlOsgaFNu4tTq4"
        "QWVK5rU6yB8U27jFOrhBJUw2dTrIIBgk27jFOrhBxTu2tTrIIDEOwBAAAAAAAAUwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAFUmpZsu/"
        "hIOnwO0q17GDD0JATYCNTGF2ZjYyLjEyLjEwMldBjUxhdmY2Mi4xMi4xMDJzpJAfQKd/Xi4CYnAdzmOHtmnNRImIQJ9AAAAAAAAWVK5rQIu/"
        "hDLVR6yuAQAAAAAAAHzXgQFzxYi9z8m93l/"
        "54ZyBACK1nIN1bmSIgQCGhlZfRkZWMYOBASPjg4Q7msoA4JCwgSC6gSCagQJVsIRVuYEBVe6BAOwBAAAAAAAAAgAAY6KqViuE0ZwFL0E8YCbpX"
        "DdvXRt2l506ycQgQx6Ln1UgUS9O+KFoO5sXE3wDElTDZ0CAv4TarNEk"
        "c3OgY8CAZ8iaRaOHRU5DT0RFUkSHjUxhdmY2Mi4xMi4xMDJzc9RjwItjxYi9z8m93l/"
        "54WfIn0Wjh0VOQ09ERVJEh5JMYXZjNjIuMjguMTAyIGZmdjFnyKFFo4hEVVJBVElPTkSHkzAwOjAwOjAyLjAwMDAwMDAwMAAfQ7Z1QQK/"
        "hP+j8hfngQCjQISBAACA/BWAAASsr/+f///+AAp5Xz//7DK+f//AAAAYAOv9GZ493oAABKyv/5////4ACnlfP//sMr5//"
        "8AAABgAGdKzDpwckwAErK//n////gAKeV8//+wyvn//wAAAGAD8Ewr+EL89AASsr/+f///+AAp5Xz//7DK+f//"
        "AAAAYANEOkTKj8IED6AB8lYAXv//f///+Of+//+w/7//4AAATAE57dHg93oAXv//f///+Of+//+w/7//4AAATAEx7FF+cHJMXv//f///+Of+//"
        "+w/7//4AAATAG0PoF4Qvz0Xv//f///+Of+//+w/7//4AAATAHkfpJEcU7trl7+EzPmEuruPs4EAt4r3gQHxggII8IEJ");
    const QByteArray video = QByteArray::fromBase64(encoded);
    return file.open() && file.write(video) == video.size() && file.flush();
}

bool containsVideoPixel(const QImage& image)
{
    if (image.isNull())
        return false;
    for (int y = 0; y < image.height(); y += 8) {
        for (int x = 0; x < image.width(); x += 8) {
            const QColor color = image.pixelColor(x, y);
            if (color.red() > 80 && color.red() > color.green() * 2 && color.red() > color.blue() * 2)
                return true;
        }
    }
    return false;
}

} // namespace

JELLYFIN_TEST_MAIN("mpv-video-item")
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    QSurfaceFormat::setDefaultFormat(format);
    QGuiApplication app(argc, argv);

    QTemporaryFile video(QDir::tempPath() + QStringLiteral("/mpv-video-item-XXXXXX.mkv"));
    if (!writeVideo(video)) {
        std::fprintf(stderr, "failed to create test video\n");
        return 1;
    }

    QQuickWindow window;
    window.setColor(Qt::black);
    window.resize(320, 180);
    JellyfinNative::MpvVideoItem videoItem(window.contentItem());
    videoItem.setSize(QSizeF(window.size()));
    window.show();
    app.processEvents();

    std::setlocale(LC_NUMERIC, "C");
    mpv_handle *handle = mpv_create();
    if (!handle || mpv_set_option_string(handle, "terminal", "no") < 0
        || mpv_set_option_string(handle, "vo", "libmpv") < 0 || mpv_set_option_string(handle, "hwdec", "no") < 0
        || mpv_initialize(handle) < 0) {
        std::fprintf(stderr, "failed to initialize mpv\n");
        if (handle)
            mpv_terminate_destroy(handle);
        return 1;
    }

    videoItem.setMpvHandle(handle);
    if (!videoItem.waitForRenderContext()) {
        std::fprintf(stderr, "render context was not ready before media load\n");
        mpv_terminate_destroy(handle);
        return 1;
    }

    const QByteArray path = QFile::encodeName(video.fileName());
    const char *command[] = { "loadfile", path.constData(), nullptr };
    if (mpv_command(handle, command) < 0) {
        std::fprintf(stderr, "failed to load test video\n");
        videoItem.releaseMpvHandle();
        mpv_terminate_destroy(handle);
        return 1;
    }

    bool rendered = false;
    QElapsedTimer timer;
    timer.start();
    while (!rendered && timer.elapsed() < 5000) {
        app.processEvents(QEventLoop::AllEvents, 20);
        rendered = containsVideoPixel(window.grabWindow());
        QThread::msleep(10);
    }

    const bool released = videoItem.releaseMpvHandle();
    mpv_terminate_destroy(handle);
    if (!rendered || !released) {
        std::fprintf(stderr, "video result: rendered=%d released=%d\n", rendered, released);
        return 1;
    }
    return 0;
}
