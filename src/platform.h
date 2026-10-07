// Fritz Dash - what the game needs from the platform: whole-file reads and
// writes and a directory listing. ps4_main.c implements it on the console,
// pc_main.c on Windows, tools/sim.c in the desktop simulator.
#ifndef FRITZ_PLATFORM_H
#define FRITZ_PLATFORM_H

// Reads up to max bytes. Returns the count, or -1 if the file cannot be read.
int plat_read_file(const char *path, char *buf, int max);
// Replaces the whole file. Returns 0 on success.
int plat_write_file(const char *path, const char *buf, int len);
// Deletes a file. Returns 0 on success.
int plat_remove_file(const char *path);
// Calls fn with the name of each entry in dir. Returns -1 if dir cannot be read.
int plat_list_dir(const char *dir, void (*fn)(const char *name, void *ctx), void *ctx);

#endif
