#include "AndroidUpdateController.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJniObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QtCore/qnativeinterface.h>

namespace JellyfinNative {

namespace {

    constexpr auto kManifestUrl = "https://sachk.github.io/spool/updates/android.json";
    constexpr auto kJavaBridge = "com/sachk/spool/AndroidUpdateBridge";
    constexpr qsizetype kMaximumManifestBytes = 512 * 1024;

    QString apkPath(const QString& cacheRoot)
    {
        return QDir(cacheRoot).filePath(QStringLiteral("updates/spool-update.apk"));
    }

}

AndroidUpdateController::AndroidUpdateController(QNetworkAccessManager *network, QString cacheRoot, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_cacheRoot(std::move(cacheRoot))
{
    m_speedTimer.setInterval(500);
    connect(&m_speedTimer, &QTimer::timeout, this, [this] {
        const qint64 elapsedMs = m_speedClock.elapsed();
        const qint64 intervalMs = elapsedMs - m_lastSpeedMs;
        if (intervalMs <= 0)
            return;
        m_bytesPerSecond = (m_receivedBytes - m_lastSpeedBytes) * 1000 / intervalMs;
        m_lastSpeedBytes = m_receivedBytes;
        m_lastSpeedMs = elapsedMs;
        emit progressChanged();
    });
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive || !m_waitingForPermission)
            return;
        m_waitingForPermission = false;
        setStage(canRequestPackageInstalls() ? Stage::Ready : Stage::PermissionRequired);
    });
}

AndroidUpdateController::~AndroidUpdateController()
{
    resetDownload();
}

QString AndroidUpdateController::stage() const
{
    switch (m_stage) {
    case Stage::Idle:
        return QStringLiteral("idle");
    case Stage::Checking:
        return QStringLiteral("checking");
    case Stage::Available:
        return QStringLiteral("available");
    case Stage::Downloading:
        return QStringLiteral("downloading");
    case Stage::Ready:
        return QStringLiteral("ready");
    case Stage::PermissionRequired:
        return QStringLiteral("permission");
    case Stage::Error:
        return QStringLiteral("error");
    }
    return QStringLiteral("idle");
}

QString AndroidUpdateController::version() const
{
    return m_release.version;
}

QString AndroidUpdateController::notes() const
{
    return m_release.notes;
}

QUrl AndroidUpdateController::releaseUrl() const
{
    return m_release.releaseUrl;
}

qint64 AndroidUpdateController::receivedBytes() const
{
    return m_receivedBytes;
}

qint64 AndroidUpdateController::totalBytes() const
{
    return m_totalBytes;
}

qint64 AndroidUpdateController::bytesPerSecond() const
{
    return m_bytesPerSecond;
}

double AndroidUpdateController::progress() const
{
    return m_totalBytes > 0 ? qBound(0.0, static_cast<double>(m_receivedBytes) / m_totalBytes, 1.0) : 0.0;
}

QString AndroidUpdateController::errorText() const
{
    return m_errorText;
}

bool AndroidUpdateController::allowPrerelease() const
{
    return m_allowPrerelease;
}

void AndroidUpdateController::setAllowPrerelease(bool allow)
{
    if (m_allowPrerelease == allow)
        return;
    m_allowPrerelease = allow;
    emit allowPrereleaseChanged();
}

void AndroidUpdateController::start()
{
    if (m_stage == Stage::Idle)
        checkForUpdate();
}

void AndroidUpdateController::checkForUpdate()
{
    if (!m_network || m_reply)
        return;
    m_manifestBytes.clear();
    m_errorText.clear();
    setStage(Stage::Checking);

    QNetworkRequest request(QUrl(QString::fromLatin1(kManifestUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Spool-Android/%1").arg(JELLYFIN_VERSION));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (!m_reply)
            return;
        m_manifestBytes += m_reply->readAll();
        if (m_manifestBytes.size() > kMaximumManifestBytes)
            m_reply->abort();
    });
    connect(m_reply, &QNetworkReply::finished, this, &AndroidUpdateController::finishManifestRequest);
}

void AndroidUpdateController::finishManifestRequest()
{
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    m_manifestBytes += reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool validResponse
        = reply->error() == QNetworkReply::NoError && status == 200 && m_manifestBytes.size() <= kMaximumManifestBytes;
    if (!validResponse) {
        qWarning() << "update check failed:" << status << reply->errorString();
        reply->deleteLater();
        setStage(Stage::Idle);
        return;
    }
    reply->deleteLater();

    UpdateManifestResult result
        = selectAndroidUpdate(m_manifestBytes, SPOOL_ANDROID_VERSION_CODE, m_allowPrerelease, assetKey());
    m_manifestBytes.clear();
    if (!result.error.isEmpty()) {
        qWarning() << "update manifest rejected:" << result.error;
        setStage(Stage::Idle);
        return;
    }
    if (!result.release) {
        setStage(Stage::Idle);
        return;
    }
    m_release = std::move(*result.release);
    m_totalBytes = m_release.apkSize;
    emit progressChanged();
    setStage(Stage::Available);
}

void AndroidUpdateController::decline()
{
    const Stage previousStage = m_stage;
    if (m_stage == Stage::Downloading)
        cancelDownload();
    if (m_apkReady
        && (previousStage == Stage::Ready || previousStage == Stage::PermissionRequired
            || previousStage == Stage::Error)) {
        QFile::remove(apkPath(m_cacheRoot));
        m_apkReady = false;
    }
    m_errorText.clear();
    setStage(Stage::Idle);
}

void AndroidUpdateController::download()
{
    if (m_stage != Stage::Available && m_stage != Stage::Error)
        return;
    resetDownload();
    m_apkReady = false;
    const QString directory = QDir(m_cacheRoot).filePath(QStringLiteral("updates"));
    if (!QDir().mkpath(directory)) {
        fail(QStringLiteral("Could not create a safe location for the update."));
        return;
    }

    m_output = std::make_unique<QSaveFile>(apkPath(m_cacheRoot));
    if (!m_output->open(QIODevice::WriteOnly)) {
        fail(QStringLiteral("Could not create the update file."));
        return;
    }
    m_hash = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256);
    m_receivedBytes = 0;
    m_totalBytes = m_release.apkSize;
    m_bytesPerSecond = 0;
    m_lastSpeedBytes = 0;
    m_lastSpeedMs = 0;
    m_speedClock.start();
    m_speedTimer.start();
    emit progressChanged();
    setStage(Stage::Downloading);

    QNetworkRequest request(m_release.apkUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Spool-Android/%1").arg(JELLYFIN_VERSION));
    request.setRawHeader("Accept", "application/vnd.android.package-archive");
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &AndroidUpdateController::consumeDownloadData);
    connect(m_reply, &QNetworkReply::finished, this, &AndroidUpdateController::finishDownload);
}

