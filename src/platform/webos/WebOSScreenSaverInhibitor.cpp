#include "platform/ScreenSaverInhibitor.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

extern "C" {
#include <luna-service2/lunaservice.h>
#include <webos-helpers/libhelpers.h>
}

namespace JellyfinNative {

struct ScreenSaverInhibitor::PlatformData {
    std::unique_ptr<HContext> subscription;

    static bool request(LSHandle *, LSMessage *message, void *)
    {
        const QJsonObject object
            = QJsonDocument::fromJson(QByteArray(message ? LSMessageGetPayload(message) : "")).object();
        if (object.value(QStringLiteral("state")).toString() != QStringLiteral("Active"))
            return true;
        const QJsonObject response { { QStringLiteral("clientName"), QStringLiteral("com.sachk.tern") },
            { QStringLiteral("ack"), false },
            { QStringLiteral("timestamp"), object.value(QStringLiteral("timestamp")) } };
        const QByteArray payload = QJsonDocument(response).toJson(QJsonDocument::Compact);
        HContext context {};
        context.pub = true;
        context.multiple = false;
        return HLunaServiceCall(
                   "luna://com.webos.service.tvpower/power/responseScreenSaverRequest", payload.constData(), &context)
            == 0;
    }
};

ScreenSaverInhibitor::ScreenSaverInhibitor()
    : m_platform(std::make_unique<PlatformData>())
{
}

ScreenSaverInhibitor::~ScreenSaverInhibitor()
{
    setInhibited(false);
}

void ScreenSaverInhibitor::setInhibited(bool inhibited)
{
    if (m_inhibited == inhibited)
        return;
    if (inhibited) {
        auto context = std::make_unique<HContext>();
        context->pub = true;
        context->multiple = true;
        context->callback = &PlatformData::request;
        context->userdata = m_platform.get();
        if (HLunaServiceCall("luna://com.webos.service.tvpower/power/registerScreenSaverRequest",
                "{\"subscribe\":true,\"clientName\":\"com.sachk.tern\"}", context.get())) {
            qWarning() << "screensaver: webOS inhibit registration failed";
            return;
        }
        m_platform->subscription = std::move(context);
    } else if (m_platform->subscription) {
        HUnregisterServiceCallback(m_platform->subscription.get());
        m_platform->subscription.reset();
    }
    m_inhibited = inhibited;
    qInfo() << "screensaver:" << (inhibited ? "inhibited for active playback" : "available while paused or idle");
}

bool ScreenSaverInhibitor::inhibited() const
{
    return m_inhibited;
}

} // namespace JellyfinNative
