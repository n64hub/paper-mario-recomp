// Evaluated in the page by N64Recomp-browser's tools/record_shaders.mjs (tools/shader_plan.json) and play_page.mjs
// (tools/play_plan.json, tools/play_plan_extended.json): starts a scene as the game's own menus do, by writing what the
// scene reads into RDRAM and then the scene manager's next scene, with no patch. The addresses are the USA ROM's
// (syms/smashbrothers.us.datasyms.toml), the fields the decomp's (src/sc/sctypes.h). It offers
//
//   scene()                         the scene playing (gSCManagerSceneData.scene_curr: 1 the title, 7 the main menu,
//                                   9 VS mode, 16 VS character select, 22 a VS battle, 24 its results, 52 the 1P Game,
//                                   53 a Bonus stage)
//   untilScene(scene, ms)           resolves true once that scene plays, false after ms
//   stage()                         the stage of the battle playing (the battle state's gkind: 0-8 the VS stages, 13 Meta
//                                   Crystal, 14 Duel Zone, 15 Race to the Finish, 16 Final Destination, 17 + fighter
//                                   Break the Targets, 29 + fighter Board the Platforms)
//   clock()                         the game's updates since the scene started (dSYTaskmanUpdateCount, 60 a second), for
//                                   a plan's ["clock", "clock()", 60]
//   vs(stage, fighters, ms, rules)  a time battle of four computer players (fighters: four of 0 Mario, 1 Fox, 2 Donkey
//                                   Kong, 3 Samus, 4 Luigi, 5 Link, 6 Yoshi, 7 Captain Falcon, 8 Kirby, 9 Pikachu,
//                                   10 Jigglypuff, 11 Ness) on that stage, items on, as character select and stage
//                                   select start one; resolves true once it plays. rules: { minutes, teams, humans:
//                                   the ports (0-3) played by their controllers in place of the computer }
//   go(scene, prev, ms)             starts that scene as if from prev (a menu that reads where it came from), and
//                                   resolves true once it plays: a menu, from another menu
//   timeUp()                        ends the VS battle playing as its clock running out does: its results follow,
//                                   after Sudden Death when it ends in a tie (the computer players end that too, in
//                                   a few seconds to half a minute), so a plan waits for them with untilScene(24, ms)
//   quit(ms)                        ends the scene playing as its own end does, and resolves true once another plays (a
//                                   Bonus stage goes back to its character select)
//   bonus(kind, fighter, ms)        Bonus Practice 1 (Break the Targets) or 2 (Board the Platforms) for that fighter,
//                                   played by port 1, as its character select starts it; resolves true once it plays
//   spgame(stage, fighter, ms)      the 1P Game from that stage of it (the decomp's nSC1PGameStage: 0 Link, 1 the
//                                   Yoshi Team, 2 Fox, 4 the Mario Bros., 5 Pikachu, 6 Giant Donkey Kong, 8 the Kirby
//                                   Team, 9 Samus, 10 Metal Mario, 11 Race to the Finish, 12 the Fighting Polygon
//                                   Team, 13 Master Hand, 14-17 the challengers Luigi, Ness, Jigglypuff and Captain
//                                   Falcon), port 1 playing that fighter, as its character select starts it; resolves
//                                   true once its battle is on that stage. The 1P Game runs its stages, intros and
//                                   results itself (sc1PManagerUpdateScene), with the scene manager's scene_curr left
//                                   at what it starts after a game over (1)
//   training(stage, man, com, ms)   Training Mode on that stage, port 1 playing man against the computer's com, as
//                                   its character select and stage select start it (they set the fighters, without
//                                   which it starts none); resolves true once it plays
//   boot(), setBoot(n), errors()    the save's boot count, which the title adds one to at each power-on, and its
//                                   anti-piracy penalty bits (gSCManagerBackupData's boot and error_flags, the
//                                   decomp's LBBACKUP_ERROR_*): the checks us.toml answers run on entering VS Mode's
//                                   menu once boot is over 21, the 1P menu over 42, a VS battle over 68 and a 1P Game
//                                   stage over 92, and a check that fails sets its bit
//   motor(port)                     the port's Rumble Pak as osMotorInit left it (sSYControllerMotorPfs[port].status:
//                                   8, PFS_MOTOR_INITIALIZED, once the game has a motor there to start)
//
// A scene is started from any other: the one playing ends at its next update once sSYTaskmanStatus says to load the
// next (syTaskmanSetLoadScene), and the scene manager starts gSCManagerSceneData.scene_curr with its overlays. Ask from
// a menu, not from a battle: a VS battle sets its results as the next scene as it ends.
(() => {
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  const r8 = (a) => (n64r(a & ~3) >>> (24 - 8 * (a & 3))) & 0xFF;
  const w8 = (a, v) => {
    const shift = 24 - 8 * (a & 3), word = n64r(a & ~3);
    n64w(a & ~3, (word & ~(0xFF << shift)) | ((v & 0xFF) << shift));
  };
  const SCENE = 0x800A4AD0, PREV = 0x800A4AD1, GKIND = 0x800A4ADF, PLAYER = 0x800A4AE3, FKIND = 0x800A4AE4,
    COSTUME = 0x800A4AE5, SPGAME_STAGE = 0x800A4AE7, BONUS_FKIND = 0x800A4B09, BONUS_COSTUME = 0x800A4B0A,
    TRAINING_MAN = 0x800A4B0B;
  const TRANSFER = 0x800A4D08, SPGAME_STATE = 0x800A4B18, BATTLE_STATE = 0x800A50E8;
  const STATUS = 0x800465D0, UPDATES = 0x8003B6E4, BACKUP = 0x800A44E0, MOTOR_PFS = 0x800452C8;
  const until = async (test, ms) => {
    const end = Date.now() + ms;
    while (!test() && Date.now() < end) await sleep(100);
    return test();
  };
  const start = (scene, prev) => {
    w8(PREV, prev);
    w8(SCENE, scene);
    n64w(STATUS, 1);
  };
  window.scene = () => r8(SCENE);
  window.untilScene = (scene, ms) => until(() => window.scene() === scene, ms);
  window.stage = () => {
    const state = n64r(BATTLE_STATE);
    return (state >= 0x80000000 && state < 0x80800000) ? r8(state + 1) : -1;
  };
  window.clock = () => n64r(UPDATES) | 0;
  // rules: { minutes, teams: [four of 0 red, 1 blue, 2 green], humans: [ports] } for another time limit than 3, a team
  // battle, or ports their controllers play (HMN on character select: the port's own color and tag).
  window.vs = async (stage, fighters, ms, rules = {}) => {
    const t = TRANSFER;
    w8(t + 0x02, rules.teams ? 1 : 0);  // is_team_battle
    w8(t + 0x03, 1);            // game_rules: time
    w8(t + 0x06, rules.minutes || 3);  // time_limit, minutes
    const humans = rules.humans || [];
    for (let i = 0; i < 4; i++) {
      const p = t + 0x20 + 0x74 * i, man = humans.includes(i);
      w8(p + 0x00, 9);          // CPU level
      w8(p + 0x02, man ? 0 : 1);  // pkind: the port's controller, or the computer
      w8(p + 0x03, fighters[i]);
      w8(p + 0x04, rules.teams ? rules.teams[i] : 0);  // team
      w8(p + 0x05, i);          // player
      w8(p + 0x06, 0);          // costume
      w8(p + 0x07, 0);          // shade
      w8(p + 0x08, man ? i : 4);  // color: the port's, or a computer's
      w8(p + 0x09, 1);          // is_single_stockicon, for a time battle
      w8(p + 0x0A, man ? i : 4);  // tag: P1-P4, or CP
    }
    w8(t + 0x04, humans.length);      // pl_count
    w8(t + 0x05, 4 - humans.length);  // cp_count
    w8(GKIND, stage);
    start(22, 16);
    return until(() => window.scene() === 22 && r8(TRANSFER + 1) === stage && n64r(BATTLE_STATE) === TRANSFER, ms);
  };
  window.go = (scene, prev, ms) => { start(scene, prev); return window.untilScene(scene, ms); };
  window.timeUp = () => { n64w(TRANSFER + 0x14, 2); return true; };
  window.quit = (ms) => {
    const before = window.scene();
    n64w(STATUS, 1);
    return until(() => window.scene() !== before, ms);
  };
  window.bonus = async (kind, fighter, ms) => {
    w8(PLAYER, 0);
    w8(BONUS_FKIND, fighter);
    w8(BONUS_COSTUME, 0);
    start(53, kind === 1 ? 19 : 20);
    const stage = (kind === 1 ? 17 : 29) + fighter;
    return until(() => window.scene() === 53 && window.stage() === stage, ms);
  };
  window.spgame = async (spgameStage, fighter, ms) => {
    w8(PLAYER, 0);
    w8(FKIND, fighter);
    w8(COSTUME, 0);
    w8(SPGAME_STAGE, spgameStage);
    start(52, 17);
    // Its stages' grounds (sc1pgame.c's table): the VS stages' 0-8, Small Yoshi's Island 12, Meta Crystal 13, Duel
    // Zone 14, Race to the Finish 15, Final Destination 16.
    const stage = { 0: 4, 1: 12, 2: 1, 4: 0, 5: 7, 6: 2, 8: 6, 9: 3, 10: 13, 11: 15, 12: 14, 13: 16, 14: 0, 15: 6,
      16: 7, 17: 3 }[spgameStage];
    return until(() => n64r(BATTLE_STATE) === SPGAME_STATE && window.stage() === stage, ms);
  };
  window.training = async (stage, man, com, ms) => {
    w8(PLAYER, 0);
    w8(TRAINING_MAN, man);
    w8(TRAINING_MAN + 1, 0);    // costume
    w8(TRAINING_MAN + 2, com);
    w8(TRAINING_MAN + 3, 1);    // costume
    w8(GKIND, stage);
    start(54, 21);
    return until(() => window.scene() === 54 && window.stage() === stage, ms);
  };
  window.boot = () => r8(BACKUP + 0x5E3);
  window.setBoot = (n) => { w8(BACKUP + 0x5E3, n); return window.boot() === n; };
  window.errors = () => r8(BACKUP + 0x5E2);
  window.motor = (port) => n64r(MOTOR_PFS + 0x68 * port);
  return `scene ${window.scene()}`;
})()
