import QtQuick
import QtTest
import "../../qml/shell/ItemActivation.js" as ItemActivation

// Activation used to be branched on itemType in four places, and they had
// drifted apart. These cases are the union of what those four did, so the
// shared rule cannot quietly lose one of them again.
TestCase {
    name: "ItemActivation"

    function recorder() {
        return {
            "calls": [],
            "playFromModel": function (model, index) {
                this.calls.push(["playFromModel", model, index])
            },
            "openPerson": function (person) {
                this.calls.push(["openPerson", person.id, person.name])
            },
            "pushRoute": function (route) {
                this.calls.push(["pushRoute", route])
            },
            "openDetailsAt": function (model, index, source, returnRoute) {
                this.calls.push(["openDetailsAt", model, index, source, returnRoute])
            }
        }
    }

    function browseContext() {
        return {
            "source": "search",
            "returnRoute": "search",
            "browseRoute": "libraryGrid"
        }
    }

    function test_ordinaryItemsOpenTheirDetails() {
        const decision = ItemActivation.plan({
                                                 "itemType": "Movie"
                                             }, browseContext())
        compare(decision.action, "details")
        compare(decision.source, "search")
        compare(decision.returnRoute, "search")
    }

    // An album is a details page, but a distinct kind of one, and every
    // caller that knew that spelled it the same way.
    function test_albumsCarryTheirOwnSource() {
        compare(ItemActivation.plan({
                                        "itemType": "MusicAlbum"
                                    }, browseContext()).source, "album")
    }

    function test_peopleOpenTheirCredits() {
        const decision = ItemActivation.plan({
                                                 "itemType": "Person",
                                                 "movieId": "p1",
                                                 "title": "Ada"
                                             }, browseContext())
        compare(decision.action, "person")
        compare(decision.person.id, "p1")
        compare(decision.person.name, "Ada")
    }

    function test_containersPlayRatherThanOpen() {
        const types = ["Playlist", "Folder", "PhotoAlbum", "MusicArtist"]
        for (const type of types) {
            const decision = ItemActivation.plan({
                                                     "itemType": type
                                                 }, browseContext())
            compare(decision.action, "playModel", type + " should play")
            compare(decision.browseRoute, "libraryGrid")
        }
    }

    // The library grid is already the browse route, so it passes no
    // browseRoute and must not stack a second one on itself.
    function test_aCallerAlreadyBrowsingDoesNotPushAnotherRoute() {
        const app = recorder()
        const shell = recorder()
        ItemActivation.open({
                                "itemType": "Playlist"
                            }, {
                                "source": "movies",
                                "returnRoute": "libraryGrid",
                                "browseRoute": ""
                            }, app, shell, "model", 4)
        compare(app.calls.length, 1)
        compare(app.calls[0][0], "playFromModel")
        compare(app.calls[0][2], 4)
        compare(shell.calls.length, 0)
    }

    function test_searchPushesTheBrowseRouteAfterPlaying() {
        const app = recorder()
        const shell = recorder()
        ItemActivation.open({
                                "itemType": "MusicArtist"
                            }, browseContext(), app, shell, "model", 2)
        compare(app.calls[0][0], "playFromModel")
        compare(shell.calls[0][0], "pushRoute")
        compare(shell.calls[0][1], "libraryGrid")
    }

    function test_performRoutesDetailsThroughTheShell() {
        const shell = recorder()
        ItemActivation.open({
                                "itemType": "Series"
                            }, browseContext(), recorder(), shell, "model", 7)
        compare(shell.calls[0], ["openDetailsAt", "model", 7, "search", "search"])
    }

    // The sheet over the player has no shell. It must be able to ask the
    // question without the answer throwing.
    function test_aCallerWithoutAShellIsToldItCannotOpen() {
        compare(ItemActivation.open({
                                        "itemType": "Movie"
                                    }, browseContext(), recorder(), null, "model", 0), false)
        compare(ItemActivation.open({
                                        "itemType": "Person",
                                        "movieId": "p1"
                                    }, browseContext(), recorder(), null, "model", 0), false)
    }

    function test_aPersonWithNoIdIsNotOpened() {
        compare(ItemActivation.open({
                                        "itemType": "Person"
                                    }, browseContext(), recorder(), recorder(), "model", 0), false)
    }

    // What the compact sheet asks to decide between descending and adding.
    function test_childBearingKindsAreRecognised() {
        for (const type of ["Series", "Season", "MusicAlbum", "BoxSet", "Playlist", "Folder"])
            verify(ItemActivation.hasChildren({
                                                  "itemType": type
                                              }), type + " should have children")
        for (const type of ["Movie", "Episode", "Audio", "Person"])
            verify(!ItemActivation.hasChildren({
                                                   "itemType": type
                                               }), type + " should not")
    }

    function test_episodicKindsAreTheOnesThatCanBeQueuedWhole() {
        verify(ItemActivation.isEpisodic({
                                             "itemType": "Series"
                                         }))
        verify(ItemActivation.isEpisodic({
                                             "itemType": "Season"
                                         }))
        verify(!ItemActivation.isEpisodic({
                                              "itemType": "Episode"
                                          }))
        verify(!ItemActivation.isEpisodic({}))
    }

    function test_missingItemsDegradeToDetails() {
        compare(ItemActivation.plan(null, {}).action, "details")
        compare(ItemActivation.plan(null, {}).source, "movies")
    }
}
