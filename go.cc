#include<cstdio>
#include<queue>
#include<set>
#include<utility>
#include<cstdlib>
#include<stdint.h>

#define BOARD 19
#define EMPTY 0
#define BLACK 1
#define WHITE 2
#define FOREVER for(;;)
#define TO_INDEX(x,y) ((y)*BOARD+(x))
#define ESC 27
void ANSI_cursor_move(int r, int c) { printf("%c[%d;%dH", ESC, r, c); }
uint64_t zobrist[BOARD*BOARD][2];

// Disjoint set structure. Implements union-find w/rank + path compression.
// Layout:
//   1 unremovable head for all empty stones
//   set of white chains - chains live/die together, so removal from set is not an issue.
//   set of black chains - addition to set/membership is trivial.
typedef struct Chain {
  int parent;
  int colour;
  int liberties; // empty adjacents
  Chain() : colour(EMPTY), parent(-1), liberties(0) {}
} Chain;

inline static int east_of(int x, int y) { return (x-1) >= 0 ? y*BOARD+x-1 : -1; }
inline static int west_of(int x, int y) { return x+1 < BOARD ? y*BOARD+x+1 : -1; }
inline static int north_of(int x, int y) { return y+1 < BOARD ? (y+1)*BOARD+x : -1; }
inline static int south_of(int x, int y) { return (y-1) >= 0 ? (y-1)*BOARD+x : -1; }

inline static int east_ofp(int x) { return (x%BOARD)-1 >= 0 ? x-1 : -1; }
inline static int west_ofp(int x) { return (x%BOARD)+1 < BOARD ? x+1 : -1; }
inline static int north_ofp(int x) { return (x/BOARD)+1 < BOARD ? x+BOARD : -1; }
inline static int south_ofp(int x) { return (x/BOARD)-1 >= 0 ? x-BOARD : -1; }

typedef struct GameBoard {
  uint64_t hash;
  Chain allstones[BOARD*BOARD];
  GameBoard() {
    hash = 0;
    for (int i = 0; i < BOARD; ++i) {
      for (int j = 0; j < BOARD; ++j) {
        allstones[TO_INDEX(j, i)].parent = TO_INDEX(j, i);
        allstones[TO_INDEX(j, i)].colour = EMPTY;
        allstones[TO_INDEX(j, i)].liberties = 0;
        if (east_of(j, i) >= 0) allstones[TO_INDEX(j, i)].liberties++;
        if (west_of(j, i) >= 0) allstones[TO_INDEX(j, i)].liberties++;
        if (south_of(j, i) >= 0) allstones[TO_INDEX(j, i)].liberties++;
        if (north_of(j, i) >= 0) allstones[TO_INDEX(j, i)].liberties++;
      }
    }
  }
  int Find(int pos) {
    if (allstones[pos].parent == pos) return pos;
    else return (allstones[pos].parent = Find(allstones[pos].parent));
  }
  void Remove(int pos) { // flood fill to remove.
    int colour = allstones[pos].colour;
    allstones[pos].colour = EMPTY; allstones[pos].parent = pos;
#define FLOOD_FILL(dir) \
    if (dir >= 0) { \
      allstones[dir].liberties++; \
      if (allstones[dir].colour == colour) { Remove(dir); allstones[pos].liberties++; } \
    }

    FLOOD_FILL(east_ofp(pos)); FLOOD_FILL(west_ofp(pos));
    FLOOD_FILL(south_ofp(pos)); FLOOD_FILL(north_ofp(pos));
  }
  int Connected(int pos1, int pos2) { return Find(pos1) == Find(pos2); }
  int Empty(int pos) { return allstones[Find(pos)].colour == EMPTY; }
  int UnionAdd(int pos1, int pos2) {
    int p1 = pos1, p2 = Find(pos2);
    if (p1 == p2) return p1;
    if (p1 < p2) {
      allstones[p1].liberties += allstones[p2].liberties;
      allstones[p2].liberties = 0;
      allstones[p2].parent = p1; return p1;
    } else {
      allstones[p2].liberties += allstones[p1].liberties;
      allstones[p1].liberties = 0;
      allstones[p1].parent = p2; return p2;
    }
  }
  int Union(int pos1, int pos2) { return UnionAdd(Find(pos1), pos2); }
  int Add(int x, int y, int colour) {
    int pos = TO_INDEX(x,y);
    if (allstones[pos].colour != EMPTY) { return 0; } // error, occupied cell
    int east = east_of(x,y), west = west_of(x,y);
    int south = south_of(x,y), north = north_of(x,y);
    // check for suicides.
    int save = 0;
#define CHECKED_LIBERTY_ON(dir) \
    if (dir >= 0) { \
      int dir_rep = Find(dir); \
      if (allstones[dir_rep].colour == colour) { \
        if (allstones[dir_rep].liberties == 1) { return 0; } else {save = 1;} \
      } \
    }

    if (!allstones[pos].liberties) {
      CHECKED_LIBERTY_ON(east); CHECKED_LIBERTY_ON(west);
      CHECKED_LIBERTY_ON(south); CHECKED_LIBERTY_ON(north);
      if (!save) return 0;
    }

    // merge with chains on NSEW adjacencies
#define CHECKED_UNION_ON(dir) \
    if (dir >= 0) {           \
      allstones[Find(dir)].liberties--; \
      if (allstones[dir].colour == colour) { UnionAdd(pos, dir); } \
      else if (allstones[dir].colour != EMPTY) { if (!allstones[Find(dir)].liberties) Remove(dir); } \
    }

    CHECKED_UNION_ON(east); CHECKED_UNION_ON(west);
    CHECKED_UNION_ON(south); CHECKED_UNION_ON(north);

    allstones[pos].colour = colour;
    //hash ^= zobrist[y*BOARD+x][colour-1]; //update hash
    return 1;
  }
} GameBoard;

