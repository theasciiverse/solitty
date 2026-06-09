#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>

#include "common.h"

int g_x_offset = 0;
int g_y_offset = 0;

bool term_size_ok(void) {
  int lines, columns;
  getmaxyx(stdscr, lines, columns);
  return(lines >= MIN_TERM_LINES && columns >= MIN_TERM_COLS);
}

void update_board_offsets(void) {
  int lines, cols;
  getmaxyx(stdscr, lines, cols);
  g_x_offset = (cols > MIN_TERM_COLS) ? (cols - MIN_TERM_COLS) / 2 : 0;
  g_y_offset = (lines > MIN_TERM_LINES) ? (lines - MIN_TERM_LINES) / 2 : 0;
}

void tty_solitaire_generic_error(int errno, char *file, int line) {
  char message[TTY_SOLITAIRE_BUFSIZ];
  snprintf(message,
           TTY_SOLITAIRE_BUFSIZ,
           "%s: %s (%s:%d)",
           program_name,
           strerror(errno),
           file,
           line - 1);
  fprintf(stderr, "%s\n", message);
  exit(errno);
}
