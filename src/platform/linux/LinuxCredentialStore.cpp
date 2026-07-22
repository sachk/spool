#include "../CredentialStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

namespace JellyfinNative::CredentialStore {
namespace {

    QString testPath(const QString& profileId)
    {
        const QString root = qEnvironmentVariable("JELLYFIN_CREDENTIAL_STORE_DIR");
        if (root.isEmpty())
            return {};
        const QString name
            = QString::fromLatin1(QCryptographicHash::hash(profileId.toUtf8(), QCryptographicHash::Sha256).toHex());
        return QDir(root).filePath(name);
    }

    QString loadTestCredential(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(file.readAll());
    }

    bool saveTestCredential(const QString& path, const QString& token)
    {
        QDir directory = QFileInfo(path).dir();
        if (!directory.mkpath(QStringLiteral(".")))
            return false;
        QFile::setPermissions(
            directory.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        return file.write(token.toUtf8()) >= 0;
    }

    struct SecretResult {
        bool success = false;
        QByteArray output;
    };

    SecretResult runSecretTool(const QStringList& arguments, const QByteArray& input = {})
    {
        QProcess process;
        process.setProgram(QStringLiteral("secret-tool"));
        process.setArguments(arguments);
        process.start();
        if (!process.waitForStarted(3000))
            return {};
        if (!input.isEmpty()) {
            process.write(input);
            process.closeWriteChannel();
        }
        if (!process.waitForFinished(10000))
            return {};
        return { process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
            process.readAllStandardOutput() };
    }

    QStringList attributes(const QString& profileId)
    {
        return { QStringLiteral("application"), QStringLiteral("jellyfin-native"), QStringLiteral("profile"),
            profileId };
    }

} // namespace

QString load(const QString& profileId)
{
    if (profileId.isEmpty())
        return {};
    const QString path = testPath(profileId);
    if (!path.isEmpty())
        return loadTestCredential(path);
    const SecretResult result = runSecretTool(QStringList { QStringLiteral("lookup") } + attributes(profileId));
    return result.success ? QString::fromUtf8(result.output).trimmed() : QString();
}

bool save(const QString& profileId, const QString& accessToken)
{
    if (profileId.isEmpty() || accessToken.isEmpty())
        return false;
    const QString path = testPath(profileId);
    if (!path.isEmpty())
        return saveTestCredential(path, accessToken);
    return runSecretTool(
        QStringList { QStringLiteral("store"), QStringLiteral("--label=Jellyfin Native") } + attributes(profileId),
        accessToken.toUtf8())
        .success;
}

void remove(const QString& profileId)
{
    if (profileId.isEmpty())
        return;
    const QString path = testPath(profileId);
    if (!path.isEmpty()) {
        QFile::remove(path);
        return;
    }
    runSecretTool(QStringList { QStringLiteral("clear") } + attributes(profileId));
}

void clear()
{
    const QString root = qEnvironmentVariable("JELLYFIN_CREDENTIAL_STORE_DIR");
    if (!root.isEmpty()) {
        QDir(root).removeRecursively();
        return;
    }
    runSecretTool({ QStringLiteral("clear"), QStringLiteral("application"), QStringLiteral("jellyfin-native") });
}

} // namespace JellyfinNative::CredentialStore
