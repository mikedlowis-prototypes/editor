# Implementation Tweaks and Bug Fixes

* Arbitrary command execution (<|> operators)
* Add tag for ctags lookup
* Implement minimal regex search (per Kernighan article)

* off by one error on scrolling wrapped lines
* block selection should handle brace-balancing
* Right click should fetch file if it exists, search otherwise
    * directories in browse?

# Internals and Miscellaneous

* Mode to expand tabs to spaces
* Calculate line numbers and keep up to date while editing
* Implement ctags lookup and regeneration
* Implement cscope lookup and regeneration
* Implement fuzzy file/buffer/tag picker
* Implement omnicomplete pop-up
* Implement keyboard shortcut for command/tag execution

# Graphical User Interface

* Display line location and num lines in status
* Implement visual mode

# Command Language

* Implement a Sam/Acme-like command language

# Maybe Someday Features

* Implement fuse file-system backend?
* Spell checker integration
* Syntax highlighting
