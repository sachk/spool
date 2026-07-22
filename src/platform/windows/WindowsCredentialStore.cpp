#include "../CredentialStore.h"

#include <QByteArray>

#include <windows.h>

#include <wincred.h>

namespace JellyfinNative::CredentialStore {
namespace {

    QString target(const QString& profileId)
    {
        return QStringLiteral("JellyfinNative/%1").arg(profileId);
    }

} // namespace

QString load(const QString& profileId)
{
    PCREDENTIALW credential = nullptr;
    const QString name = target(profileId);
    if (!CredReadW(reinterpret_cast<LPCWSTR>(name.utf16()), CRED_TYPE_GENERIC, 0, &credential))
        return {};
    const QString token = QString::fromUtf8(reinterpret_cast<const char *>(credential->CredentialBlob),
        static_cast<qsizetype>(credential->CredentialBlobSize));
    CredFree(credential);
    return token;
}

bool save(const QString& profileId, const QString& accessToken)
{
    const QString name = target(profileId);
    const QByteArray token = accessToken.toUtf8();
    CREDENTIALW credential {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(name.utf16()));
    credential.CredentialBlobSize = static_cast<DWORD>(token.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(token.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<LPWSTR>(L"Jellyfin Native");
    return CredWriteW(&credential, 0) != FALSE;
}

void remove(const QString& profileId)
{
    const QString name = target(profileId);
    CredDeleteW(reinterpret_cast<LPCWSTR>(name.utf16()), CRED_TYPE_GENERIC, 0);
}

void clear()
{
    DWORD count = 0;
    PCREDENTIALW *credentials = nullptr;
    if (!CredEnumerateW(L"JellyfinNative/*", 0, &count, &credentials))
        return;
    for (DWORD index = 0; index < count; ++index)
        CredDeleteW(credentials[index]->TargetName, CRED_TYPE_GENERIC, 0);
    CredFree(credentials);
}

} // namespace JellyfinNative::CredentialStore
