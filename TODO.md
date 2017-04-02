# Issue List

Up Next:

* Status line should omit characters from beginning of path to make file path fit
* clipboard does not persist after exit
* Run commands in the background and don't block the main thread.
* Make Fn keys execute nth command in the tags buffer

* check for file changes on save
* check for file changes when window regains focus
* Right click in tags region should search edit region
* 100% coverage with unit and unit-integration tests
* Add a SaveAs tag that takes an argument for the filename to save as
* Add a GoTo tag for ctags lookup and line number jump (or right click magic?) 
* Add keyboard shortcut to highlight the thing under the cursor
* right click to fetch file or line
* selecting text should set PRIMARY x11 selection

Straight-up Bugs:

* tab inserts dont coalesce like one would expect
* off by one error on scrolling up with wrapped lines
* Ctrl+u with a selection clears to bol *before* selected text

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
