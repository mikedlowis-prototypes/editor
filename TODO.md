# Issue List

Up Next:

* refactor selection handling to buf.c to prepare fo rmultiple selections.
* refactor x11.c and win.c
* Make Fn keys execute nth command in the tags buffers
* Run commands in the background and don't block the main thread.
* check for file changes on save
* check for file changes when window regains focus
* 100% coverage with unit and unit-integration tests
* right click to fetch file or line
* Status line should omit characters from beginning of path to make file path fit

Straight-up Bugs:

* tab inserts dont coalesce like one would expect

The Future:

* shortcut to repeat previous operation
* add command line flags to toggle options (Tabs, Indent, etc..)
* add command env vars to set options (Tabs, Indent, etc..)
* implement command diffing logic to optimize the undo/redo log

# Auxillary Programs

* Visual diff tool
* Win-like terminal emulator
* File browser
* Acme-like window manager
