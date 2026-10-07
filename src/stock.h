// Fritz Dash - stock icons: ready-made 16x16 faces in sets (the SECRET ones
// are found by easter eggs) and pets with two animation frames. The pixel art
// is code in tools/icon_art.py, compiled to C by tools/gen_icons.py. Worn as
// the face from Customize (Triangle: STOCK ICONS); Look.stock is saved as
// "stock N" in custom.txt.
#ifndef FRITZ_STOCK_H
#define FRITZ_STOCK_H

#include <stdint.h>
#include "play.h"

#define STOCK_PX    16
#define STOCK_MAX   128         // stock icons the found bits have room for
#define STOCK_COL1  0x1000000   // palette entries that are the cube's own two colours
#define STOCK_COL2  0x2000000

typedef struct {
    LookOption opt;         // name, and the stars, coins or badge that unlock it
    int set;                // index into STOCK_SET_NAMES
    int secret;             // shown as "?" until an easter egg finds it
    color_t pal[8];
    const char *px;         // 256 digits row by row: '0' clear, else '1' + palette index
} StockIcon;

typedef struct {
    const char *name;
    color_t pal[8];
    const char *px[2];      // two animation frames, as StockIcon.px
} StockPet;

extern const int STOCK_COUNT, STOCK_SET_COUNT, PET_COUNT;
extern const char *const STOCK_SET_NAMES[];
extern const StockIcon STOCK[];
extern const StockPet PETS[];

// Secret icons found so far: the easter egg system calls stock_find as it
// loads its save and as eggs are found; stock_forget clears them all.
void stock_find(int k);
int stock_found(int k);
void stock_forget(void);
// Why stock icon k cannot be worn yet: STOCK_FREE when it can.
enum { STOCK_FREE, STOCK_SECRET, STOCK_STARS, STOCK_COINS, STOCK_BADGE };
int stock_lock(int k, int stars, int coins, uint32_t badges);
color_t stock_color(color_t c, color_t c1, color_t c2);
// Stock icon k, or frame f of pet p, with its top-left at (x, y), each pixel px x px.
void stock_draw(int k, int x, int y, int px, color_t c1, color_t c2);
void pet_draw(int p, int f, int x, int y, int px);

// The STOCK ICONS browser: set tabs and a grid, with a big preview.
enum { SBR_NONE, SBR_BACK, SBR_WEAR };
typedef struct {
    int set, sel;           // set tab, and the place in its grid
    char msg[48];
    int msg_frames;
} StockBrowser;

// Opens on stock icon k (the one worn), or the first set when k < 0.
void stockui_open(StockBrowser *b, int k);
// The stock icon under the cursor.
int stockui_icon(const StockBrowser *b);
// One frame; SBR_WEAR: wear stockui_icon(b).
int stockui_step(StockBrowser *b, int edges, int stars, int coins, uint32_t badges);
void stockui_draw(const StockBrowser *b, const Look *look, int stars, int coins, uint32_t badges, int frame);

#endif
