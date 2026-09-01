.pragma library

// When a page counts as settled -- showing what it is going to show, so the
// route transition that brought it up is over.
//
// The rule lives here because two places need it and they have to agree: the
// route host, which holds a latency transition open until the page settles,
// and the startup splash, which stays up until the first route does. When
// they disagreed, one of them waited forever.
//
// A page that does not declare readiness at all is settled the moment it
// exists. That is the common case -- a static page has nothing to wait for --
// and reading an absent property as "still waiting" would hang every one of
// them.
//
// Note what this deliberately does not say: it does not ask whether the page
// has content. A page that has finished and has nothing to show is settled.
// Pages express that themselves, because only they know the difference
// between an empty answer and one that has not arrived, but the distinction
// is the whole reason this is not just a truthiness check on a model count.
function isSettled(item) {
    if (!item)
        return false
    return typeof item.contentReady === "undefined" || Boolean(item.contentReady)
}
