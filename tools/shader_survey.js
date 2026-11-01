// Evaluated in the page by N64Recomp-browser's tools/record_shaders.mjs (tools/shader_plan.json) and play_page.mjs
// (tools/play_plan.json, tools/play_plan_extended.json, tools/play_plan_maps.json): takes the game where a plan
// needs it as the game's own code does, by writing what that code writes into RDRAM, with no patch. The addresses
// are the USA ROM's (syms/papermario.us.datasyms.toml), the fields the decomp's (include/common_structs.h,
// src/game_modes.h). It offers
//
//   mode()                     the game mode playing (CurGameMode: 2 the title, 4 the world, 5 a map change, 8 a
//                              battle, 10 the pause menu, 14 file select, 16 the intro story, 17 the attract demo)
//   map()                      the map playing, by name ("kmr_20"), or "" outside the world
//   clock()                    the game's frames (gGameStatus.frameCounter, 30 a second as the game runs), for a plan's
//                              ["clock", "clock()", 30]
//   demo(ms)                   from the title, ends its wait at once and starts the attract demo, as the title's
//                              timer running out does when the intro story played last; resolves true once it plays.
//                              It plays its seventeen scenes, five of them battles, on its own, about 3 minutes
//   demoScene()                the next scene the demo starts (gGameStatus.nextDemoScene, 0-17)
//   intro(ms)                  from the title, the intro story the same way, which the boot's Start skips; about 2
//                              minutes, then the title again
//   warp(name, entry, ms, story)
//                              from the world (a save played, or a new file's first map), goes to that map at that
//                              entrance as an exit does (the decomp's src/evt/map_api.c goto_map, then
//                              state_init_change_map): the map's area and number found by its name in gAreas, the
//                              standard fade (a new file's opening leaves the slow one of a game's start set, and a
//                              map changed through that draws its background and none of its world), the map change
//                              mode set in game mode slot 0, its fade from the map playing and the load.
//                              story is the story's progress to set first (the save's GB_StoryProgress, a signed
//                              byte, src's enums.h StoryProgress), so that the map's scripts set it up as at that
//                              point in the game; resolves true once the map plays with the player's input back.
//                              From Goombario joining on (-115) it gives Mario Goombario if he has no partner, as at
//                              that point in the game: many maps' scripts make the partner speak, and with none they
//                              read a null NPC, which crashes a console too. It also sets the game's own debug switch
//                              that lets no enemy touch Mario (gGameStatus.debugEnemyContact, DEBUG_CONTACT_CANT_TOUCH;
//                              loading a file clears it), so that a plan's walk never starts a battle nothing would end
//   battleOver(ms)             resolves true once no battle is playing (game mode neither 8 nor 9)
//   sweep(prefix, story)       warp()s to each map of an area ("kmr", its gAreas name without "area_") in turn at
//                              entrance 0 and that story, a few seconds each, and logs a line a map ("sweep: kmr_00
//                              ok ...") and one for the area (tools/play_plan_maps.json). Before each it clears what
//                              the map's exits would have: a cutscene's curtain callback (gCurtainDrawCallback, in the
//                              map's code, which the next map replaces) and its ambient sound (ClearAmbientSounds).
//                              Koopa Village's radio is stopped by its own exits only, so a sweep visits nok last
//
// A warp asked for while a map change, a battle or a cutscene's own map change is under way waits for the world.
(() => {
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  const r8 = (a) => (n64r(a & ~3) >>> (24 - 8 * (a & 3))) & 0xFF;
  const r16 = (a) => (n64r(a & ~3) >>> (16 - 8 * (a & 2))) & 0xFFFF;
  const w8 = (a, v) => {
    const shift = 24 - 8 * (a & 3), word = n64r(a & ~3);
    n64w(a & ~3, (word & ~(0xFF << shift)) | ((v & 0xFF) << shift));
  };
  const w16 = (a, v) => {
    const shift = 16 - 8 * (a & 2), word = n64r(a & ~3);
    n64w(a & ~3, (word & ~(0xFFFF << shift)) | ((v & 0xFFFF) << shift));
  };
  const str = (a) => {
    let s = '';
    for (let c; s.length < 32 && (c = r8(a + s.length)); ) s += String.fromCharCode(c);
    return s;
  };
  const GAME_STATUS = 0x80074024, CUR_GAME_MODE = 0x800A08F0, MAIN_GAME_MODE = 0x80151700, AREAS = 0x800934F0;
  const TRANSITION_ALPHA = 0x800A0940, TRANSITION_FADE_RATE = 0x800A0942, TRANSITION_STATE = 0x800A0944;
  const PLAYER_DATA = 0x8010F290, CUR_PARTNER = PLAYER_DATA + 0x12, PARTNERS = PLAYER_DATA + 0x14, GOOMBARIO = 1;
  const CURRENT_SCREEN_TRANSITION = 0x8014C6F0, TRANSITION_STANDARD = 0;
  const PLAY_INTRO_NEXT = 0x80077A34, TITLE_TIME_LEFT = 0x800A0988, STORY_PROGRESS = 0x800DACC0 + 0x10B0;
  // The map change mode's step and its UI (src/state_map_transitions.c).
  const STEP_CHANGE_MAP = 0x8003617C, DRAW_UI_CHANGE_MAP = 0x800363FC;
  // gGameStatus's fields.
  const AREA_ID = GAME_STATUS + 0x86, PREV_AREA = GAME_STATUS + 0x88, DID_AREA_CHANGE = GAME_STATUS + 0x8A,
    MAP_ID = GAME_STATUS + 0x8C, ENTRY_ID = GAME_STATUS + 0x8E, NEXT_DEMO_SCENE = GAME_STATUS + 0x72,
    STARTUP_STATE = GAME_STATUS + 0xAC, FRAME_COUNTER = GAME_STATUS + 0x134, DEBUG_ENEMY_CONTACT = GAME_STATUS + 0x74;
  const CONTACT_CANT_TOUCH = 1;
  const until = async (test, ms) => {
    const end = Date.now() + ms;
    while (!test() && Date.now() < end) await sleep(100);
    return test();
  };
  // gAreas: { mapCount, maps, "area_xxx", name } each, zero-terminated; a map's MapConfig is 0x20 bytes, its name first.
  const findMap = (name) => {
    for (let area = 0; n64r(AREAS + area * 0x10) !== 0; area++) {
      const count = n64r(AREAS + area * 0x10), maps = n64r(AREAS + area * 0x10 + 4);
      for (let map = 0; map < count; map++) {
        if (str(n64r(maps + map * 0x20)) === name) return [area, map];
      }
    }
    return null;
  };
  const findArea = (prefix) => {
    for (let area = 0; n64r(AREAS + area * 0x10) !== 0; area++) {
      if (str(n64r(AREAS + area * 0x10 + 8)) === 'area_' + prefix) return area;
    }
    return null;
  };
  window.mode =() => (r16(CUR_GAME_MODE) << 16) >> 16;
  window.map = () => {
    if (window.mode() !== 4) return '';
    const area = r16(AREA_ID), map = r16(MAP_ID);
    return str(n64r(n64r(AREAS + area * 0x10 + 4) + map * 0x20));
  };
  window.clock = () => r16(FRAME_COUNTER);
  // The title's hold (startupState 2) ends when TitleScreen_TimeLeft reaches 0, in the demo or the intro story by
  // PlayIntroNext (src/state_title_screen.c).
  const fromTitle = async (intro, modes, ms) => {
    if (!(await until(() => window.mode() === 2 && r8(STARTUP_STATE) === 2, ms))) return false;
    n64w(PLAY_INTRO_NEXT, intro);
    w16(TITLE_TIME_LEFT, 1);
    return until(() => modes.includes(window.mode()), ms);
  };
  window.demo = (ms) => fromTitle(0, [3, 17], ms);
  window.intro = (ms) => fromTitle(1, [16], ms);
  window.demoScene = () => r8(NEXT_DEMO_SCENE);
  window.warp = async (name, entry, ms, story) => {
    const ids = findMap(name);
    if (!ids) throw new Error(`no map ${name}`);
    const end = Date.now() + ms;
    // The world with the player's input on: game mode slot 0 stepping state_step_world.
    if (!(await until(() => window.mode() === 4, ms))) return false;
    if (story !== undefined) w8(STORY_PROGRESS, story);
    // gPlayerData's partner, which the next map's load creates (partner_reset_data); PartnerData is 8 bytes, enabled first.
    if (story !== undefined && story >= -115 && r8(CUR_PARTNER) === 0) {
      w8(PARTNERS + GOOMBARIO * 8, 1);
      w8(CUR_PARTNER, GOOMBARIO);
    }
    w8(DEBUG_ENEMY_CONTACT, CONTACT_CANT_TOUCH);
    w16(AREA_ID, ids[0]);
    w16(MAP_ID, ids[1]);
    w16(ENTRY_ID, entry);
    n64w(CURRENT_SCREEN_TRANSITION, TRANSITION_STANDARD);
    // state_init_change_map.
    w16(TRANSITION_ALPHA, 0);
    w16(TRANSITION_FADE_RATE, 20);
    w16(TRANSITION_STATE, 0);
    w16(DID_AREA_CHANGE, r16(PREV_AREA) !== ids[0] ? 1 : 0);
    w16(PREV_AREA, ids[0]);
    // set_game_mode_slot(0, &GameModeChangeMap): its step and UI, its first step not done yet. The step is written last,
    // so the frame that reads it finds the rest set.
    n64w(MAIN_GAME_MODE + 0x10, DRAW_UI_CHANGE_MAP);
    w16(MAIN_GAME_MODE, 3);
    w16(CUR_GAME_MODE, 5);
    n64w(MAIN_GAME_MODE + 8, STEP_CHANGE_MAP);
    return until(() => window.map() === name, Math.max(end - Date.now(), 0));
  };
  window.battleOver = (ms) => until(() => window.mode() !== 8 && window.mode() !== 9, ms);
  // AmbientSoundData { s16 flags, s16 fadeState, s32 fadeTime, s32 soundID, ... } (src/audio/ambience.c).
  const CURTAIN_DRAW_CALLBACK = 0x8009BAA0, AMBIENT_SOUNDS = 0x8015C7C0;
  window.sweep = async (prefix, story) => {
    const area = findArea(prefix), names = [];
    for (let map = 0; area !== null && map < n64r(AREAS + area * 0x10); map++) {
      names.push(str(n64r(n64r(AREAS + area * 0x10 + 4) + map * 0x20)));
    }
    const failed = [];
    for (const name of names) {
      const start = Date.now();
      n64w(CURTAIN_DRAW_CALLBACK, 0);
      // play_ambient_sounds(AMBIENT_SILENCE, 250): fade out whatever plays (fadeState AMBIENCE_STATE_FADE_OUT).
      if ((n64r(AMBIENT_SOUNDS + 8) | 0) !== -1) {
        n64w(AMBIENT_SOUNDS + 4, 250);
        n64w(AMBIENT_SOUNDS + 8, -1);
        w16(AMBIENT_SOUNDS + 2, 1);
      }
      let result;
      try { result = await window.warp(name, 0, 20000, story); } catch (e) { result = e.message; }
      await sleep(2500);
      console.log(`sweep: ${name} ${result === true ? 'ok' : 'FAIL ' + result} mode ${window.mode()} map ` +
        `${window.map() || '-'} clock ${window.clock()} ${Date.now() - start}ms`);
      if (result !== true) failed.push(name);
      // A map that left the world (a cutscene's battle) is given 10 s to come back.
      await until(() => window.mode() === 4, 10000);
    }
    console.log(`sweep: area ${prefix} ${names.length - failed.length}/${names.length} ok` +
      (failed.length ? ` failed ${failed.join(',')}` : ''));
    return names.length > 0;
  };
})()
