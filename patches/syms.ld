/* The natives the patches call: each address is a dummy N64Recomp turns into a call of the C function of that name
   (the runtime's <libultra function>_recomp, or src/game/recomp_api.cpp's). patches.h renames the libultra calls. */
__start = 0x80000000;

osRecvMesg_recomp = 0x8F000004;
osEPiStartDma_recomp = 0x8F000008;
recomp_get_aspect_ratio = 0x8F00000C;
