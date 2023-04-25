/*
 * Copyright (c) 2023 Y.F.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include <SDL.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

/* Default board size is 48x48 cells */
#define BOARD_WIDTH   48
#define BOARD_HEIGHT  48

/* The "occupied" flag is the 3rd least significant bit of
 * the value representing the state of a cell of the board.
 * It's used to indicate that the cell is currently occupied
 * by the snake.
 * We define OCC_FLAG as (1 << 2), aka 1 shifted two bits to the
 * left, aka binary 100 (4).
 * We use the IS_OCCUPIED(state) macro to test for the flag by
 * a bitwise AND operation of the state against OCC_FLAG.
 * If the state's 3rd least significant bit is 1 - the
 * result will be binary 100, which would also evaluate to a boolean
 * TRUE in the C language. Otheriwise the result will be 0 - FALSE.
*/
#define OCC_FLAG            (1 << 2)
#define IS_OCCUPIED(state)  ((state) & OCC_FLAG)

/* Definitions for directions the snake may face */
#define SNAKE_LEFT  0
#define SNAKE_RIGHT 1
#define SNAKE_UP    2
#define SNAKE_DOWN  3

#define SCREEN_CELL_WIDTH   8
#define SCREEN_CELL_HEIGHT  8
#define WINDOW_WIDTH  (BOARD_WIDTH * SCREEN_CELL_WIDTH)
#define WINDOW_HEIGHT (BOARD_HEIGHT * SCREEN_CELL_HEIGHT)

#define TRUE  1
#define FALSE 0

#define BITS_PER_CELL   3
#define CELLS_PER_PACK  ((sizeof(cell_pack_t) * 8) / BITS_PER_CELL)
#define PACKS_PER_ROW   (BOARD_WIDTH / CELLS_PER_PACK + 1)
#define PACK_INDEX(i)   ((i) / CELLS_PER_PACK)
#define CELL_OFFSET(i)  (((i) % CELLS_PER_PACK) * BITS_PER_CELL)
#define CELL_MASK       ~(~0 << BITS_PER_CELL)

typedef int bool_t;

typedef unsigned int cell_pack_t;

/* We define an enumerate data type cell_state_t which represents the state
 * of a cell on the board.
 * Occupied cells have the occupied flag set to 1 and the direction which
 * the snake turned to while its head was in the cell. We use bitwise OR
 * to combine the occupied flag with the direction.
 * The size of cell_state_t is the same as the size of int.
*/
typedef enum _cell_state
{
  CELL_EMPTY = 0, /* Nothing is in the cell. Binary 000 */
  CELL_HAS_FOOD = 1,  /* There is food in the cell. Binary 001 */
  CELL_OCC_LEFT = (OCC_FLAG | SNAKE_LEFT), /* Occupied, turns left. Binary 100 */
  CELL_OCC_RIGHT = (OCC_FLAG | SNAKE_RIGHT),  /* Occupied, turns right. Binary 101 */
  CELL_OCC_UP = (OCC_FLAG | SNAKE_UP), /* Occupied, turns up. Binary 110 */
  CELL_OCC_DOWN = (OCC_FLAG | SNAKE_DOWN) /* Occupied, turns down. Binary 111 */
} cell_state_t;

/* We define a data type to represent the location of a cell on the board */
typedef struct _cell
{
  int row;
  int column;
} cell_t;

/* This type represents the status of the game - Continue or end */
typedef enum _game_status
{
  GAME_CONT,
  GAME_LOSE
} game_status_t;

/* The board is global static 2D array of
 * BOARD_HEIGHT x BOARD_WIDTH items of type cell_state_t
*/
static cell_pack_t board[BOARD_HEIGHT][PACKS_PER_ROW];

/* The current cells of the snake's head and tail */
static cell_t snake_head, snake_tail, food_loc;

/* We declare function prototypes here */
static bool_t cells_eq(const cell_t* cell1, const cell_t* cell2);
static cell_state_t get_cell(int row, int column);
static void set_cell(int row, int column, cell_state_t state);
static bool_t get_next_cell(cell_t* cell, int direction);
static void gen_food();
static void init_board();
static game_status_t move_snake(int direction);
static void init_window(SDL_Window* window, SDL_Surface* surface);
static void draw_board(SDL_Window* window, SDL_Surface* surface);
static void draw_cell(const cell_t* cell, SDL_Surface* surface, Uint32 color);
static void game_loop(SDL_Window* window, SDL_Surface* surface);

/* Function: cells_wq
 * Purpose: Compare the location of two cells
 * Arguments:
 * cell1: Pointer to the first cell
 * cell2: Pointer to the second cell
 * returns: TRUE if the cells are at the same location, FALSE otherwise.
 */
