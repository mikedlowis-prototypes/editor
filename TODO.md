# Issue List

Up Next:

* move by words is inconsistent. Example:
    var infoId = 'readerinfo'+reader.id;
* Add a way to CD using a builtin (buffers will track original dir)
* shortcut to jump to previous edit
* xrandr dpi setting does not affect tide like it should
* add command line flags to toggle options (Tabs, Indent, etc..)

The Future:

* Ctrl+Shift+Enter copies indent of wrong line
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
* add command env vars to set options (Tabs, Indent, etc..)
* implement command diffing logic to optimize the undo/redo log
* Status line should omit characters from beginning of path to make file path fit

Possible Shortcuts:

* Ctrl+{,} - Move to start or end brace of block
* Ctrl+(,) - Move to start or end of paragraph
* Ctrl+'   - Move to matching brace, bracket, or paren

Maybe think about addressing these later:

* add current dir to path
* add support for guidefiles
* Shift+Insert should insert primary selection
* Find shortcut should select previous word if current char is newline
* diagnostic messages can stack up if deselected and not resolved
* mouse click and hold on scroll bar should continually scroll

# Auxillary Programs

* Visual diff tool
* Win-like terminal emulator
* File browser
* Acme-like window manager