void AndroidUpdateController::consumeDownloadData()
{
    if (!m_reply || !m_output || !m_hash)
        return;
    const QByteArray bytes = m_reply->readAll();
    if (bytes.isEmpty())
        return;
    if (m_receivedBytes + bytes.size() > m_release.apkSize || m_output->write(bytes) != bytes.size()) {
        m_reply->abort();
        return;
    }
    m_hash->addData(bytes);
    m_receivedBytes += bytes.size();
    emit progressChanged();
}

void AndroidUpdateController::finishDownload()
{
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    consumeDownloadData();
    m_reply = nullptr;
    m_speedTimer.stop();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool networkOk = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
    const QString networkError = reply->errorString();
    reply->deleteLater();

    const bool sizeOk = m_receivedBytes == m_release.apkSize;
    const bool hashOk = m_hash && m_hash->result().toHex() == m_release.apkSha256;
    if (!networkOk || !sizeOk || !hashOk || !m_output || !m_output->commit()) {
        resetDownload();
        if (!networkOk)
            fail(QStringLiteral("The update download failed: %1").arg(networkError));
        else if (!sizeOk)
            fail(QStringLiteral("The downloaded APK has the wrong size and was discarded."));
        else if (!hashOk)
            fail(QStringLiteral("The downloaded APK failed its SHA-256 check and was discarded."));
        else
            fail(QStringLiteral("The verified update could not be saved."));
        return;
    }

    m_output.reset();
    m_hash.reset();
    m_bytesPerSecond = 0;
    emit progressChanged();
    m_apkReady = true;
    setStage(Stage::Ready);
}

void AndroidUpdateController::cancelDownload()
{
    resetDownload();
    m_receivedBytes = 0;
    m_bytesPerSecond = 0;
    emit progressChanged();
    setStage(Stage::Available);
}

void AndroidUpdateController::install()
{
    if (m_stage != Stage::Ready && m_stage != Stage::PermissionRequired)
        return;
    if (!canRequestPackageInstalls()) {
        setStage(Stage::PermissionRequired);
        return;
    }
    if (!launchInstaller())
        fail(QStringLiteral("Android could not open the package installer."));
}

void AndroidUpdateController::openInstallSettings()
{
    if (m_stage != Stage::PermissionRequired)
        return;
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    const jboolean opened = QJniObject::callStaticMethod<jboolean>(
        kJavaBridge, "openInstallSettings", "(Landroid/content/Context;)Z", context.object<jobject>());
    if (!opened) {
        fail(QStringLiteral("Android could not open the ‘Install unknown apps’ setting."));
        return;
    }
    m_waitingForPermission = true;
}

void AndroidUpdateController::retry()
{
    if (m_apkReady) {
        setStage(Stage::Ready);
        return;
    }
    if (m_release.versionCode > 0)
        download();
    else
        checkForUpdate();
}

void AndroidUpdateController::setStage(Stage stage)
{
    if (m_stage == stage)
        return;
    m_stage = stage;
    emit changed();
}

void AndroidUpdateController::fail(QString message)
{
    m_errorText = std::move(message);
    setStage(Stage::Error);
}

void AndroidUpdateController::resetDownload()
{
    m_speedTimer.stop();
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_output)
        m_output->cancelWriting();
    m_output.reset();
    m_hash.reset();
}

bool AndroidUpdateController::canRequestPackageInstalls() const
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    return QJniObject::callStaticMethod<jboolean>(
        kJavaBridge, "canRequestPackageInstalls", "(Landroid/content/Context;)Z", context.object<jobject>());
}

bool AndroidUpdateController::launchInstaller() const
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    const QJniObject path = QJniObject::fromString(apkPath(m_cacheRoot));
    return QJniObject::callStaticMethod<jboolean>(kJavaBridge, "installApk",
        "(Landroid/content/Context;Ljava/lang/String;)Z", context.object<jobject>(), path.object<jstring>());
}

QString AndroidUpdateController::assetKey() const
{
#ifdef SPOOL_ANDROID_TV
    const QString variant = QStringLiteral("tv");
#else
    const QString variant = QStringLiteral("phone");
#endif
    return variant + QLatin1Char('-') + QStringLiteral(SPOOL_ANDROID_ABI);
}

} // namespace JellyfinNative
