# Issue List

Up Next:

* Num Lock causes modifier checks to fail in xedit.c
* Implement X Selection protocol for handling clipboard and primary selections
* Right click in tags region should search edit region
* Add keyboard shortcut to highlight the thing under the cursor
* Tag line count should account for wrapped lines
* ctrl+alt+f should find next occurence of previous search term
* invalid memory accesses while viewing docs/waf
* check for file changes on save
* check for file changes when window regains focus
* Use $SHELL if defined, fallback on /bin/sh
* Set ENV to ~/.config/edit/rc if undefined

The Rest:

* Selection bug when selecting text with tabs
* add a distinct state for pointer move versus drag
* Add a SaveAs tag that takes an argument for the filename to save as
* Add a GoTo tag for ctags lookup and line number jump
* implement command diffing logic to optimize the undo/redo log
* add command line flags to toggle options (Tabs, Indent, etc..)
* Add a ctrl+space shortcut to autocomplete ctag
* off by one error on scrolling up with wrapped lines
* 100% coverage with unit and unit-integration tests
* shortcut to repeat previous operation

The Future:

* Run commands in the background and don't block the main thread.

# Auxillary Programs

* Visual diff tool
* Win-like terminal emulator
* File browser
* Acme-like window manager
