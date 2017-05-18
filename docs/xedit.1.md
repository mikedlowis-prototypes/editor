# xedit -- a text editor inspired by acme(1) from Plan 9 and Inferno

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

The scroll bar is located to the left of the content region. It's operation is
similar to that of acme(1) or 9term(1). A left click on the scroll bar will 
scroll the adjacent line in the content region to the bottom of the window. A 
right click will move the adjacent line in the content region to the top of the 
window. A middle click will jump to an approximate location in the file 
determined by calculating the vertical distance from the top of the scrollbar 
and applying that as a percentage to the offset in the file.

### Typing and Editing

Typed characters in `xedit` are delivered to the currently active region. Which
region is active is determined by the placement of the mouse or by keyboard 
shortcut. That is to say, the focus follows the mouse much as it does in acme(1)
but there is a keyboard shortcut that allows users to toggle focus between the 
content region and the tag region without using the mouse.

The mechanics of editing text in the tag and content region is identical with 
the exception of searching and saving. Edited content in the tag region is not 
saved to disk unlike the content region. This region is considered a scratch 
buffer for commands, notes, and other bits of text that are placed there 
temporarily. The content region displays the current view of the file being 
edited and can be flushed to disk when requested.

Searching with a term selected in the tags region will search for the term in 
the content region rather than the tags region. In this way a user can edit the 
search term incrementally and perform repeated searches through the content 
region. Searching for a term in the content region *will* search for the term in
the content region however.

## TEXT SELECTION

`xedit` uses a series of rules to determine how much text to select when the 
user executes a context sensitive selection, a search, or a context sensitive 
execution. The following rules are applied in order until a match is found.

1. `Cursor over '(' or ')'`:
    Highlight text between the parentheses including nested parentheses.

2. `Cursor over '[' or ']'`:
    Highlight text between the brackets including nested brackets.

3. `Cursor over '{' or '}'`:
    Highlight text between the braces including nested braces.
    
4. `Cursor at beginning or end of line`:
    Highlight the entire line (including the newline)
    
5. `Cursor over alphanumeric character or underscore`:
    Highlight the word under the cursor consisting of only alphanumeric and 
    underscore characters.
    
If none of the above rules match, `xedit` will simply highlight the block of 
non-whitespace characters under the cursor.

## MOUSE HANDLING

* `Left Button`:
    The left mouse button is used for selecting text or moving the cursor. A
    single-click will move the mose to the clicked location. A double-click will
    will select the object under the clicked location based on context as 
    described in `TEXT SELECTION`. A triple-click will select the largest
    contiguous chunk of non-whitespace characters at the clicked location.

* `Middle Button`:
    The middle mouse button is used for executing text at the clicked location.
    The command to be executed is determined by the context rules defined in
    the `TEXT SELECTION` section. The cursor position is not changed on a middle 
    click.

* `Right Button`:
    The right button is used to search for the next occurrence of the clicked 
    text. The search term is determined by the context rules defined in the 
    `TEXT SELECTION` section. The search direction follows the direction of the
    previous search operation. The `Shift` key can be held in combination with a
    click of the right mosue button in order to reverse the search direction.

## COMMAND EXECUTION

`xedit` allows for the execution of any arbitrary text as a command. The input 
and output to/from each command executed can be controlled by prepending one of
a set of sigils defined below. These sigils instruct `xedit` from where the 
command will receive its input and where it will place its output (both standard 
and errors).

* `'!' - Run command detached from editor`:
    The command will be executed in the background and all of its input and 
    output file descriptors will be closed.

* `'<' - Input from command`:
    The command will be executed in the background and its standard output will 
    be placed in the content region. Its error output will be placed in the tags 
    region.

* `'>' - Output to command`:
    The command will be executed in the background. The currently selected text
    will be written to the command's standard input. The command's standard 
    output and standard error content will be written to the tags region.

* `'|' - Pipe through command`:
    The command will be executed in the background. The currently selected text
    will be written to the command's standard input. The command's standard 
    output will replace the currently selected text. Any error output will be
    placed in the tags region.
    
* `':' - Pipe through sed(1)`:
    Identical to '|' except that the command is always sed(1). This is a 
    convenience shortcut to allow  quick and easy access to sed for editing 
    blocks of text.

* `Commands with no sigil`:
    Commands with none of the aforementioned sigils will be executed in the 
    background and have their standard output placed in the content region and 
    their error output placed in the tags region.

## KEYBOARD SHORTCUTS

### Unix Standard Shortcuts

* `Ctrl+u`:
    Delete from the cursor position to the beginning of the line.

* `Ctrl+k`:
    Delete from the cursor position to the end of the line.

* `Ctrl+w`:
    Delete the word to the left.

* `Ctrl+a`:
    Move cursor to the beginning of the line.

* `Ctrl+e`:
    Move cursor to the end of the line.

### Cursor Movement and Selection

The key combinations below are responsible for moving the cursor around the
document by character, by word, or by line. The `Shift` modifier key can be
applied to any of them to also extend the current selection to the new cursor
position.

* `Escape`:
    Highlight the last contiguous block of inserted text or clear the current
    selection (deselect the currently selected text).

