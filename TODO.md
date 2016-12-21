# Implementation Tweaks and Bug Fixes

* Indent on first line of buffer causes infinite loop
* block selection should handle brace-balancing
* Add a GoTo tag for ctags lookup and line number jump
* Add a SaveAs tag that takes an argument for the filename to save as
* Add a ctrl+space shortcut to autocomplete ctag
* Use select to check for error strings in exec.c
* Should not be able to undo initial tag line text insertion
* check for file changes on save
* backspace should delete indent if preceded by whitespace
* context sensitive selection of words, commands, line numbers, or filenames.

Nice to haves: 

* Undo/Redo should set selection to inserted text.
* focus should follow mouse between regions
* Expand tabs setting should be disabled if opened file contains tabs
* Add a tools dir to namespace utility scripts only useful inside the editor
* shift+click to extend selection

Need to reproduce:

* drag with middle and right mouse buttons causes infinite loops
* off by one error on scrolling up with wrapped lines
* Implement minimal regex search (per Kernighan article)
* check for file changes when window regains focus
* add command line flags to toggle options (Tabs, Indent, etc..)
* implement command diffing logic to optimize the undo/redo log

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
