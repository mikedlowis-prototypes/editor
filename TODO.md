# Implementation Tweaks and Bug Fixes

* disable autoindent when pasting text
* update getstr to select context when selection is null
* Use select to check for error strings in exec.c
* Should not be able to undo initial tag line text insertion
* Disallow scrolling past end of buffer
* track down double click bug for selecting whole line
* Add tag for ctags lookup
* Implement minimal regex search (per Kernighan article)
* Implement fuzzy file/buffer/tag picker
* Implement omnicomplete pop-up
* off by one error on scrolling up with wrapped lines
* block selection should handle brace-balancing

# Internals and Miscellaneous

* Calculate line numbers and keep up to date while editing
* Implement ctags lookup and regeneration
* Implement cscope lookup and regeneration

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
