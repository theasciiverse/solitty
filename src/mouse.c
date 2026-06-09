#include <ncurses.h>
#include "mouse.h"

void mouse_init(void) {
  mousemask(BUTTON1_CLICKED | BUTTON1_PRESSED | BUTTON3_CLICKED | BUTTON3_PRESSED,
            NULL);
}
