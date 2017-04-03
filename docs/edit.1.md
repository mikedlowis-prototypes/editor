# edit -- convenience script for launching xedit(1) with a proper environment

## SYNOPSIS

`edit` [_file_...]

## DESCRIPTION

This script acts as a wrapper around xedit(1). It is responsible for setting up
the environment variables and loading the rc file xedit(1) before launching a 
new instance of xedit(1) for each file provided on the command line. If no files
are provided, xedit(1) will be launched to edit a scratch buffer.

## FILES

* `$HOME/.config/edit/editrc`:
    Shell script loaded in current environment to make shell functions and 
    environment variables available to xedit(1)

## ENVIRONMENT
    
* `BASH_ENV`:
    Set to same value as $EDITRCFILE so that the file is loaded as a bash script 
    in the event that the user shell is bash(1)

* `DISPLAY`:
    This variable is used to determine if we are running in an X11 environment. 
    If $DISPLAY is not set then the contents of the $EDITOR variable is used to 
    determine what editor to launch in lieu of xedit(1). If $EDITOR is not set 
    then vim(1) is launched instead.
    
* `EDITRCFILE`:
    Contains the path of the shell script which is loaded before xedit(1) to 
    setup the environment and define shell functions which can be called during 
    an editing session.

* `EDITOR`:
    Used as a fallback for when not running in an X11 system (xedit(1) is of 
    course X11 only).

* `PATH`:
    The $PATH variable is modified in order to add $HOME/config/edit/tools/ to 
    the path. This folder is a standard location in which user scripts and tools
    can be placed so they can be used from within xedit(1) without cluttering up
    the normal system path.

## AUTHOR

Michael D. Lowis

## SEE ALSO

xedit(1) xpick(1) xfilepick(1) xtagpick(1)

