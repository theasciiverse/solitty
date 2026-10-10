#include <stdlib.h>
#include <ncurses.h>
#include <assert.h>

#include "keyboard.h"
#include "card.h"
#include "stack.h"
#include "game.h"
#include "cursor.h"
#include "gui.h"
#include "common.h"
#include "mouse.h"

/* Map a mouse click to a cursor position and queue synthetic key(s).
 *
 * Cards are FRAME_WIDTH(7) wide, spaced 8 apart, starting at x=1.
 * Column n occupies x = [1+8n .. 7+8n], cursor_x = CURSOR_BEGIN_X + 8n.
 *
 * Top row  (y 1..5):  stock | waste | gap | foundation[0..3]
 * Maneuvre (y >= 9):  maneuvre[0..6]
 *
 * For an initial pick-up click on a maneuvre card at depth d below the top,
 * we queue KEY_SPACEBAR followed by d synthetic 'm' presses so that
 * handle_card_movement's existing 'm' handler extends the selection.
 * (ungetch is LIFO, so we push the 'm's first, then KEY_SPACEBAR.)
 *
 * In in_movement mode (inside handle_card_movement's loop):
 *   left-click  -> warp cursor to destination + queue KEY_SPACEBAR
 *   right-click -> queue KEY_ESCAPE (cancel)
 */
static void handle_mouse_event(MEVENT *event, bool in_movement) {
  int mx = event->x;
  int my = event->y;

  if (mx < 1 + g_x_offset) return;
  int col = (mx - 1 - g_x_offset) / 8;
  int col_offset = (mx - 1 - g_x_offset) % 8;
  if (col > 6 || col_offset >= FRAME_WIDTH) return;

  int new_x;
  int new_y;
  int depth = 0;

  if (event->bstate & (BUTTON3_CLICKED | BUTTON3_PRESSED)) {
    if (in_movement) ungetch(KEY_ESCAPE);
    return;
  }

  if (!(event->bstate & (BUTTON1_CLICKED | BUTTON1_PRESSED))) return;

  if (my >= MANEUVRE_BEGIN_Y + g_y_offset) {
    struct stack *s = deck->maneuvre[col];
    new_y = 6 + s->card->frame->begin_y;

    if (!in_movement && !stack_empty(s) && s->card->face == EXPOSED) {
      int top_y = s->card->frame->begin_y;
      depth = top_y - my;
      if (depth < 0) depth = 0;

      /* Cap depth by the number of consecutive exposed cards below the top. */
      int max_depth = 0;
      for (struct stack *node = s->next; node; node = node->next) {
        if (node->card->face != EXPOSED) break;
        max_depth++;
      }
      if (depth > max_depth) depth = max_depth;
    }
  } else if (my >= STOCK_BEGIN_Y + g_y_offset &&
             my <= STOCK_BEGIN_Y + FRAME_HEIGHT - 1 + g_y_offset) {
    new_y = CURSOR_BEGIN_Y + g_y_offset;
  } else {
    return;
  }

  new_x = CURSOR_BEGIN_X + g_x_offset + col * 8;

  erase_cursor(cursor);
  cursor->x = new_x;
  cursor->y = new_y;
  draw_cursor(cursor);

  /* Push in reverse order of desired processing (ungetch is LIFO). */
  for (int i = 0; i < depth; i++) ungetch('m');
  ungetch(KEY_SPACEBAR);
}

static void handle_term_resize(void) {
  clear();
  refresh();
  if (term_size_ok()) {
    int old_x = g_x_offset;
    int old_y = g_y_offset;
    update_board_offsets();
    game_reposition(old_x, old_y);
    draw_deck(deck);
    draw_cursor(cursor);
  } else {
    mvprintw(1, 1, SMALL_TERM_MSG);
  }
}

/* FIXME: this function does not work on stacks with no marked cards.
 * In that case it returns the stack's length. */
