# The web page

The game plays in a browser: built with Emscripten on N64Recomp-browser's `frontend/` into `SmashBrosRecompiled.html`
(the launcher, the recomp's menus, SDL2's audio and keyboard, the ROM from Load ROM, and the saves kept in IndexedDB;
`lib/N64Recomp-browser/frontend/CMakeLists.txt`'s header says what each part does), drawn through WebGPU. The page is
built from your ROM like the desktop program, so it is never built in CI and never published with the game in it.

What is the game's own stays here: the `EMSCRIPTEN` branches of `CMakeLists.txt` that make the game a page,
`tools/build_web.sh`, the prebuilt shader list (`src/rt64_shaders.txt`, recorded by `tools/shader_plan.json`; the
list's header says how to record it again), the play plans (`tools/play_plan.json`, `tools/play_plan_extended.json`)
and the survey they share (`tools/shader_survey.js`). The rest is N64Recomp-browser's: `docs/porting.md` (WebGPU and
the shaders, prebuilt shader lists, Emscripten), `docs/playing.md` (playing and measuring a page), `docs/forks.md` and
`docs/findings.md`.

## Build it

On Linux with Docker, with the ROM at `roms/smashbrothers.us.z64`:

```bash
tools/build_web.sh                  # [cpus] [cmake arguments]
```

It builds `build/web/SmashBrosRecompiled.html` with its `.js`, `.wasm` (about 14 MB) and `.data` (about 9 MB: the
recomp's fonts and menus, nothing of the game) in `tools/docker/Dockerfile.web`, the image every N64Recomp-browser
page is built in, with ccache in `build/ccache-web`, so a rebuild after a patch takes a minute or two. The script's
header says each step. Two things differ from the desktop build:

- The patches library (`patches/`, built for MIPS) is recompiled by the native N64Recomp that `tools/recompile.sh`
  builds, not the runtime's, which Emscripten would build for the browser.
- RT64 builds its pipelines from `src/rt64_shaders.txt` (`RT64_PREBUILT_SHADER_LIST`), in sections: the boot's before
  the game starts, and a stage's the first time the stage draws.

Serve `build/web` over HTTP from localhost (any static server; the page brings its own `coi_serviceworker.js` for the
headers threads need) and open `SmashBrosRecompiled.html` in Chrome or Firefox with WebGPU, then Load ROM.

## The plans

A plan reaches character select the way a player does, by the keyboard's defaults (Enter is Start, Space A, WASD the
stick): Start through the opening and the title, then the stick and A to VS Mode and A on its Start. From there
`tools/shader_survey.js` starts each stage as the game's own menus do, with no patch: it writes what the scene reads
(the battle's players and stage in `gSCManagerTransferBattleState` and `gSCManagerSceneData`), then the next scene in
`gSCManagerSceneData.scene_curr` and the load in `sSYTaskmanStatus`, as `syTaskmanSetLoadScene` does, and the scene
manager starts it with its overlays. So a plan reaches any stage, Bonus stage or 1P Game stage without the stage
select's cursor or a fighter's portrait, and a VS battle is four computer players, so every play goes as far whatever
the browser. The survey's header has each function and address; the waits in a battle are on the game's own update
count (`dSYTaskmanUpdateCount`, 60 a second).

`tools/play_plan.json` (about 2 minutes) boots, shoots the title, the main menu, VS Mode and character select, plays
a minute of the game's time on Peach's Castle and runs the battle's clock out to its results.
`tools/play_plan_extended.json` (about 4 minutes) goes on to Mushroom Kingdom, Link's Break the Targets and the 1P
Game from Metal Mario's stage. Play them with N64Recomp-browser's `tools/play_page.mjs` (its `docs/playing.md`).

## The shader list

`src/rt64_shaders.txt` holds 390 shaders: 59 for the boot (the launcher, the opening, the title and the main menu, and
five VS battles drew in plays after the recording, added there so that every stage has them), then 38 sections
recorded in headless Chrome at 1256x645 by `tools/record_shaders.sh`, a page load a section, about 45 minutes in all:
the menus (29), the nine VS stages (92-156 each, most of them in every one, a battle's fighters, items and results
among them), the 1P Game's own four (Meta Crystal, Race to the Finish, Duel Zone, Final Destination, 37-68) and each
fighter's two Bonus Practice stages (17-38). Its header says what each section played and what it leaves out.
