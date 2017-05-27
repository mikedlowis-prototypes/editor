static void select_line(void) {
    View* view = win_view(FOCUSED);
    view_eol(view, false);
    view_selctx(view);
}

static void select_all(void) {
    View* view = win_view(FOCUSED);
    view->selection.beg = 0;
    view->selection.end = buf_end(&(view->buffer));
}

static void join_lines(void) {
    View* view = win_view(FOCUSED);
    view_eol(view, false);
    view_delete(view, RIGHT, false);
    Rune r = view_getrune(view);
    for (; r == '\t' || r == ' '; r = view_getrune(view))
        view_byrune(view, RIGHT, true);
    if (r != '\n' && r != RUNE_CRLF)
        view_insert(view, false, ' ');
}

static void delete(void) {
    bool byword = x11_keymodsset(ModCtrl);
    view_delete(win_view(FOCUSED), RIGHT, byword);
}

static void onpaste(char* text) {
    view_putstr(win_view(FOCUSED), text);
}

static void cut(void) {
    /* select the current line if no selection */
    if (!view_selsize(win_view(FOCUSED)))
        select_line();
    /* now perform the cut */
    char* str = view_getstr(win_view(FOCUSED), NULL);
    x11_sel_set(CLIPBOARD, str);
    if (str && *str) delete();
}

static void paste(void) {
    assert(x11_sel_get(CLIPBOARD, onpaste));
}

static void copy(void) {
    /* select the current line if no selection */
    if (!view_selsize(win_view(FOCUSED)))
        select_line();
    char* str = view_getstr(win_view(FOCUSED), NULL);
    x11_sel_set(CLIPBOARD, str);
}

static void del_to_bol(void) {
    view_bol(win_view(FOCUSED), true);
    if (view_selsize(win_view(FOCUSED)) > 0)
        delete();
}

static void del_to_eol(void) {
    view_eol(win_view(FOCUSED), true);
    if (view_selsize(win_view(FOCUSED)) > 0)
        delete();
}

static void del_to_bow(void) {
    view_byword(win_view(FOCUSED), LEFT, true);
    if (view_selsize(win_view(FOCUSED)) > 0)
        delete();
}

static void backspace(void) {
    bool byword = x11_keymodsset(ModCtrl);
    view_delete(win_view(FOCUSED), LEFT, byword);
}

static void cursor_bol(void) {
    view_bol(win_view(FOCUSED), false);
}

static void cursor_eol(void) {
    view_eol(win_view(FOCUSED), false);
}

static void move_line_up(void) {
    if (!view_selsize(win_view(FOCUSED)))
        select_line();
    cut();
    view_byline(win_view(FOCUSED), UP, false);
    paste();
}

static void move_line_dn(void) {
    if (!view_selsize(win_view(FOCUSED)))
        select_line();
    cut();
    cursor_eol();
    view_byrune(win_view(FOCUSED), RIGHT, false);
    paste();
}

static void cursor_home(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        view_bof(win_view(FOCUSED), extsel);
    else
        view_bol(win_view(FOCUSED), extsel);
}

static void cursor_end(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        view_eof(win_view(FOCUSED), extsel);
    else
        view_eol(win_view(FOCUSED), extsel);
}

static void cursor_up(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        move_line_up();
    else
        view_byline(win_view(FOCUSED), UP, extsel);
}

static void cursor_dn(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        move_line_dn();
    else
        view_byline(win_view(FOCUSED), DOWN, extsel);
}

static void cursor_left(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        view_byword(win_view(FOCUSED), LEFT, extsel);
    else
        view_byrune(win_view(FOCUSED), LEFT, extsel);
}

static void cursor_right(void) {
    bool extsel = x11_keymodsset(ModShift);
    if (x11_keymodsset(ModCtrl))
        view_byword(win_view(FOCUSED), RIGHT, extsel);
    else
        view_byrune(win_view(FOCUSED), RIGHT, extsel);
}

static void page_up(void) {
    view_scrollpage(win_view(FOCUSED), UP);
}

static void page_dn(void) {
    view_scrollpage(win_view(FOCUSED), DOWN);
}

static void select_prev(void) {
    view_selprev(win_view(FOCUSED));
}

static void change_focus(void) {
    win_setregion(win_getregion() == TAGS ? EDIT : TAGS);
}

static void undo(void) {
    view_undo(win_view(FOCUSED));
}

static void redo(void) {
    view_redo(win_view(FOCUSED));
}
