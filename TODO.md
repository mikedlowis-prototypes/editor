# Issue List

Up Next:

* Change PageUp/Dn to move cursor by screenfuls
* Add Ctrl+Shift+A shortcut to select all
* move by words is inconsistent. Example:
    var infoId = 'readerinfo'+reader.id;
* Add a way to CD using a builtin (buffers will track original dir)
* Ctrl+Shift+Enter copies indent of wrong line
* Shift+Insert should insert primary selection

The Future:

* highlight all matches of search term
* Make Fn keys execute nth command in the tags buffers
* jump to previous or next line with less indent
* use transaction ids to only mark buffer dirty when it really is
* refactor selection handling to buf.c to prepare for multiple selections.
* 100% coverage with unit and unit-integration tests
* right click to fetch file or line
* tab inserts dont coalesce like one would expect
* Run commands in the background and don't block the main thread.
* shortcut to repeat previous operation
* add command line flags to toggle options (Tabs, Indent, etc..)
* add command env vars to set options (Tabs, Indent, etc..)
* implement command diffing logic to optimize the undo/redo log
* Status line should omit characters from beginning of path to make file path fit

Maybe think about addressing these later:

* Find shortcut should select previous word if current char is newline
* Ctrl+\ Shortcut to warp cursor to middle of current screen.
* diagnostic messages can stack up if deselected and not resolved

# Auxillary Programs

* Visual diff tool
* Win-like terminal emulator
* File browser
* Acme-like window manager