uint64_t rand64() {
  return static_cast<uint64_t>(rand()) ^ (static_cast<uint64_t>(rand()) << 15)
                ^ (static_cast<uint64_t>(rand()) << 30)
                ^ (static_cast<uint64_t>(rand()) << 45)
                ^ (static_cast<uint64_t>(rand()) << 60);
}

void gen_zobrist() {
  for (int i = 0; i < BOARD*BOARD; ++i) {
    zobrist[i][0] = rand64();
    zobrist[i][1] = rand64();
  }
}

void make_move(int p, GameBoard& gb, std::set<uint64_t>& past) {
  char x;
  unsigned int y;
  while (1) {
    if (p == WHITE) fputs("White to move: ", stdout);
    else fputs("Black to move: ", stdout);
    if (scanf("%c%u", &x, &y) != 2) continue;
    int xx = static_cast<int>(x-'A');
    int yy = static_cast<int>(y)-1;
    if (xx < 0 || xx > 18 || yy < 0 || yy > 18) {
      fprintf(stderr, "ILLEGAL MOVE: Invalid point\n");
      continue;
    }
    //GameBoard t(gb);
    if (!gb.Add(xx, yy, p)) { fprintf(stderr, "ILLEGAL MOVE: some shit\n"); continue; }
    /*
    if (past.find(t.hash) != past.end()) {
      fprintf(stderr, "ILLEGAL MOVE: Repeated board state\n");
      continue;
    }
    past.insert(t.hash);
    gb = t;
    */
    return;
  }
}

void ANSI_goto_column(int col) { printf("%c[%dG", ESC, col); }
void ANSI_double_thick() { printf("%c#6", ESC); }
void ANSI_set_colour(int c) { printf("%c[%dm", ESC, c); }
void ANSI_clear_colour() { printf("%c[m", ESC); }
void ANSI_cursor_down(int x) { printf("%c[%dB", ESC, x); }
void ANSI_clear_screen(void) { printf("%c[2J", ESC); }

#define EMPTY_AA  43
#define WHITE_AA 107
#define BLACK_AA  40
void print_state(GameBoard& b) {
  //*
  int blacks = 0, whites = 0;
  ANSI_clear_screen();
  ANSI_cursor_move(5, 5);
  printf("   ABCDEFGHIKLMNOPQRST");
  ANSI_double_thick();
  ANSI_cursor_down(1);
  // */
  for (int i = 0; i < BOARD; ++i) {
    //*
    ANSI_goto_column(5);
    ANSI_double_thick();
    printf("%2d ", BOARD-i);
    for (int j = 0; j < BOARD; ++j) {
      int colour = EMPTY_AA;
      int pos = b.Find(TO_INDEX(j, BOARD-i-1));
      colour = b.allstones[pos].colour == BLACK ? BLACK_AA : b.allstones[pos].colour == WHITE ? WHITE_AA : EMPTY_AA;
      if (colour != EMPTY_AA) {
        if (colour == BLACK_AA) { blacks++; //fprintf(stderr, "BLACK: %d %d\n", j, BOARD-i-1);
        } else { whites++; }//fprintf(stderr, "WHITE: %d %d\n", j, BOARD-i-1); }
      }
      ANSI_set_colour(colour);
      printf(" ");
      ANSI_clear_colour();
    }
    printf(" %2d ", BOARD-i);
    ANSI_cursor_down(1);
    // */
  }
  //*
  ANSI_goto_column(5);
  printf("   ABCDEFGHIKLMNOPQRST");
  ANSI_cursor_down(1);
  printf(" B: %d W: %d", blacks, whites);
  ANSI_cursor_down(1);
  ANSI_goto_column(5);
  // */
}

int main(int argc, char** argv) {
  GameBoard b;
  gen_zobrist();
  std::set<uint64_t> past_states;
  past_states.insert(0);
  print_state(b);
  FOREVER {
    make_move(WHITE, b, past_states); print_state(b);
    make_move(BLACK, b, past_states); print_state(b);
  }
  return 0;
}
