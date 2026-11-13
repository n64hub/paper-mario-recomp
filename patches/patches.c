// Patches to the game's C go in this folder, a file each (widescreen.c, hud.c, extended_gbi.c); the fixes the recompiled code
// needs to run are instruction patches and hooks in us.toml. This one export lets a mod ask for this recompilation's
// patches (RECOMP_IMPORT) to require them.

#include "patches.h"

RECOMP_EXPORT u32 pm_patches_version(void) {
    return 1;
}
