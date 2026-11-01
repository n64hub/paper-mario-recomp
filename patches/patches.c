// Patches to the game's C go in this folder, a file each. The game needs none yet (CLAUDE.md, How it fits together):
// its fixes are instruction patches and hooks in us.toml. The library is still built and registered, since mods are
// patches over it and see its exports, and N64Recomp's file_to_c refuses an empty patches.bin, so it carries this one
// export: a mod can ask for it (RECOMP_IMPORT) to require this recompilation's patches.

#include "patches.h"

RECOMP_EXPORT u32 ssb_patches_version(void) {
    return 1;
}
