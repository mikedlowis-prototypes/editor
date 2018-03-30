# Issue List

Up Next:

* Add matching logic for "", '', ``, and <>
* B2+B1 click executes command with selection as argument
* add command line flags to toggle options (Tabs, Indent, etc..)

Tags:

* Clear - Clear the current window's edit buffer
* Line - Get the current line number(s) containing the selection
* Kill - Kill a background command
* Font - Toggle font between monospace and proportional or set the font
* ID - Get window id of the X window
* Zerox - Create a copy of the window
* Lang - Set the language for syntax highlighting

The Future:

* Wily-like rc file for registering builtins
* Case insensitive search
* Ctrl+Up,Down requires two undos to revert.
* Ctrl+Up,Down with non line selection should track column
* use transaction ids to only mark buffer dirty when it really is
* refactor selection handling to buf.c to prepare for multiple selections.
* 100% coverage with unit and unit-integration tests
* tab inserts dont coalesce like one would expect
* implement command diffing logic to optimize the undo/redo log
* move by words is inconsistent. Example:
    var infoId = 'readerinfo'+reader.id;

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

# Fetch Rule Syntax

include [FILE]
set [NAME] [VALUE]
unset [NAME] [VALUE]
is [NAME] [VALUE]
isdir [VALUE]
isfile [VALUE]
findfile [VALUE]
matches [NAME] [REGEX]
exec [CMD] [ARGS...]

