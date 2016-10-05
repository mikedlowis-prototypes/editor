# Internals and Miscellaneous

* Re-work column tracking to keep the cursor reasonably close to where expected
* Calculate line numbers and keep up to date while editing
* Support multiple buffers
* Implement undo/redo log
* Implement ctags lookup and regeneration
* Implement cscope lookup and regeneration
* Implement fuzzy file/buffer/tag picker
* Implement omnicomplete pop-up
* Implement Tab for command/tag execution

# Unicode

* Fix display of asian scripts and combining characters
* Read the file in bytewise and detect the encoding and line endings. Use that info to perform cursor movements and redisplay.

# Graphical User Interface

* Display modified status of buffer
* Display name of file in status
* Display line location and num lines in status
* Display file encoding in status
* Display line ending style in status
* Implement visual mode

# Mouse User Interface

* Left click to move cursor
* Left click and drag to select text
* Double left click to select word
* Double left click on newline or empty space to select line
* Triple left click to select "big" word (contiguous block of non-ws chars)
* Right click to plumb word or search
* Copy to clipboard on select

# Keyboard User Interface

* Implement VI-like keybindings

# Command Language

* Implement a Sam/Acme-like command language

# Maybe Someday Features

* Implement fuse file-system backend?
* Spell checker integration
* Syntax highlighting
