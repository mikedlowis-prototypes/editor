# Issue List

Up Next:

* ctrl+d with no selection does word selection instead of context
* add config option to change default tab style
* Diagnostic messages should order options so most frequent is closest to cursor
* get rid of edit wrapper script
* Add a way to CD using a builtin
* Ctrl+Shift+N to set a mark, Ctrl+N to jump to a mark
* Make Fn keys execute nth command in the tags buffers
* move by words is inconsistent. Example:
    var infoId = 'readerinfo'+reader.id;
* rename to tide

The Future:

* Ctrl+Shift+g to jump to undo a goto line action
* Shortcut to warp cursor to middle of current screen.
* Find shortcut should select previous word if current char is newline
* diagnostic messages can stack up if deselected and not resolved
* highlight all matches of search term
* Ctrl+Shift+Enter copies indent of wrong line

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

# Auxillary Programs

* Visual diff tool
* Win-like terminal emulator
* File browser
* Acme-like window manager