static bool_t cells_eq(const cell_t* cell1, const cell_t* cell2)
{
  return cell1->row == cell2->row && cell1->column == cell2->column;
}

static cell_state_t get_cell(int row, int column)
{
  return (board[row][PACK_INDEX(column)] >> CELL_OFFSET(column)) & CELL_MASK;
}

/* Function: set_cell
 * Purpose: Update the state of a cell on the board.
 * Arguments:
 * row: row of the cell
 * column: column of the cell
 * state: the new state
 */
static void set_cell(int row, int column, cell_state_t state)
{
  if (row < BOARD_HEIGHT && column < BOARD_WIDTH) /* validate arguments */
  {
    int offset = CELL_OFFSET(column);
    cell_pack_t* pack = &(board[row][PACK_INDEX(column)]);
    cell_pack_t mask = ~(CELL_MASK << offset);
    *pack = (*pack & mask) | (state << offset);
  }
}

/* Function: get_next_cell
 * Purpose: Update cell to the next one in direction.
 * Arguments:
 * cell: Pointer to the cell (Will be modified by this function)
 * direction: The direction of the next cell
 * return: TRUE if updated successfully, FALSE otherwise.
 */
static bool_t get_next_cell(cell_t* cell, int direction)
{
  bool_t valid;
  
  valid = TRUE;
  switch (direction)
  {
    case SNAKE_LEFT:
      cell->column--;
      break;
    case SNAKE_RIGHT:
      cell->column++;
      break;
    case SNAKE_UP:
      cell->row--;
      break;
    case SNAKE_DOWN:
      cell->row++;
      break;
    default:
      valid = FALSE;
  }

  return valid;
}

/* Function: gen_food
 * Purpose: Generate food at a random unoccuppied cell on the board.
 */
static void gen_food()
{
  do
  {
    food_loc.row = rand() % BOARD_HEIGHT;
    food_loc.column = rand() % BOARD_WIDTH;
  } while (get_cell(food_loc.row, food_loc.column) != CELL_EMPTY);
  set_cell(food_loc.row, food_loc.column, CELL_HAS_FOOD);
}

/* Function: init_board
 * Purpose: Set the board to the initial game state.
*/
static void init_board()
{
  int i;
  /* For each row i */
  for (i = 0; i < BOARD_HEIGHT; i++)
  {
    int j;
    /* For each column j */
    for (j = 0; j < BOARD_WIDTH; j++)
      set_cell(i, j, CELL_EMPTY); /* Set cell (i, j) as empty */
  }

  /* Set the snake's head and tail in the middle of the board.
   * They're both at the same cell so the snake is at initial length of 1
  */
  snake_head.row = snake_tail.row = BOARD_HEIGHT / 2;
  snake_head.column = snake_tail.column = BOARD_WIDTH / 2;
}

/* Function: move_snake
 * Purpose: Move the snake to the next cell in direction.
 * Arguments:
 * direction: The direction in which to move the snake.
 * Returns: status of the game (continue or lose).
 */
static game_status_t move_snake(int direction)
{
  game_status_t status;

  /* Update current head cell with the direction */
  set_cell(snake_head.row, snake_head.column, OCC_FLAG | direction);

  /* Update snake head to the next cell */
  if (!get_next_cell(&snake_head, direction))
      return GAME_CONT;

  /* If the head hits the borders of the board or the snake itseslf, game over */
  if (snake_head.row < 0 || snake_head.row >= BOARD_HEIGHT ||
      snake_head.column < 0 || snake_head.column >= BOARD_WIDTH ||
      (!cells_eq(&snake_head, &snake_tail) &&
       IS_OCCUPIED(get_cell(snake_head.row, snake_head.column))))
  {
    status = GAME_LOSE;
  }
  else
  {
    /* Unless the next cell has food, we want to move the tail in its direction,
     * otherwise we keep the tail in position to grow the snake by one cell.
     */
    if (get_cell(snake_head.row, snake_head.column) != CELL_HAS_FOOD)
    {
      int tail_dir = get_cell(snake_tail.row, snake_tail.column) & 3;
      set_cell(snake_tail.row, snake_tail.column, CELL_EMPTY);
      get_next_cell(&snake_tail, tail_dir);
    }
    else
    {
      /* We just ate food, generate new one */
      gen_food();
    }
    /* New head cell is now occupied */
    set_cell(snake_head.row, snake_head.column, OCC_FLAG | direction);
    status = GAME_CONT;
  }

  return status;
}

/* Function: draw_cell
 * Purpose: Draw a cell on the screen.
 * Arguments:
 * cell: Pointer to the cell.
 * surface: The SDL surface to which the cell is drawn.
 * color: Color to pain the cell.
 */
