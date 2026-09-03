#pragma once

namespace JellyfinNative {

// Answers one question on a real television: can the app install an IPK by
// itself, on a set that has not been rooted?
//
// It is a probe rather than a feature because LS2 will not tell you what a
// role permits -- you find out by making the call and reading which kind of
// "no" comes back. A denial from the bus ("Denied method call ... for security
// reasons") means the role forbids it and no amount of retrying will help; an
// error from the service itself means the call was allowed through and only
// the request was wrong, which is a solvable problem. Those two look nothing
// alike and mean opposite things, so the probe logs replies verbatim and
// judges nothing.
//
// Built only when SPOOL_WEBOS_SELFUPDATE_PROBE is on; it never ships.
void startSelfUpdateProbe();

} // namespace JellyfinNative
