# xpick -- fuzzy find an item from a list of items

## SYNOPSIS

`xpick` [_query_]

## DESCRIPTION

`xpick` Takes a list of items on standard input delimited by newlines and an
optional initial _query_. A gui window is then presented to the user which
allows the user to filter the list using a fuzzy-find algorithm. The user's
selection is printed to standard output upon completion. If no option is
selected `xpick` exits with no output.

### Fuzzy-Find Algorithm

TODO: Document this

## ENVIRONMENT

* `XPICKTITLE`:
    If this variable is set its contents are used to populate the status region
    of the `xpick` window.

## AUTHOR

Michael D. Lowis

## SEE ALSO

tide(1) xpick(1) xfilepick(1) xtagpick(1)
