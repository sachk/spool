#pragma once

#include <QString>

namespace JellyfinNative::CredentialStore {

QString load(const QString& profileId);
bool save(const QString& profileId, const QString& accessToken);
void remove(const QString& profileId);
void clear();

} // namespace JellyfinNative::CredentialStore
