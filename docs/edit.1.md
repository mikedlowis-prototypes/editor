# edit -- convenience script for launching xedit(1) with a proper environment

## SYNOPSIS

    edit [file ..]

## DESCRIPTION

This script acts as a wrapper around xedit(1). It is responsible for setting up
the environment variables and loading the rc file xedit(1) before launching a 
new instance of xedit(1) for each file provided on the command line. If no files
are provided, xedit(1) will be launched to edit a scratch buffer.

## FILES

    $HOME/.config/edit/editrc
        Shell script loaded in current environment to make shell functions and environment variables available to xedit(1)

## ENVIRONMENT
    
    BASH_ENV
    DISPLAY
    EDITRCFILE
    EDITOR
    PATH
    
## AUTHOR

Michael D. Lowis

## SEE ALSO

xedit(1) xpick(1) xfilepick(1) xtagpick(1)

