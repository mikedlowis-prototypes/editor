# Issue List

Up Next:

* invalid memory accesses while viewing docs/waf
* Tag line count should account for wrapped lines
* block selection should handle brace-balancing
* context sensitive selection of words, commands, line numbers, or filenames.
* ctrl+alt+f should find next occurence of previous search term
* check for file changes on save
* check for file changes when window regains focus

The Rest: 

* Implement X Selection protocol for handling clipboard and primary selections
* add a distinct state for pointer move versus drag
* Add a SaveAs tag that takes an argument for the filename to save as
* Add a GoTo tag for ctags lookup and line number jump
* Add a tools dir to namespace utility scripts only useful inside the editor
* implement command diffing logic to optimize the undo/redo log
* add command line flags to toggle options (Tabs, Indent, etc..)
* backspace should delete indent if preceded by whitespace
* Add a ctrl+space shortcut to autocomplete ctag
* off by one error on scrolling up with wrapped lines
* Auto-save on focus change or quit

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
