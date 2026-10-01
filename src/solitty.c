#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "game.h"
#include "keyboard.h"
#include "mouse.h"

#ifndef VERSION
#define VERSION "n/a"
#endif

const char *program_name;
struct game game;

void version(void);
void usage(const char *);
void draw_greeting(void);

/* Parse the --passes argument: a whole number of at least 1. Anything else
 * (garbage, 0, negative, overflow) is rejected instead of silently becoming
 * 0 via atoi(). */
static int parse_passes(const char *arg) {
  char *end;
  long passes;

  errno = 0;
  passes = strtol(arg, &end, 10);
  if (errno != 0 || end == arg || *end != '\0' || passes < 1 ||
      passes > INT_MAX) {
    fprintf(stderr, "%s: --passes expects a whole number of at least 1, got \"%s\"\n",
            program_name, arg);
    exit(1);
  }
  return ((int)passes);
}

/* Put the terminal back (leave curses mode, show the cursor) when we are
 * terminated, e.g. by SIGTERM or the terminal window closing (SIGHUP).
 * raw() turns off ISIG, so ^C and ^Z arrive as plain keys, not signals.
 * endwin() is not formally async-signal-safe, but we exit right after it
 * and nothing else touches curses state, which is the usual practice. */
static void handle_termination(int sig) {
  endwin();
  _exit(128 + sig);
}

static void install_signal_handlers(void) {
  struct sigaction action;

  action.sa_handler = handle_termination;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGHUP, &action, NULL);
  sigaction(SIGQUIT, &action, NULL);
}

int main(int argc, char *argv[]) {
  int option;
  int option_index;
  int passes_through_deck = 3;
  static int four_color_deck;
  static int no_background_color;
  static const struct option options[] = {
      {"help", no_argument, NULL, 'h'},
      {"version", no_argument, NULL, 'v'},
      {"passes", required_argument, NULL, 'p'},
      {"four-color-deck", no_argument, &four_color_deck, 1},
      {"no-background-color", no_argument, &no_background_color, 1},
      {0, 0, 0, 0}};

  program_name = basename(argv[0]);

  while ((option = getopt_long(argc, argv, "hvp:", options, &option_index)) !=
         -1) {
    switch (option) {
    case 'v':
      version();
      exit(0);
    case 'p':
      passes_through_deck = parse_passes(optarg);
      break;
    case 'h':
    case '?':
      usage(program_name);
      exit(0);
    case 0:
      /* A flag option such as --four-color-deck; getopt_long already set it. */
      break;
    default:
      usage(program_name);
      exit(0);
    }
  }

  /* Seed once per process, not once per shuffle. */
  srand(time(NULL));

  setlocale(LC_ALL, "");
  initscr();
  raw();
  noecho();
  keypad(stdscr, TRUE);
  start_color();
  mouse_init();
  curs_set(FALSE);
  set_escdelay(0);
  install_signal_handlers();
  if (no_background_color) {
    use_default_colors();
  } else {
    assume_default_colors(COLOR_WHITE, COLOR_GREEN);
  }
  init_pair(1, COLOR_BLACK, COLOR_WHITE);
  init_pair(2, COLOR_RED, COLOR_WHITE);
  init_pair(3, COLOR_GREEN, COLOR_WHITE);
  init_pair(4, COLOR_YELLOW, COLOR_WHITE);
  init_pair(5, COLOR_WHITE, COLOR_BLUE);
  init_pair(6, COLOR_WHITE, COLOR_GREEN);

  update_board_offsets();

  int key;

  while (!term_size_ok()) {
    clear();
    mvprintw(1, 1, SMALL_TERM_MSG);
    refresh();
    if ((key = getch()) == 'q' || key == 'Q') {
      endwin();
      return (0);
    }
    update_board_offsets();
  }

  clear();
  draw_greeting();
  refresh();

  for (;;) {
    if ((key = getch()) == 'q' || key == 'Q') {
      endwin();
      return (0);
    }
    if (key == KEY_RESIZE) {
      update_board_offsets();
    }
    if (term_size_ok()) {
      clear();
      draw_greeting();
      refresh();
      if (key == KEY_SPACEBAR) {
        clear();
        refresh();
        game_init(&game, passes_through_deck, four_color_deck);
        break;
      }
    } else if (key == KEY_RESIZE) {
      clear();
      mvprintw(1, 1, SMALL_TERM_MSG);
      refresh();
    }
  }

  do {
    keyboard_event(getch());
  } while (!game_won());

  endwin();
  game_end();
  printf("You won.\n");

  return (0);
}

void draw_greeting(void) {
  int lines, cols;
  getmaxyx(stdscr, lines, cols);
  int yo = (lines - 10) / 2 - 8;
  if (yo < 0) yo = 0;
#define GCENTER(text) ((cols - (int)(sizeof(text) - 1)) / 2)
  mvprintw(8 + yo,  GCENTER("Welcome to solitty."),                              "Welcome to solitty.");
  mvprintw(10 + yo, GCENTER("Move with the arrow keys or <hjkl>."),              "Move with the arrow keys or <hjkl>.");
  mvprintw(12 + yo, GCENTER("Use the space bar to select and place cards."),     "Use the space bar to select and place cards.");
  mvprintw(13 + yo, GCENTER("Or click to pick up/place, right-click to cancel."), "Or click to pick up/place, right-click to cancel.");
  mvprintw(14 + yo, GCENTER("After selecting a card you can use <m> to select more"), "After selecting a card you can use <m> to select more");
  mvprintw(15 + yo, GCENTER("and <n> to select fewer. Press <Shift+M> to select all."), "and <n> to select fewer. Press <Shift+M> to select all.");
  mvprintw(17 + yo, GCENTER("Press the space bar to play or q to quit."),        "Press the space bar to play or q to quit.");
#undef GCENTER
}

void usage(const char *program_name) {
  printf("usage: %s [OPTIONS]\n", program_name);
  printf("  -v, --version              Show version\n");
  printf("  -h, --help                 Show this message\n");
  printf("  -p, --passes               Number of passes through the deck  "
         "(default: 3)\n");
  printf("      --four-color-deck      Draw unique card suit colors       "
         "(default: false)\n");
  printf("      --no-background-color  Don't draw background color        "
         "(default: false)\n");
}

void version(void) { printf("%s\n", VERSION); }
