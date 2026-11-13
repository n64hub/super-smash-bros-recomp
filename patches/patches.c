// Patches to the game's C go in this folder, a file each (hud.c, the battle HUD at the wide picture's sides); the
// game's fixes are instruction patches and hooks in us.toml (CLAUDE.md, How it fits together). This file carries one
// export, since mods are patches over the library and see its exports: a mod can ask for it (RECOMP_IMPORT) to
// require this recompilation's patches.

#include "patches.h"

RECOMP_EXPORT u32 ssb_patches_version(void) {
    return 1;
}