static void draw_cell(const cell_t* cell, SDL_Surface* surface, Uint32 color)
{
  /* A cell is represented as a SCREEN_CELL_WIDTH x SCREEN_CELL_HEIGHT rectangle.
   * Therefore, it's x and y coordinates are column * SCREEN_CELL_WIDTH and
   * x * SCREEN_CELL_HEIGHT, respectively.
   */
  SDL_Rect rect;
  rect.x = cell->column * SCREEN_CELL_WIDTH;
  rect.y = cell->row * SCREEN_CELL_HEIGHT;
  rect.w = SCREEN_CELL_WIDTH;
  rect.h = SCREEN_CELL_HEIGHT;

  /* Fill the rectangle */
  SDL_FillRect(surface, &rect, color);
}

/* Function: init_window
 * Purpose: Draw an initial state on the screen.
 * Arguments:
 * window: SDL window to draw
 * surface: Surface to draw
 */
static void init_window(SDL_Window* window, SDL_Surface* surface)
{
  /* Fill the whole window with black color */
  SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0x0, 0x0, 0x0));
  /* Draw initial state */
  draw_board(window, surface);
}

/* Function: draw_board
 * Purpose: Draw the current state of the game to the window.
 * Arguments:
 * window: SDL window
 * surface: SDL surface
 */
static void draw_board(SDL_Window* window, SDL_Surface* surface)
{
  /* Draw the snake's tail as a green rectangle */
  draw_cell(&snake_tail, surface, SDL_MapRGB(surface->format, 0x0, 0xff, 0x0));
  /* Draw the snake's head as a red rectangle */
  draw_cell(&snake_head, surface, SDL_MapRGB(surface->format, 0xff, 0x0, 0x0));
  /* Draw the food as a blue rectangle */
  draw_cell(&food_loc, surface, SDL_MapRGB(surface->format, 0x0, 0x0, 0xff));
  /* Render the window */
  SDL_UpdateWindowSurface(window);
}

/* Function: game_loop
 * Purpose: Process events for the game until the user quits.
 * Arguments:
 * window: SDL window
 * surface: SDL surface
 */
static void game_loop(SDL_Window* window, SDL_Surface* surface)
{
  SDL_Event e;
  bool_t quit = FALSE;
  while (!quit)
  {
    game_status_t status = GAME_CONT;
    /* New game */
    init_board();
    gen_food();
    init_window(window, surface);

    /* As long as the user didn't quit and the game continues, wait for events */
    while (!quit && status == GAME_CONT && SDL_WaitEvent(&e) != 0)
    {
      if (e.type != SDL_QUIT) /* We want to quit if quit event arrived */
      {
        if (e.type == SDL_KEYDOWN) /* If key-pressed event */
        {
          int direction;
          cell_t old_head_loc = snake_head, old_tail_loc = snake_tail;
          
          switch (e.key.keysym.sym)
          {
            case SDLK_LEFT:
              direction = SNAKE_LEFT;
              break;
            case SDLK_RIGHT:
              direction = SNAKE_RIGHT;
              break;
            case SDLK_UP:
              direction = SNAKE_UP;
              break;
            case SDLK_DOWN:
              direction = SNAKE_DOWN;
              break;
            default:
              /* Non-arrow key pressed. Do nothing and wait for next event. */
              continue;
          }

          status = move_snake(direction);
          /* Draw a green rectangle in the old head location as it is now a
           * regular snake body cell. Will be overriden if the tail and the head
           * were the same
           */
          draw_cell(&old_head_loc, surface, SDL_MapRGB(surface->format, 0x0, 0xff, 0x0));
          /* "Erase" the previous tail by drawing a black rectangle in its location.
           * (It will be drawn again if the tail didn't move. That saves us an
           * if statement).
           */
          draw_cell(&old_tail_loc, surface, SDL_MapRGB(surface->format, 0x0, 0x0, 0x0));
          draw_board(window, surface);
        }
      }
      else
      {
        quit = TRUE;
      }
    }
  }
}

/* main function. Program starts here. */
int main(int argc, char* argv[])
{
  SDL_Window* window;
  SDL_Surface* screen_surface;

  /* Initialize random seed */
  srand(time(NULL));

  /* Initialize SDL */
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
    return 1;
  }

  /* Create the window */
  if ((window = SDL_CreateWindow("Snake",
                                 SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED,
                                 WINDOW_WIDTH, WINDOW_HEIGHT,
                                 SDL_WINDOW_SHOWN)) == NULL)
  {
    fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  /* Get the screen surface of the window */
  screen_surface = SDL_GetWindowSurface(window);

  /* Start game loop */
  game_loop(window, screen_surface);

  /* We're done. Destroy the window and cleanup SDL. */
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
