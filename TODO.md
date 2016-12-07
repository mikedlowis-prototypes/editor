# Implementation Tweaks and Bug Fixes

* investigate weird behavior when editing CRLF files and copy-pasting
* Expand tabs setting should be disabled if opened file contains tabs
* Add tag for ctags lookup and line number jump
* add a shortcut to autocomplete ctag
* Add a tools dir to namespace utility scripts only useful inside the editor
* off by one error on scrolling up with wrapped lines
* block selection should handle brace-balancing
* Use select to check for error strings in exec.c
* Should not be able to undo initial tag line text insertion
* track down double click bug for selecting whole line
* Implement minimal regex search (per Kernighan article)
* Implement fuzzy file/buffer/tag picker
* check for file changes when window regains focus
* check for file changes on save
* backspace should delete indent if preceded by whitespace
* indenting with a reverse selection  expands the selection the wrong way
* add command line flags to toggle options (Tabs, Indent, etc..)
* shift+click to extend selection
* drag with middle and right mouse buttons causes infinite loops

# Auxillary Programs

* Acme-like window manager
* Win-like terminal emulator
* File browser
* Webkit-based web browser

# Graphical User Interface

* Display line location and num lines in status

# Command Language

* Implement a Sam/Acme-like command language

# Maybe Someday Features

* Implement fuse file-system backend?
* Spell checker integration
* Syntax highlighting
* Implement full featured regex engine
