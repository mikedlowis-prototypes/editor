# Issue List

Up Next:

* Implement tab completion
* synchronize title with status contents.
* B2+B1 click executes command with selection as argument
* Run commands in the background and don't block the main thread.
* update man pages with new functionality
* tfetch variable expansion needs to be reworked
* rename tide to twin and tctl to tide

* moving from tags to the gutter does not transfer focus to edit region
* implement transaction control in buf.c
* Add a way to CD using a builtin (buffers will track original dir)
* shortcut to jump to previous edit
* add command line flags to toggle options (Tabs, Indent, etc..)
* move by words is inconsistent. Example:
    var infoId = 'readerinfo'+reader.id;

The Future:

* Case insensitive search
* Ctrl+Up,Down requires two undos to revert.
* Ctrl+Up,Down with non line selection should track column
* Ctrl+Shift+Enter copies indent of wrong line
* use transaction ids to only mark buffer dirty when it really is
* refactor selection handling to buf.c to prepare for multiple selections.
* 100% coverage with unit and unit-integration tests
* tab inserts dont coalesce like one would expect
* implement command diffing logic to optimize the undo/redo log
* Status line should omit characters from beginning of path to make file path fit
* pickfile directory traversal when no tags file

Possible Shortcuts:

* Ctrl+/   - to comment/uncomment based on syntax
* Ctrl+{,} - Move to start or end brace of block
* Ctrl+(,) - Move to start or end of paragraph
* Ctrl+'   - Move to matching brace, bracket, or paren
* Ctrl+.   - repeat previous operation

Maybe think about addressing these later:

* switch buf.c to using a mmap buffer of utf-8/binary data
* add current dir to path
* add support for guidefiles
* Shift+Insert should insert primary selection
* Find shortcut should select previous word if current char is newline
* diagnostic messages can stack up if deselected and not resolved
* mouse click and hold on scroll bar should continually scroll

# Auxillary Programs

* Visual diff tool
* Win-like terminal emulator
* File browser
* Acme-like window manager

# Regular Expressions

Operators:

    c       Literal character
    ^       Start of string
    $       End of string
    .       Any char
    [ ]     Any character in the set
    [^ ]    Any character not in the reange
    ( )     Grouping/matching

    ?       Zero or one
    *       Zero or more
    +       One or more

    {n,m}   From n to m matches
    |       Alternative

# tcmd Tags

build
fetch
ls
cat
cd

# Fetch Rule Syntax

include [FILE]
set [NAME] [VALUE]
unset [NAME] [VALUE]
is [NAME] [VALUE]
isdir [VALUE]
isfile [VALUE]
findfile [VALUE]
matches [NAME] [REGEX]
exec [CMD] [ARGS...]

# Syntax Highlighting

Label, Conditional, Repeat, Character, Number, PreProc, Float, Operator, Structure

Keyword GROUP [WORD...]
Match   GROUP REGEX
Region  GROUP start=REGEX skip=REGEX end=REGEX
