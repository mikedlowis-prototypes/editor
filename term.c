#include <stdc.h>
#include <x11.h>
#include <utf.h>
#include <edit.h>
#include <ctype.h>
#include <win.h>

void onmouseleft(WinRegion id, size_t count, size_t row, size_t col) {

}

void onmousemiddle(WinRegion id, size_t count, size_t row, size_t col) {

}

void onmouseright(WinRegion id, size_t count, size_t row, size_t col) {

}

void onscroll(double percent) {

}

void onupdate(void) {
}

void onlayout(void) {
}

void onshutdown(void) {
    x11_deinit();
}

void onerror(char* msg) {

}

#ifndef TEST
int main(int argc, char** argv) {
    win_window("term", onerror);
    //win_setkeys(&Bindings);
    //win_setmouse(&MouseHandlers);
    win_loop();
    return 0;
}
#endif
