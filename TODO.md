# Internals

* Calculate line numbers and keep up to date while editing
* Support multiple buffers

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
* Right click to plumb word or search
* Double left click to select word
* Double left click on newline to select line
* Triple left click to select line

# Keyboard User Interface

* Implement VI-like keybindings
