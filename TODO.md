# Implementation Tweaks and Bug Fixes

* Should not be able to undo initial tag line text insertion
* capture stderr of executed commands and place it in tags view
* Disallow scrolling past end of buffer
* track down double click bug  for selecting whole line
* Add tag for ctags lookup
* Implement minimal regex search (per Kernighan article)
* Implement fuzzy file/buffer/tag picker
* Implement omnicomplete pop-up
* Mode to expand tabs to spaces
* Auto indent mode
* off by one error on scrolling up with wrapped lines
* block selection should handle brace-balancing
* Right click should fetch file if it exists, search otherwise
    * directories in browse?

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

