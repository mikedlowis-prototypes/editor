# Issue List

Up Next:

* Run commands in the background and don't block the main thread.
* refactor selection handling to buf.c to prepare for multiple selections.
* Make Fn keys execute nth command in the tags buffers
* 100% coverage with unit and unit-integration tests
* Status line should omit characters from beginning of path to make file path fit
* right click to fetch file or line

Straight-up Bugs:

* tab inserts dont coalesce like one would expect

The Future:

* shortcut to repeat previous operation
* add command line flags to toggle options (Tabs, Indent, etc..)
* add command env vars to set options (Tabs, Indent, etc..)
* implement command diffing logic to optimize the undo/redo log

# Mouse Chords

Mouse Sequence      Description
--------------------------------------------------------------------------------
Pl Rl               Null Selection
Pl Rl Pl Rl         Select by context
Pl Rl Pl Rl Pl Rl   Select big word
Pl Pm               Cut line or selection
Pl Pr               Paste
Pm Rm               Execute text
Pm Pr               Cancel the execution that would occur
Pr Rr               Search
Pr Pm               Cancel the search that would occur

# Auxillary Programs

* Visual diff tool
* Win-like terminal emulator
* File browser
* Acme-like window manager