static int marked_cards_count(struct stack *stack) {
  if (stack_length(stack) == 1) {
    if (stack->card->frame->begin_y > MANEUVRE_BEGIN_Y + g_y_offset) {
      return(1);
    }
  } else if (stack_length(stack) > 1) {
    for (int marked_cards_count = 0; stack; stack = stack->next) {
      marked_cards_count++;
      if (!stack->next || (stack->card->frame->begin_y - stack->next->card->frame->begin_y) > 1) {
        return(marked_cards_count);
      }
    }
  }
  return(0);
}

/* Put a cursor that is in the maneuvre row back under its column's top
 * card. Called after every drop or cancel, so the cursor ends up in the
 * right place whatever the move did (a card or block landing on an empty
 * column, the auto-move to a foundation, or no move at all). */
static void cursor_to_top_card(struct cursor *cursor) {
  if (cursor->y > CURSOR_BEGIN_Y + g_y_offset) {
    cursor_move(cursor, UP);
    cursor_move(cursor, DOWN);
  }
}

static void unmark_cards(struct stack *stack) {
  int _marked_cards_count = marked_cards_count(stack);
  for (int i = 0; i < _marked_cards_count; stack = stack->next, i++) {
    card_unmark(stack->card);
  }
}

static void handle_card_movement(struct cursor *cursor) {
  struct stack **origin = cursor_stack(cursor);
  int key;

  if (cursor_on_invalid_spot(cursor) || stack_empty(*origin)) {
    return;
  }
  if (maneuvre_stack(*origin)) {
    erase_stack(*origin);
    card_mark((*origin)->card);
    draw_stack(*origin);
    cursor->y++;
  }
  erase_cursor(cursor);
  cursor_mark(cursor);
  draw_cursor(cursor);

  for (;;) {
    if ((key = getch()) == 'q' || key == 'Q') {
      endwin();
      game_end();
      exit(0);
    }
    if (term_size_ok()) {
      switch (key) {
      case 'h':
      case 'j':
      case 'k':
      case 'l':
      case KEY_LEFT:
      case KEY_DOWN:
      case KEY_UP:
      case KEY_RIGHT:
        erase_cursor(cursor);
        cursor_move(cursor, cursor_direction(key));
        draw_cursor(cursor);
        break;
      case 'm':
        if (origin == cursor_stack(cursor) && maneuvre_stack(*origin)) {
          for (struct stack *i = *origin; i && i->next; i = i->next) {
            if (i->next->card->face == EXPOSED &&
                  (i->card->frame->begin_y - i->next->card->frame->begin_y) > 1) {
              erase_stack(*origin);
              card_mark(i->next->card);
              draw_stack(*origin);
              break;
            }
          }
        }
        break;
      case 'M':
        if (origin == cursor_stack(cursor) && maneuvre_stack(*origin)) {
          for (struct stack *i = *origin; i && i->next; i = i->next) {
            if (i->next->card->face == EXPOSED &&
                  (i->card->frame->begin_y - i->next->card->frame->begin_y) > 1) {
              erase_stack(*origin);
              card_mark(i->next->card);
              draw_stack(*origin);
            }
          }
        }
        break;
      case 'n':
        if (origin == cursor_stack(cursor) && maneuvre_stack(*origin)) {
          for (struct stack *i = (*origin)->next; i; i = i->next) {
            if (i->next) {
              if ((i->card->frame->begin_y - i->next->card->frame->begin_y) > 1) {
                erase_stack(*origin);
                card_unmark(i->card);
                draw_stack(*origin);
                break;
              }
            } else {
              if (i->card->frame->begin_y == (MANEUVRE_BEGIN_Y + 1 + g_y_offset)) {
                erase_stack(*origin);
                card_unmark(i->card);
                draw_stack(*origin);
                break;
              }
            }
          }
        }
        break;
      case 'N':
        if (origin == cursor_stack(cursor) && maneuvre_stack(*origin)) {
          erase_stack(*origin);
          unmark_cards(*origin);
          card_mark((*origin)->card);
          draw_stack(*origin);
        }
        break;

      case KEY_SPACEBAR:;
        /* http://www.mail-archive.com/gcc-bugs@gcc.gnu.org/msg259382.html */
        struct stack **destination = cursor_stack(cursor);
        int _marked_cards_count = marked_cards_count(*origin);
        if (maneuvre_stack(*origin) && _marked_cards_count > 0) {
          erase_stack(*origin);
          unmark_cards(*origin);
          draw_stack(*origin);
        }
        if (destination) {
          erase_stack(*origin);
          erase_cursor(cursor);
          if (_marked_cards_count > 1 &&
                maneuvre_stack(*origin) &&
                maneuvre_stack(*destination)) {
            struct stack *block = *origin;
            for (int i = 1; i < _marked_cards_count; block = block->next, i++)
              ;
            if (valid_move(block, *destination)) {
              move_block(origin, destination, _marked_cards_count);
            }
          } else {
            if (valid_move(*origin, *destination)) {
              move_card(origin, destination);
            }
          }

          /* If they only had one card selected... */
          if (*origin == *destination && ((maneuvre_stack(*origin)  && _marked_cards_count == 1) || waste_pile_stack(*origin))) {
            /* see if we can automatically move card to foundation */
            for (int i = 0; i <= 3; ++i) {
              struct stack **foundation = &(deck->foundation[i]);
              if (valid_move(*origin, *foundation)) {
                move_card(origin, foundation);
                draw_stack(*foundation);
                break;
              }
            }
          }

          draw_stack(*origin);
          draw_stack(*destination);
        }
        erase_cursor(cursor);
        cursor_to_top_card(cursor);
        cursor_unmark(cursor);
        draw_cursor(cursor);
        return;
      case KEY_ESCAPE:
        if (marked_cards_count(*origin) > 0 && maneuvre_stack(*origin)) {
          erase_stack(*origin);
          unmark_cards(*origin);
          draw_stack(*origin);
        }
        erase_cursor(cursor);
        cursor_to_top_card(cursor);
        cursor_unmark(cursor);
        draw_cursor(cursor);
        return;
      case KEY_MOUSE:;
        MEVENT mevent_mv;
        if (getmouse(&mevent_mv) == OK) {
          handle_mouse_event(&mevent_mv, true);
        }
        break;
      case KEY_RESIZE:
        handle_term_resize();
        break;
      }
    } else if (key == KEY_RESIZE) {
      handle_term_resize();
    }
  }
}

