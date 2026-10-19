# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versioning: [SemVer](https://semver.org/).

## [Unreleased]

### Fixed
- Mouse in menus: a click no longer confirms twice when the menu closes with an animation while the button is still held (the click
  is owned by the mouse module until the button is released).
- Mouse in menus: clicking an item and letting the mouse slip onto the next one confirms the item that was clicked.
- Mouse in menus: right after the selection moves, the module waits for the highlight to catch up before confirming or correcting
  (it could open the item before the one under the mouse).
- Mouse in menus: a highlight that blinks or animates no longer makes the click confirm whatever was selected; the module waits for
  the highlight to come back (plain X only if the selection stays unknown for 12 frames, as before).
- Mouse in menus: moving the mouse inside the same item no longer restarts the navigation, and changing target never cuts a d-pad
  press in half.
- Mouse in menus: greyed-out items (e.g. Free Duel / Network Mode in the main menu early in the game) are skipped by the game's d-pad;
  the module counted them as steps, overshot the target and the highlight kept jumping up and down. Greyed-out bars are now recognised
  (by their style, and by learning which styles have ever been selected), not counted and not targeted; a click on one does nothing.
- Very short clicks (shorter than the game's 2-frame input poll) are no longer lost: the window records every button-down and each
  click stays pressed for at least 3 frames.
- The click that only brings the window to the front is no longer sent to the game.
- Mouse buttons follow the Windows "switch primary and secondary buttons" setting.
- Clean builds (ISO only, no decompiler data) crashed when the title menu loaded: the decomp swaps call `sprintf`/`strlen` by import
  name, and those names only exist in the maintainers' NID table. They are now done on the host when the import has no name.

- Release packaging: `tools/package_release.py` looked for bash.exe in the wrong MSYS2 folder, and `--no-build` deleted the exe it was
  told to reuse; `tools/build_ffmpeg_lite.sh` stopped before copying the DLLs when the project path had a space.

### Added
- Mouse in menus now also works on **cards**: in Deck Edit (the grid of card thumbnails) and in a **duel** (your hand, your monsters
  and Spell/Trap cards, the opponent's monsters, e.g. as attack targets), clicking a card moves the game's cursor there and selects it. Measured from F4 recordings: thumbnails 22x30 in a 28x34 grid with a 9-piece frame
  cursor; hand and field cards are 3D quads drawn twice (shared frame texture + art) with a 32x32 pointer on the chosen one; on the field the
  cursor changes row one step at a time (rows have gaps). Only clicks act there (hovering does not move the cursor,
  because small windows on those screens also use the d-pad); if the cursor does not move (a window is open) nothing is confirmed.
- **F4**: records what the game draws on each frame (2D sprites, text lines, 3D card quads) plus the buttons and the mouse, to
  `logs/<session>_mouse.txt`, to develop mouse support for screens that are not lists (Deck Edit, duel).
- `tools/teste_mouse/`: offline simulator and randomized test for the mouse module (no game or Windows needed).

## [0.3.0] - 2026-10-18

### Added
- **Mods** page in the launcher: turn mods on/off and change their priority order (`mods/load_order.txt`).
- **Cheats** page in the launcher; "Unlock everything" also on the F2 key.
- Window mode (windowed / fullscreen) in the launcher and in the Graphics menu.
- Quick marks on the controller, no note box: L3 = graphics glitch, R3 = performance, Start+Select = audio (saves the last 8 s as a WAV).
- Continuous monitor recording to hunt flicker (`jogar_gravando.bat`).

### Changed
- Cheats, CPU vs CPU and Monsters on the Field no longer appear in the mod list (they have their own menu).
- Smoother music/audio transition between areas.
- The mouse cursor hides after 3 s when a controller is on.
- The F8 mark shows the real FPS mode (120/60/30).

### Fixed
- 120 FPS: text letters appearing gray or sliding when a message changes; NPC head detached from the body; menu item bar bending; map character freezing or clipping when changing pose.

## [0.2.0] - 2026-10-17

### Added
- 120 FPS option (interpolation with measured pacing) and smoother 60 FPS during the whole session.
- HD textures compressed to BC7 (a quarter of the memory), with a disk cache and background loading.
- Title menu with Continue, New Game and Exit to Desktop (asks before leaving); player name typed inside the game.
- Graphics option inside the game and in the launcher, with an Original/Enhanced texture switch (F5).
- "Unlock everything" cheat: cards, packs and Free Duel duelists.
- Single problem mark F8 (or F7 / L3+R3): records graphics and CPU state with one note.
- `tools/tf.py`: plays the game by itself (buttons, captures, memory reads) for automated tests.
- Readable C++ decomp of the title, boot, story mode, map and duel result screen.
- Map of the 351 decks by duelist.

### Changed
- Menus, dialog boxes and the Duel Result screen redrawn in navy/gold in the Enhanced Textures package.
- The HD font is now part of Enhanced Textures (no option of its own).
- Only one instance per save folder.

### Fixed
- Lines and seams on the head of HD portraits; line through the middle of card art at high resolution.
- Polymerization cards disappearing; wrong deck in Free Duel (Zane got Jaden's).
- Flicker on message change and character entrance with 60/120 FPS; slowdowns on first run.
- Crash on the story map; wrong character schedule (player and director) on the map.

### Removed
- "Integer scale" and "Texture Upscaling" options (replaced by the Enhanced package).
