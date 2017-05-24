# Issue List

Up Next:

* Make Fn keys execute nth command in the tags buffers
* arrow keys with selection should clear the selection instead of moving.
* move by words is inconsistent. Example:
    var infoId = 'readerinfo'+reader.id;

The Future:

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
