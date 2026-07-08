#include "common/RequestGeneration.h"

#include <cstdlib>

using JellyfinNative::RequestGeneration;

int main()
{
    RequestGeneration generation;
    const RequestGeneration::Token first = generation.next();
    if (!generation.isCurrent(first))
        return EXIT_FAILURE;

    const RequestGeneration::Token second = generation.next();
    if (generation.isCurrent(first) || !generation.isCurrent(second))
        return EXIT_FAILURE;

    generation.invalidate();
    if (generation.isCurrent(second))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
