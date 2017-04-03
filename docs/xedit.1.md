# xedit -- a text editor inspired by Acme from Plan 9 and Inferno

## SYNOPSIS

`xedit` [_file_]

## DESCRIPTION

`xedit` is a text editor inspired by the Acme editor from the Plan 9 and Inferno
operating systems. Unlike Acme, `xedit` is a single window, single file editor.
Instead of baking a window manager into the editor, this job is relegated to an
X11 window manager. It is recommended that `xedit` be used with a tiling window
manager such as dwm(1) or spectrwm(1). These window managers will make dealing
with multiple windows much easier and more efficient.

### Windows

`xedit` windows are divided into four basic regions: a one-line status, an 
expanding tags region, a main content region and an adjacent scroll region. The 
status region contains a set of symbolic flags indicating the current state of 
the editor followed by the path of the file being edited. The tags region acts 
as scratch buffer for commands that can be executed to affect the file or the
state of the editor. As the content of this region grows it will expand up to a
quarter of the size of the window shrinking the main coantent region in kind. 
The main content region displays a view of the file currently being edited. To
the left of the content region is a narrow vertical region matching the height
of the content region. This region acts as a scrollbar for the content region. 

### Scrolling
### Typing

## MOUSE HANDLING
## COMMAND EXECUTION

## KEYBOARD SHORTCUTS

* `Ctrl+u`:
* `Ctrl+k`:
* `Ctrl+w`:
* `Ctrl+a`:
* `Ctrl+e`:

* `Ctrl+s`:
* `Ctrl+z`:
* `Ctrl+y`:
* `Ctrl+x`:
* `Ctrl+c`:
* `Ctrl+v`:

* `Ctrl+[`:
* `Ctrl+]`:

## BUILTINS

* `Cut`: 
    Cut the selection to the X11 CLIPBOARD selection.
* `Copy`: 
    Copy the selection to the X11 CLIPBOARD selection.
* `Eol`: 
    Toggle the line-ending style for the buffers contents between LF and CRLF
* `Find`:
    Find the next occurrence of the selected text.
* `Indent`:
    Toggle the autoindent feature on or off.
* `Paste`:
    Paste the contents of the X11 CLIPBOARD selection into the buffer.
* `Quit`:
    Quit the editor.
* `Redo`:
    Redo the last undone change.
* `Save`:
    Save the contents of the buffer to disk.
* `Tabs`:
    Toggle the expand tabs featuer on or off.
* `Undo`:
    Undo the previous edit.

## FILES

* `$HOME/.config/edit/editrc`:
    Shell script loaded in current environment to make shell functions and 
    environment variables available to xedit(1)

## ENVIRONMENT

## BUGS
## AUTHOR

Michael D. Lowis

## SEE ALSO

xedit(1) xpick(1) xfilepick(1) xtagpick(1)
