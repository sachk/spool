.pragma library

function itemIdFor(item) {
    return item ? String(item.movieId || item.itemId || item.id || "") : ""
}

function itemTypeFor(item) {
    return item ? String(item.itemType || item.type || "") : ""
}

function modelCount(model) {
    return model && model.rowCount ? model.rowCount() : 0
}

function modelItem(model, index) {
    return model && model.get && index >= 0 && index < modelCount(model) ? model.get(index) : ({})
}

function modelIndexForItemId(model, itemId, fallbackIndex) {
    const count = modelCount(model)
    const id = String(itemId || "")
    if (id.length > 0) {
        for (let i = 0; i < count; ++i) {
            if (itemIdFor(model.get(i)) === id)
                return i
        }
    }
    return count > 0 ? Math.max(0, Math.min(Number(fallbackIndex || 0), count - 1)) : -1
}

function detailsContext(args, fallbackModel) {
    const routeArgs = args || ({})
    const model = routeArgs.model || fallbackModel
    const index = modelIndexForItemId(model, routeArgs.itemId, routeArgs.focusIndex)
    let item = modelItem(model, index)
    let effectiveIndex = index
    if (itemIdFor(item).length <= 0 && String(routeArgs.itemId || "").length > 0) {
        item = {
            movieId: String(routeArgs.itemId),
            itemType: String(routeArgs.itemType || "Video"),
            title: String(routeArgs.title || "Selected item"),
            seriesId: String(routeArgs.seriesId || ""),
            seasonId: String(routeArgs.seasonId || ""),
            playable: true
        }
        effectiveIndex = 0
    }
    return {
        model: model,
        index: effectiveIndex,
        item: item,
        source: String(routeArgs.source || "movies"),
        returnRoute: String(routeArgs.returnRoute || "libraryGrid")
    }
}

function normalizeDetailsRoute(request, fallbackModel, currentRoute) {
    const nextModel = request && request.model ? request.model : fallbackModel
    const focusIndex = Math.max(0, Number(request && request.focusIndex !== undefined ? request.focusIndex : 0))
    const fallbackItem = modelItem(nextModel, focusIndex)
    const requestedItemId = request ? String(request.itemId || "") : ""
    const itemId = requestedItemId.length > 0 ? requestedItemId : itemIdFor(fallbackItem)
    if (itemId.length <= 0)
        return null

    return {
        model: nextModel,
        itemId: itemId,
        itemType: request && request.itemType ? String(request.itemType) : itemTypeFor(fallbackItem),
        source: request && request.source ? String(request.source) : "movies",
        returnRoute: request && request.returnRoute ? String(request.returnRoute) : (currentRoute || "libraryGrid"),
        focusIndex: focusIndex,
        title: request && request.title ? String(request.title) : String(fallbackItem.title || fallbackItem.seriesName
                                                                         || ""),
        seriesId: request && request.seriesId ? String(request.seriesId) : String(fallbackItem.seriesId || ""),
        seasonId: request && request.seasonId ? String(request.seasonId) : String(fallbackItem.seasonId || "")
    }
}

function detailsRouteAt(model, index, source, returnRoute, currentRoute) {
    const focusIndex = Math.max(0, Number(index || 0))
    const item = modelItem(model, focusIndex)
    return normalizeDetailsRoute({
                                     model: model,
                                     itemId: itemIdFor(item),
                                     itemType: itemTypeFor(item),
                                     title: String(item.title || item.seriesName || ""),
                                     seriesId: String(item.seriesId || ""),
                                     seasonId: String(item.seasonId || ""),
                                     source: source || "movies",
                                     returnRoute: returnRoute || currentRoute || "libraryGrid",
                                     focusIndex: focusIndex
                                 }, model, currentRoute)
}
