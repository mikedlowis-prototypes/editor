# Implementation Tweaks and Bug Fixes

* Should not be able to undo initial tag line text insertion
* block selection should handle brace-balancing
* Indent on first line of buffer causes infinite loop
* Add a SaveAs tag that takes an argument for the filename to save as
* Add a GoTo tag for ctags lookup and line number jump
* Add a ctrl+space shortcut to autocomplete ctag
* Use select to check for error strings in exec.c
* check for file changes on save
* context sensitive selection of words, commands, line numbers, or filenames.

Nice to haves: 

* Expand tabs setting should be disabled if opened file contains tabs
* Add a tools dir to namespace utility scripts only useful inside the editor
* shift+click to extend selection
* implement command diffing logic to optimize the undo/redo log
* add command line flags to toggle options (Tabs, Indent, etc..)
* check for file changes when window regains focus
* backspace should delete indent if preceded by whitespace

Need to reproduce:

* drag with middle and right mouse buttons causes infinite loops
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
