# pick -- fuzzy find an item from a list of items

## SYNOPSIS

`pick` [_query_]

## DESCRIPTION

`pick` Takes a list of items on standard input delimited by newlines and an
optional initial _query_. A gui window is then presented to the user which
allows the user to filter the list using a fuzzy-find algorithm. The user's
selection is printed to standard output upon completion. If no option is
selected `pick` exits with no output.

### Fuzzy-Find Algorithm

TODO: Document this

## ENVIRONMENT

* `XPICKTITLE`:
    If this variable is set its contents are used to populate the status region
    of the `pick` window.

## AUTHOR

Michael D. Lowis

## SEE ALSO

tide(1) pick(1) pickfile(1) picktag(1)
