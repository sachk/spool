#include "app/SettingsSchema.h"

#include <QDebug>
#include <QHash>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

#include <cstdlib>

using namespace JellyfinNative;

namespace {

QString keyString(const SettingSpec &spec)
{
    return QString::fromLatin1(spec.key);
}

void require(bool condition, const QString &message)
{
    if (condition)
        return;
    qCritical().noquote() << message;
    std::exit(EXIT_FAILURE);
}

const SettingSpec &requiredSpec(const QString &key)
{
    const SettingSpec *spec = findSettingSpec(key);
    require(spec != nullptr, QStringLiteral("missing setting spec for %1").arg(key));
    return *spec;
}

QSet<QString> stringSet(const QStringList &values)
{
    QSet<QString> result;
    result.reserve(values.size());
    for (const QString &value : values)
        result.insert(value);
    return result;
}

QStringList choiceValues(const SettingSpec &spec)
{
    QStringList values;
    values.reserve(spec.choiceCount);
    for (qsizetype i = 0; i < spec.choiceCount; ++i)
        values.push_back(QString::fromLatin1(spec.choices[i].value));
    return values;
}

QString choiceLabel(const SettingSpec &spec, const QString &value)
{
    for (qsizetype i = 0; i < spec.choiceCount; ++i) {
        if (value == QString::fromLatin1(spec.choices[i].value))
            return QString::fromLatin1(spec.choices[i].label);
    }
    return {};
}

QVariantMap schemaRow(const QString &key)
{
    for (const QVariant &item : settingSchemaModel()) {
        const QVariantMap row = item.toMap();
        if (row.value(QStringLiteral("key")).toString() == key)
            return row;
    }
    return {};
}

QHash<QString, QString> choicesByLabelFromRow(const QVariantMap &row)
{
    const QVariantList values = row.value(QStringLiteral("choiceValues")).toList();
    const QVariantList labels = row.value(QStringLiteral("choiceLabels")).toList();
    require(values.size() == labels.size(),
            QStringLiteral("schema row choice values and labels diverged"));

    QHash<QString, QString> result;
    result.reserve(values.size());
    for (qsizetype i = 0; i < values.size(); ++i)
        result.insert(values.at(i).toString(), labels.at(i).toString());
    return result;
}

void requiredPersistedKeysArePresentExactlyOnce()
{
    const QStringList expectedKeys{
        QStringLiteral("settings/nightMode"),
        QStringLiteral("playback/maxStreamingBitrateMbps"),
        QStringLiteral("playback/preferRemux"),
        QStringLiteral("settings/audioDelayMs"),
        QStringLiteral("settings/audioOutputMode"),
        QStringLiteral("subtitles/language"),
        QStringLiteral("subtitles/mode"),
        QStringLiteral("subtitles/burnIn"),
        QStringLiteral("subtitles/renderPgs"),
        QStringLiteral("subtitles/alwaysBurnInWhenTranscoding"),
        QStringLiteral("subtitles/styling"),
        QStringLiteral("subtitles/textSize"),
        QStringLiteral("subtitles/textWeight"),
        QStringLiteral("subtitles/font"),
        QStringLiteral("subtitles/textColor"),
        QStringLiteral("subtitles/dropShadow"),
        QStringLiteral("subtitles/textBackground"),
        QStringLiteral("subtitles/verticalPosition"),
        QStringLiteral("settings/toneMappingVisualization"),
        QStringLiteral("input/redButton"),
        QStringLiteral("input/greenButton"),
        QStringLiteral("input/yellowButton"),
        QStringLiteral("input/blueButton"),
    };
    const QSet<QString> expected = stringSet(expectedKeys);

    QHash<QString, int> counts;
    counts.reserve(settingSpecs().size());
    for (const SettingSpec &spec : settingSpecs()) {
        const QString key = keyString(spec);
        counts[key] += 1;
        require(expected.contains(key),
                QStringLiteral("unexpected persisted setting key %1").arg(key));
    }

    require(settingSpecs().size() == expectedKeys.size(),
            QStringLiteral("persisted setting key count changed"));
    for (const QString &key : expectedKeys) {
        require(counts.value(key) == 1,
                QStringLiteral("persisted setting key %1 appeared %2 times")
                    .arg(key)
                    .arg(counts.value(key)));
    }
}

void normalizersPreservePersistedValueSemantics()
{
    const SettingSpec &audioOutput = requiredSpec(QStringLiteral("settings/audioOutputMode"));
    require(normalizedSettingValue(audioOutput, QStringLiteral("starfish")).toString()
                == QStringLiteral("starfish-pcm"),
            QStringLiteral("legacy starfish audio output did not normalize to starfish-pcm"));
    require(normalizedSettingValue(audioOutput, QStringLiteral("starfish-pcm")).toString()
                == QStringLiteral("starfish-pcm"),
            QStringLiteral("starfish-pcm audio output was not preserved"));
    require(normalizedSettingValue(audioOutput, QStringLiteral("unexpected")).toString()
                == QStringLiteral("alsa"),
            QStringLiteral("unknown audio output did not fall back to ALSA"));

    const SettingSpec &bitrate = requiredSpec(QStringLiteral("playback/maxStreamingBitrateMbps"));
    require(normalizedSettingValue(bitrate, QStringLiteral("4")).toInt() == 5,
            QStringLiteral("streaming bitrate below the floor was not clamped"));
    require(normalizedSettingValue(bitrate, QStringLiteral("1001")).toInt() == 1000,
            QStringLiteral("streaming bitrate above the ceiling was not clamped"));
    require(serializedSettingValue(bitrate, QStringLiteral("42")) == QStringLiteral("42"),
            QStringLiteral("in-range streaming bitrate was not serialized unchanged"));

    const SettingSpec &nightMode = requiredSpec(QStringLiteral("settings/nightMode"));
    require(normalizedSettingValue(nightMode, true).toBool(),
            QStringLiteral("boolean QVariant true did not normalize to true"));
    require(normalizedSettingValue(nightMode, QStringLiteral(" YES ")).toBool(),
            QStringLiteral("yes boolean text did not normalize to true"));
    require(normalizedSettingValue(nightMode, QStringLiteral("1")).toBool(),
            QStringLiteral("1 boolean text did not normalize to true"));
    require(!normalizedSettingValue(nightMode, QStringLiteral("0")).toBool(),
            QStringLiteral("0 boolean text did not normalize to false"));
    require(serializedSettingValue(nightMode, QStringLiteral("yes")) == QStringLiteral("true"),
            QStringLiteral("truthy boolean text did not serialize to true"));

    const SettingSpec &textColor = requiredSpec(QStringLiteral("subtitles/textColor"));
    require(normalizedSettingValue(textColor, QStringLiteral(" #A0b1C2 ")).toString()
                == QStringLiteral("#a0b1c2"),
            QStringLiteral("valid subtitle colour was not trimmed and lower-cased"));
    require(normalizedSettingValue(textColor, QStringLiteral("blue")).toString()
                == QStringLiteral("#ffffff"),
            QStringLiteral("invalid subtitle colour did not fall back to white"));

    const SettingSpec &dropShadow = requiredSpec(QStringLiteral("subtitles/dropShadow"));
    require(normalizedSettingValue(dropShadow, QStringLiteral("uniform")).toString()
                == QStringLiteral("uniform"),
            QStringLiteral("valid subtitle drop-shadow choice was not preserved"));
    require(normalizedSettingValue(dropShadow, QStringLiteral("outer-glow")).toString().isEmpty(),
            QStringLiteral("invalid subtitle drop-shadow choice did not fall back to the default choice"));
}

void schemaModelRowsMatchVisibilityContract()
{
    const QVariantList model = settingSchemaModel();
    require(model.size() == settingSpecs().size(),
            QStringLiteral("schema model did not expose one row per setting spec"));

    QSet<QString> modelKeys;
    QStringList hiddenKeys;
    for (const QVariant &item : model) {
        const QVariantMap row = item.toMap();
        const QString key = row.value(QStringLiteral("key")).toString();
        require(!key.isEmpty(), QStringLiteral("schema row had no key"));
        require(!modelKeys.contains(key),
                QStringLiteral("schema model exposed duplicate row for %1").arg(key));
        modelKeys.insert(key);

        const SettingSpec &spec = requiredSpec(key);
        require(row.value(QStringLiteral("visible")).toBool() == spec.visible,
                QStringLiteral("schema row visibility diverged for %1").arg(key));
        if (!row.value(QStringLiteral("visible")).toBool())
            hiddenKeys.push_back(key);
    }

    require(modelKeys.size() == settingSpecs().size(),
            QStringLiteral("schema model key set did not match setting specs"));
    for (const SettingSpec &spec : settingSpecs()) {
        const QString key = keyString(spec);
        require(modelKeys.contains(key),
                QStringLiteral("schema model missed setting row %1").arg(key));
    }

    require(hiddenKeys == QStringList{QStringLiteral("subtitles/textBackground")},
            QStringLiteral("schema model should expose only visible rows plus hidden subtitles/textBackground"));
}

void buttonChoicesAndLabelsExposePlayerActions()
{
    const QStringList buttonKeys{
        QStringLiteral("input/redButton"),
        QStringLiteral("input/greenButton"),
        QStringLiteral("input/yellowButton"),
        QStringLiteral("input/blueButton"),
    };
    const QStringList expectedActions{
        QStringLiteral("none"),
        QStringLiteral("togglePause"),
        QStringLiteral("toggleSubs"),
        QStringLiteral("cycleSubs"),
        QStringLiteral("cycleAudio"),
        QStringLiteral("skipBack10"),
        QStringLiteral("skipForward10"),
        QStringLiteral("skipBack30"),
        QStringLiteral("skipForward30"),
        QStringLiteral("skipBack90"),
        QStringLiteral("skipForward90"),
        QStringLiteral("skipBackAndEnableSubs"),
        QStringLiteral("skipSegment"),
        QStringLiteral("showInfo"),
        QStringLiteral("stop"),
    };
    const QSet<QString> expectedActionSet = stringSet(expectedActions);

    for (const QString &key : buttonKeys) {
        const SettingSpec &spec = requiredSpec(key);
        require(spec.choiceCount == expectedActions.size(),
                QStringLiteral("button setting %1 exposed an unexpected action count").arg(key));
        require(stringSet(choiceValues(spec)) == expectedActionSet,
                QStringLiteral("button setting %1 did not expose the expected actions").arg(key));

        const QVariantMap row = schemaRow(key);
        require(!row.isEmpty(), QStringLiteral("schema model missed button row %1").arg(key));
        const QHash<QString, QString> modelLabels = choicesByLabelFromRow(row);
        require(modelLabels.size() == expectedActions.size(),
                QStringLiteral("schema model button labels changed for %1").arg(key));

        require(choiceLabel(spec, QStringLiteral("togglePause")) == QStringLiteral("Play / Pause"),
                QStringLiteral("togglePause label changed"));
        require(choiceLabel(spec, QStringLiteral("toggleSubs")) == QStringLiteral("Toggle subtitles"),
                QStringLiteral("toggleSubs label changed"));
        require(choiceLabel(spec, QStringLiteral("cycleAudio")) == QStringLiteral("Cycle audio track"),
                QStringLiteral("cycleAudio label changed"));
        require(choiceLabel(spec, QStringLiteral("skipBackAndEnableSubs"))
                    == QStringLiteral("Skip back 10 s + enable subs"),
                QStringLiteral("skipBackAndEnableSubs label changed"));
        require(choiceLabel(spec, QStringLiteral("skipSegment")) == QStringLiteral("Skip intro / outro"),
                QStringLiteral("skipSegment label changed"));
        require(choiceLabel(spec, QStringLiteral("stop")) == QStringLiteral("Stop playback"),
                QStringLiteral("stop label changed"));

        require(modelLabels.value(QStringLiteral("togglePause")) == QStringLiteral("Play / Pause"),
                QStringLiteral("schema model togglePause label changed"));
        require(modelLabels.value(QStringLiteral("skipBackAndEnableSubs"))
                    == QStringLiteral("Skip back 10 s + enable subs"),
                QStringLiteral("schema model skipBackAndEnableSubs label changed"));
        require(modelLabels.value(QStringLiteral("skipSegment")) == QStringLiteral("Skip intro / outro"),
                QStringLiteral("schema model skipSegment label changed"));
    }
}

} // namespace

int main()
{
    requiredPersistedKeysArePresentExactlyOnce();
    normalizersPreservePersistedValueSemantics();
    schemaModelRowsMatchVisibilityContract();
    buttonChoicesAndLabelsExposePlayerActions();
    return EXIT_SUCCESS;
}
