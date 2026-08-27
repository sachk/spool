#include "platform/PlatformApplicationServices.h"

#include "app/AppController.h"
#include "player/PlayerController.h"

#include <QGuiApplication>

namespace JellyfinNative {

struct PlatformApplicationServices::PlatformData {
    explicit PlatformData(AppController& controller)
        : player(controller.player())
    {
    }

    PlayerController *player = nullptr;
};

PlatformApplicationServices::PlatformApplicationServices(
    QGuiApplication&, NativeAppWindow&, AppController& controller, RouterController&)
    : m_platform(std::make_unique<PlatformData>(controller))
{
}

PlatformApplicationServices::~PlatformApplicationServices() = default;

void PlatformApplicationServices::start()
{
    QObject::connect(qGuiApp, &QGuiApplication::applicationStateChanged, qGuiApp,
        [platform = m_platform.get()](Qt::ApplicationState state) {
            if (!platform->player)
                return;
            if (state == Qt::ApplicationActive) {
                platform->player->resyncForForeground();
            } else if (state == Qt::ApplicationSuspended || state == Qt::ApplicationHidden) {
                if (platform->player->sessionActive() && !platform->player->paused())
                    platform->player->setPaused(true);
            }
        });
}

} // namespace JellyfinNative
