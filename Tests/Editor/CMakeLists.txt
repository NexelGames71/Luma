# Editor-level tests. We statically link the panels we want to exercise
# directly so we can call their public API without bringing up the full
# editor.

add_subdirectory(panels/ContentBrowser)
