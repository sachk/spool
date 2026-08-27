#include "platform/CredentialStore.h"
#include "platform/PlatformCapabilities.h"
#include "platform/PlatformPaths.h"
#include "platform/PlatformSettingsPolicy.h"
#include "platform/PlatformStartup.h"
#include "platform/PlatformSystemProbes.h"
#include "platform/ScreenSaverInhibitor.h"

#include "app/SettingsSchema.h"
#include "platform/NativeAppWindow.h"
#include "platform/common/CredentialStoreFileBackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJniObject>
#include <QStandardPaths>

#include <algorithm>

namespace JellyfinNative {
namespace {
    constexpr SettingChoice kAndroidAudioChoices[] = { { "auto", "Automatic" } };

    class AndroidScreenSaverBackend final : public ScreenSaverBackend {
    public:
        bool acquire() override
        {
            return setKeepScreenOn(true);
        }
        bool release() override
        {
            return setKeepScreenOn(false);
        }

    private:
        static bool setKeepScreenOn(bool enabled)
        {
            bool applied = false;
            auto task = QNativeInterface::QAndroidApplication::runOnAndroidMainThread([enabled, &applied] {
                const QJniObject activity = QNativeInterface::QAndroidApplication::context();
                const QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
                if (!window.isValid())
                    return;
                constexpr jint keepScreenOn = 0x00000080;
                if (enabled)
                    window.callMethod<void>("addFlags", "(I)V", keepScreenOn);
                else
                    window.callMethod<void>("clearFlags", "(I)V", keepScreenOn);
                applied = true;
            });
            task.waitForFinished();
            return applied;
        }
    };
}

const PlatformCapabilities& platformCapabilities()
{
#ifdef SPOOL_ANDROID_TV
    static const PlatformCapabilities capabilities {
        .deviceName = QStringLiteral("Android TV"),
        .rendererName = QStringLiteral("libmpv OpenGL ES"),
        .isTV = true,
        .hasDesktopPointer = false,
    };
#else
    static const PlatformCapabilities capabilities {
        .deviceName = QStringLiteral("Android"),
        .rendererName = QStringLiteral("libmpv OpenGL ES"),
        .isTV = false,
        .hasDesktopPointer = false,
    };
#endif
    return capabilities;
}

QString resolveAppRoot(const char *)
{
    return QStringLiteral(":");
}

QString bundledFontsPath(const QString&)
{
    const QString root
        = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("fonts"));
    QDir().mkpath(root);
    static constexpr const char *fontFiles[] = {
        "AtkinsonHyperlegible-Bold.otf",
        "AtkinsonHyperlegible-Regular.otf",
        "IBMPlexSans-Variable.ttf",
        "PTRootUI-Variable.ttf",
        "MaterialIcons-Regular.ttf",
    };
    for (const char *fontFile : fontFiles) {
        const QString destination = QDir(root).filePath(QString::fromLatin1(fontFile));
        if (QFile::exists(destination))
            continue;
        QFile::copy(QStringLiteral(":/fonts/%1").arg(QString::fromLatin1(fontFile)), destination);
    }
    return root;
}

QString startupCacheRoot(const QString&)
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

QString persistentDataRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QStringList appLogDirectories(const QString&)
{
    return {
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("logs"))
    };
}

QString appLogFileName()
{
    return QStringLiteral("jellyfin-native.log");
}

const PlatformAudioOutputPolicy& platformAudioOutputPolicy()
{
    static const PlatformAudioOutputPolicy policy { kAndroidAudioChoices, 1, "auto" };
    return policy;
}

QString normalizedPlatformAudioOutputMode(const QString&)
{
    return QStringLiteral("auto");
}

QStringList platformSystemSubtitleFonts()
{
    QStringList families = QFontDatabase::families();
    families.sort(Qt::CaseInsensitive);
    families.removeDuplicates();
    return families;
}

int platformDefaultUiScalePercent()
{
#ifdef SPOOL_ANDROID_TV
    return 150;
#else
    return 100;
#endif
}

bool platformUsesPerOutputAudioDelay()
{
    return false;
}
QString normalizedPlatformAudioRoute(const QString& output)
{
    return output;
}
QString platformAudioRouteDisplayName(const QString&)
{
    return QStringLiteral("Global");
}
QString platformAudioDelayStorageKey(const QString&)
{
    return QStringLiteral("settings/audioDelayMs");
}
int platformAutomaticAudioDelayMs(const QString&, int, int)
{
    return 0;
}

PlatformCpuProbe platformCpuProbe(int logicalCpus)
{
    return { std::max(1, logicalCpus), QStringLiteral("hardware_concurrency") };
}

PlatformMemoryPolicy platformMemoryPolicy()
{
    constexpr qint64 mib = 1024LL * 1024LL;
    return { 0, 2048LL * mib, 128LL * mib, 128LL * mib, 32, 24LL * mib, 96LL * mib };
}

qint64 effectiveLinuxMemoryBytes(const QByteArray&, const QByteArray&, const QByteArray&)
{
    return 0;
}

QString platformProcessMemoryDiagnostics()
{
    return {};
}

std::unique_ptr<ScreenSaverBackend> createPlatformScreenSaverBackend()
{
    return std::make_unique<AndroidScreenSaverBackend>();
}

bool configurePlatformEnvironment(const QString&)
{
    qputenv("QT_QUICK_CONTROLS_STYLE", QByteArrayLiteral("Basic"));
    return true;
}

QSurfaceFormat platformSurfaceFormat()
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(3, 0);
    format.setProfile(QSurfaceFormat::NoProfile);
    return format;
}

void configurePlatformWindow(NativeAppWindow& window)
{
    window.setFlags(Qt::Window | Qt::FramelessWindowHint);
}

namespace CredentialStore {
    namespace {
        void ensureCredentialRoot()
        {
            if (qEnvironmentVariableIsEmpty("JELLYFIN_CREDENTIAL_STORE_DIR")) {
                const QString root = QDir(persistentDataRoot()).filePath(QStringLiteral("credentials"));
                qputenv("JELLYFIN_CREDENTIAL_STORE_DIR", root.toUtf8());
            }
        }
    }

    QString load(const QString& profileId)
    {
        ensureCredentialRoot();
        return FileBackend::load(profileId);
    }
    bool save(const QString& profileId, const QString& accessToken)
    {
        ensureCredentialRoot();
        return FileBackend::save(profileId, accessToken);
    }
    void remove(const QString& profileId)
    {
        ensureCredentialRoot();
        FileBackend::remove(profileId);
    }
    void clear()
    {
        ensureCredentialRoot();
        FileBackend::clear();
    }
} // namespace CredentialStore

} // namespace JellyfinNative
