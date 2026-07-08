#pragma once

#include <QString>

namespace JellyfinNative {

class NavigationState final {
public:
    QString page() const;
    void reset();
    bool setPage(const QString& page);

private:
    QString m_page = QStringLiteral("login");
};

} // namespace JellyfinNative
