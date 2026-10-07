// Fritz Dash - FRITZ WORLD: his levels in an order he chooses, as a path of
// nodes. Friends and family play them in that order; beating one opens the
// next. Saved in WORLD_FILE: "reached N" and one "level <file>" line per node.
#ifndef FRITZ_WORLD_H
#define FRITZ_WORLD_H

#define WORLD_FILE  "/data/fritzdash/world.txt"
#define WORLD_MAX   12
#define WORLD_NAME  24

enum { WR_NONE, WR_BACK, WR_PLAY, WR_PRACTICE };

typedef struct {
    int n, reached;                 // nodes, and how many are beaten (node `reached` is the one to play)
    char files[WORLD_MAX][WORLD_NAME];
    int sel, build;                 // the node the cursor is on; building the path (or playing it)
    int del_armed, reset_armed;
    char msg[48];
    int msg_frames;
} World;

void world_load(World *w);
void world_save(const World *w);
// Opens the screen on the node to play next.
void world_open(World *w);
// The node's level was beaten in normal mode: returns 1 if that opened the
// next node, 2 if it was the last one.
int world_beaten(World *w, int node);

struct Game;
// One frame of the world screen; WR_PLAY / WR_PRACTICE: play w->sel.
int world_step(struct Game *g, int input, int edges);
void world_draw(const struct Game *g);

#endif