* `Left`:
    Move the cursor one character to the left.

* `Right`:
    Move the cursor one character to the right.

* `Up`:
    Move the cursor to the previous line.

* `Down`:
    Move the cursor to the next line.
    
* `Ctrl+Up`:
    Move the current line or selection up a line.

* `Ctrl+Down`:
    Move the current line or selection down a line.

* `Ctrl+Left`:
    Move the cursor to the beginning of the word to the left.

* `Ctrl+Right`:
    Move the cursor to the end of the word to the right.

### Modern Text Editing Shortcuts

* `Ctrl+s`:
    Save the contents of the content region to disk.

* `Ctrl+z`:
    Undo the last change performed on the active region.

* `Ctrl+y`:
    Redo the previously undone change on the active region.

* `Ctrl+x`:
    Cut the selected text to the X11 CLIPBOARD selection. If no text is selected
    then the current line is cut.

* `Ctrl+c`:
    Copy the selected text to the X11 CLIPBOARD selection. If no text is selected
    then the current line is copied.

* `Ctrl+v`:
    Paste the contents of the X11 CLIPBOARD selection to the active region.

* `Delete`:
    Delete the character to the right.

* `Ctrl+Delete`:
    Delete the word to the right.

* `Backspace`:
    Delete the character to the left.

* `Ctrl+Backspace`:
    Delete the word to the left.

* `Ctrl+Enter`:
    Create a new line after the current line and place the cursor there.

* `Ctrl+Shift+Enter`:
    Create a new line before the current line and place the cursor there.

* `PageUp`:
    Scroll the active region up by one screenful of text. The cursor is not
    affected by this operation.

* `PageDn`:
    Scroll the active region down by one screenful of text. The cursor is not
    affected by this  operation.

### Search Shortcuts

The shortcuts below allow the user to search for selected text or by context.
The direction of the search defaults to the forward direction with regard to the
position in the file. Each search follows the direction of the previous search
unless the `Shift` modifier is applied. The `Shift` modifier causes the current
search operation to be applied in the opposite direction of the previous.

* `Ctrl+f`:
    Search for the next occurrence of the selected text in the content region.
    If no text is currently selected, the text under the cursor is selected
    based on context as described in `TEXT SELECTION`.

* `Ctrl+Alt+f`:
    Search for the next occurence previous search term in the content region.

### Implementation-specific Shortcuts

* `Ctrl+[`:
    Decrease the indent level of the selected text.

* `Ctrl+]`:
    Increase the indent level of the selected text.

* `Ctrl+h`:
    Highlight the item under cursor following the rules in `TEXT SELECTION`

* `Ctrl+t`:
    Toggle focus between the tags region and the content region.

* `Ctrl+q`:
    Quit the editor. If the file is modified a warning will be printed in the
    tags region and the editor will not quit. Executing the  shortcut twice
    within 250ms will ignore the warning and quit the editor without saving.

* `Ctrl+d`:
    Execute the selected text as described in `COMMAND EXECUTION`. If no text
    is selected, the text under cursor is selecte dbased on context as described
    in `TEXT SELECTION`.

* `Ctrl+o`:
    Launch xfilepick(1) to choose a file from a recursive list of files in the
    current deirectory and sub directories. This file will be opened in a
    new instance of `xedit`.

* `Ctrl+p`:
    Launch xtagpick(1) to select a tag from a ctags(1) generated index file.
    `xedit` will jump to the selected ctag definition in the current window if
    the file is currently being edited. Otherwise, a new instance of `xedit`
    will be launched with the target file and the cursor set to the line
    containing the definition.

* `Ctrl+g`:
    Lookup the selected symbol or symbol under the cursor in a ctags(1)
    generated index file. Jump to the location of the definition if it exist in
    the current file. Otherwise, a new instance of `xedit` will be launched with
    the target file and the cursor set to the line containing the definition.

* `Ctrl+n`:
    Open a new instance of `xedit` with no filename.

## BUILTINS

* `Cut`:
    Cut the selection to the X11 CLIPBOARD selection.

* `Copy`:
    Copy the selection to the X11 CLIPBOARD selection.

* `Eol`:
    Toggle the line-ending style for the buffers contents between LF and CRLF

* `Find [term]`:
    Find the next occurrence of the selected text.

* `GoTo [arg]`:
    Jump to a specific line number or symbol.

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

* `SaveAs [path]`:
    Save the contents of the buffer to disk. If a path argument is provided, the 
    buffer will be saved to the new path.

* `Tabs`:
    Toggle the expand tabs featuer on or off.

* `Undo`:
    Undo the previous edit.

## FILES

* `$HOME/.config/edit/editrc`:
    Shell script loaded in current environment to make shell functions and
    environment variables available to xedit(1)

## ENVIRONMENT

* `EDITTAGS`:
    The contents of this environment variable is used to initialize the contents
    of the tags region of the window.

* `SHELL`:
    The contents of this variable are used as the shell in which all non-builtin
    commands are executed. If this variable is not defined, sh(1) is used
    as a fallback shell.

## AUTHOR

Michael D. Lowis

## SEE ALSO

xedit(1) xpick(1) xfilepick(1) xtagpick(1)