void keyboard_event(int key) {
  if (key == 'q' || key == 'Q') {
    endwin();
    game_end();
    exit(0);
  }
  if (term_size_ok()) {
    switch (key) {
    case 'h':
    case 'j':
    case 'k':
    case 'l':
    case KEY_LEFT:
    case KEY_DOWN:
    case KEY_UP:
    case KEY_RIGHT:
      erase_cursor(cursor);
      cursor_move(cursor, cursor_direction(key));
      draw_cursor(cursor);
      break;
    case KEY_SPACEBAR:
      if (cursor_on_stock(cursor)) {
        if (stack_empty(deck->stock)) {
          if (game.passes_through_deck_left >= 1) {
            while (!stack_empty(deck->waste_pile)) {
              move_card(&(deck->waste_pile), &(deck->stock));
              card_cover(deck->stock->card);
            }
            draw_stack(deck->stock);
            draw_stack(deck->waste_pile);
          }
        } else {
          move_card(&(deck->stock), &(deck->waste_pile));
          if (stack_empty(deck->stock)) {
            game.passes_through_deck_left--;
          }
          card_expose(deck->waste_pile->card);
          erase_stack(deck->waste_pile);
          draw_stack(deck->stock);
          draw_stack(deck->waste_pile);
        }
      } else {
        struct card *card;
        if (cursor_stack(cursor) &&
              (card = (*cursor_stack(cursor))->card)->face == COVERED) {
          card_expose(card);
          draw_card(card);
        } else {
          handle_card_movement(cursor);
        }
      }
      break;
    case KEY_MOUSE:;
      MEVENT mevent_kv;
      if (getmouse(&mevent_kv) == OK) {
        handle_mouse_event(&mevent_kv, false);
      }
      break;
    case KEY_RESIZE:
      handle_term_resize();
      break;
    }
  } else if (key == KEY_RESIZE) {
    handle_term_resize();
  }
}
