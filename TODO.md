# Issue List

Up Next:

* Tag line count should account for wrapped lines
* Middle click on nothing quits the editor
* Indent/Unindent on first line of buffer causes infinite loop
* block selection should handle brace-balancing
* context sensitive selection of words, commands, line numbers, or filenames.
* ctrl+f should move the pointer to the match
* ctrl+shift+f should find next occurence of previous search term
* check for file changes on save
* check for file changes when window regains focus

The Rest: 

* Implement EWMH hooks to prevent window manager killing client with modifications
* Implement X Selection protocol for handling clipboard and primary selections
* add a distinct state for pointer move versus drag
* Add a SaveAs tag that takes an argument for the filename to save as
* Add a GoTo tag for ctags lookup and line number jump
* Use select to check for error strings in exec.c
* Expand tabs setting should be disabled if opened file contains tabs
* Add a tools dir to namespace utility scripts only useful inside the editor
* shift+click to extend selection
* implement command diffing logic to optimize the undo/redo log
* add command line flags to toggle options (Tabs, Indent, etc..)
* backspace should delete indent if preceded by whitespace
* Add a ctrl+space shortcut to autocomplete ctag
* off by one error on scrolling up with wrapped lines

# Auxillary Programs

* Acme-like window manager
* Win-like terminal emulator
* File browser
* Webkit-based web browser

# Graphical User Interface

* Display line location and num lines in status

# Maybe Someday Features

* Implement fuse file-system backend?
* Spell checker integration
* Syntax highlighting
* Implement full featured regex engine
