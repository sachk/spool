#define __STDC_CONSTANT_MACROS
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

} // namespace

int main()
{
    const std::string license = avutil_license();
    const std::string configuration = avutil_configuration();

    require(license.starts_with("LGPL"), "FFmpeg must report an LGPL license");
    require(configuration.find("--disable-everything") != std::string::npos,
        "FFmpeg must start from a disable-everything feature set");
    require(configuration.find("--disable-gpl") != std::string::npos, "FFmpeg must explicitly disable GPL code");
    require(configuration.find("--disable-version3") != std::string::npos,
        "FFmpeg must explicitly disable version-3-only code");
    require(
        configuration.find("--disable-nonfree") != std::string::npos, "FFmpeg must explicitly disable non-free code");
    require(configuration.find("--enable-gpl") == std::string::npos,
        "FFmpeg configuration must not contain an inherited GPL enable flag");
    require(configuration.find("--enable-version3") == std::string::npos,
        "FFmpeg configuration must not contain an inherited version-3 enable flag");

    require(av_find_input_format("hls") != nullptr, "FFmpeg must retain Jellyfin HLS transcode playback");
    constexpr const char *nightModeFilters[]
        = { "alimiter", "compand", "dialoguenhance", "equalizer", "highpass", "pan", "speechnorm", "treble" };
    for (const char *filter : nightModeFilters)
        require(avfilter_get_by_name(filter) != nullptr, "FFmpeg must retain every Night Mode audio filter");
    return EXIT_SUCCESS;
}
