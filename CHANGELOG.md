# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versioning: [SemVer](https://semver.org/).

## [Unreleased]

### Fixed
- **Deck Edit hang** (also in 0.3.0): after moving a card between the deck and the Trunk, the message "The card will be moved to Trunk."
  never closed and the screen stopped responding. The rewritten BOOT window functions (`decomp/boot`) are the cause (the game goes on with
  `TF_DECOMP=off` and with `TF_DECOMP_SKIP=BOOT`), so they are off by default and the game's own translated code handles windows;
  `TF_DECOMP_BOOT=on` turns them back on for whoever fixes them.
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
- Duel: the small options window that opens over the cards (Activate / Attack / ...) is now recognised as a menu (its text lines
  overlap: step 15 for 17 px of text, so it was missed). Clicking an option used to be taken as a click on the Spell/Trap zone
  under the window, and the module pressed the d-pad, moving the window's selection. Now hovering highlights the option and
  clicking confirms that option. Tight lines only count as a menu when a highlight bar marks exactly one of them.
- Duel: your **deck** (the pointer starts on it at the beginning of your turn, x=448 y=175), your **Graveyard** right above it
  (x=450 y=161), your **banished** zone above that (x=439 y=129) and the opponent's pile (x=208 y=61) are known places: the mouse was stuck at the start of every turn until a key moved the cursor off the deck.
- Duel: the 2-option **Attack** box (over the opponent's monster row) is recognised as a menu too (2 lines count when a highlight marks
  one of them); clicking "Attack" used to be taken as a click on the opponent's zone under it. A text menu whose highlight blinks
  (confirming) stays a menu for up to 12 frames while its lines are still on screen.
- Duel: the **Set / Activate** window of a Spell/Trap card is recognised (clicking "Set" used to move the cursor down to the zone under
  the window). Every column of aligned text lines is now tried, largest first, and the first one that is really a menu wins; before,
  only the largest was tried, and the card text on the left (3 lines, no highlight) hid the 2-option window.
- Duel: leaving an isolated place (e.g. the banished zone) when up/down does not move the cursor: the mouse steps sideways toward the
  target's side of the screen (a detour allowed to change row), and hovering now also tries the other direction before giving up.
- **Faster**: the waits between d-pad steps follow the game's measured reaction (pointer/highlight moves 4-6 frames after a press, from
  the F4 recordings) instead of fixed long waits: about 35% faster on the duel field and 30% faster in menus, same precision. The wait
  is adaptive: it only stays short once the cursor has shown every step that was sent, otherwise it waits as long as before, so a
  slower screen never gets an extra press (no overshoot), even with twice the measured delay.
- Duel: the **Yes/No** window (e.g. "activate this trap?") is a menu: two 60x24 buttons side by side, the chosen one drawn with extra
  layers on the same rectangle.
- Duel: choosing several cards in a row (e.g. **tributes**) works by hovering: the hover pause after confirming (X) now lasts half a
  second (time for a window to appear and become a menu) instead of until the pointer moved, which never happened while the pointer
  stayed on the first chosen monster. Safety for windows the mouse does not recognise: two d-pad attempts toward cards with no cursor
  movement (checked independently of the target, even while the mouse crosses several cards) pause hovering.
- **Deck Edit tabs**: clicking Deck, Fusion, Side or Trunk switches to that tab. Measured: four 3D tabs at the top, the active one drawn
  2 px higher; Square moves to the next one in the order Deck -> Fusion -> Side -> Trunk -> Deck. The mouse presses Square as many times as
  needed and checks the active tab after each press.
- **Deck Edit hover**: hovering a card in the deck or Trunk grid moves the cursor to it, like in the main menu (the grid cards had no
  reference point for hover memory); same pauses as in duels while a window that is not a menu may be using the d-pad.
- **Rock-paper-scissors** before a duel works like a menu: three 80x116 hands (step 90), the chosen one rises 20 px with a 9-piece
  frame of the same texture. The icon-row detection (also used by phase selection) now allows the chosen icon to be raised, a frame
  sharing the icons' texture, rows of 3 big icons and steps up to 120; the clickable band is anchored on the icons that stay still.
- **Story Mode school map**: hovering a place moves the map cursor there (like the main menu) and clicking enters it. Places are 16x16
  quads drawn twice; the cursor is a 16x16 arrow bobbing above the chosen place. The d-pad goes to the neighbour in that direction
  (not a grid), so the mouse steps toward the target one place at a time, with detours when a neighbour leads away.
- Hover remembers where the mouse stopped for 30 frames: stopping on a target while the game's cursor is hidden (moving) still works.
- Duel **phase selection** (Draw ... End): recognised as a horizontal menu (six 20x32 icons, step 40, with a 9-piece highlight
  frame). Hovering highlights a phase and clicking selects it; unavailable phases (drawn from another part of the texture) are
  skipped like the game does and are not targets.
- Very short clicks (shorter than the game's 2-frame input poll) are no longer lost: the window records every button-down and each
  click stays pressed for at least 3 frames.
- The click that only brings the window to the front is no longer sent to the game.
- Mouse buttons follow the Windows "switch primary and secondary buttons" setting.
- Clean builds (ISO only, no decompiler data) crashed when the title menu loaded: the decomp swaps call `sprintf`/`strlen` by import
  name, and those names only exist in the maintainers' NID table. They are now done on the host when the import has no name.

- Release packaging: `tools/package_release.py` looked for bash.exe in the wrong MSYS2 folder, and `--no-build` deleted the exe it was
  told to reuse; `tools/build_ffmpeg_lite.sh` stopped before copying the DLLs when the project path had a space.

### Added
- **Mouse in the card shop**: the booster grid (12 x 4 slots, 22 x 47 px, a 2x8 divider on top of each slot; the cursor is a pulsing
  4-corner frame of the same texture) is recognized and navigated like the Deck Edit grid (click a pack, or hover). The grid has no holes,
  so a move to another row and column sends the vertical part right after the horizontal one and checks once at the end (~8% faster).
  New test `tools/teste_mouse/loja.cpp` (in rodar.sh).

### Changed
- Lists and option windows: an option under a pointer that stopped moving less than ~20 frames ago is selected as soon as the window
  appears (before, the pointer had to move again). Moving the pointer keeps the earlier behaviour.
- Tests can print timings (TF_TEMPO=1) to measure each change.
- **Faster mouse in menus, option windows and the duel** (measured on an F4 recording): once the game has drawn the highlight/cursor at
  the expected place for every press sent, the mouse goes on at once instead of waiting its safety margin. Menus and option windows:
  each step/confirmation ~4 frames sooner (10 -> ~6 frames per item); duel hand and field: 3 -> 1 frame of margin per step. The long wait
  stays when the game has not answered yet (slow menus), and the Deck Edit and the map keep the earlier pace. Deck Edit: a multi-row step
  now waits for every row to show before deciding (checking at the first row could send one step too many).

### Added
- **American version gets the European title menu** (Graphics in the main menu, Exit to Desktop on the title screen, the reworked
  menus): the libtitle swaps are compiled per version with that version's addresses. `tools/portar_mods.py` also ports every address the
  decomp modules use; `tools/gerar_portes.py <VER>` turns the table into `gen/portes/<VER>/portes_decomp.h` (run by build_run.sh and
  package_release.py) and turns a module on only when all its addresses were found with coherent shifts; `decomp/port.h` gives the
  constexpr `PORT(eu_address)` (identity in the European build, so nothing changes there). On other versions only the ported modules run.
  For the decomp modules only, a European function only ports to a callable address of the SAME module in the other version
  (several PSP modules share one load address, so other modules are never searched; mods keep the earlier, working rule) (the recompiler's <module>_fns[]
  tables in gen[/<VER>]/*.c: function starts and extra entries), and swaps whose address is not callable there are not installed, and on other versions a swap's call to an address with no function is skipped and logged
  instead of closing the game.
- **Mods on the American version**: plugin API 8 adds `game_id` and `port(eu_address)`, which translates a European address to the version
  running using `mods/enderecos_<ID>.txt` (or `tools/` in the project); unported addresses give 0 and `hook(0)`, `call(0)` and
  `mem(< 0x08000000)` are safe no-ops. Better AI, Better Decks, Booster box, Cheats, CPU vs CPU and Monsters on the field now write their
  addresses through `port()` and accept ULUS-10136. The release package carries the tables found in `tools/`.
- Loading icon hiding works on other versions when `tools/portar_mods.py` ports its two BOOT addresses (`port_eu`); otherwise the icon
  stays, as before. 'Similar code' is also accepted when the best match is far ahead of the second (>= 65% and 35 points).
- `tools/portar_mods.py`: ports the European addresses the mods use (hooks, calls, data) to the American version (ULUS-10136), by
  matching the code of both installed versions (instruction shapes with addresses masked; data through the code that builds the address,
  majority vote; functions that changed a little are found near the place predicted by their ported neighbours, with an aligned
  comparison; far struct fields need every reference to agree; a function start is never ported as data, and a 'similar code'
  result must agree with the shift of its ported neighbours). Writes `tools/enderecos_ULUS10136.txt`.
- The release package also contains `run_ULUS10136.exe`, the same generic runtime built for the **American version (ULUS-10136)**: picking
  a US ISO in TagForceRecomp.exe hands the game over to it (gameid.cpp), and it installs the game with `tools/modules_ULUS10136.tsv`.
  The mouse, Game Speed and HD Textures work on it (they do not depend on game addresses); the other mods stay European-only.
  `--no-us` leaves it out.
- **HD Textures** slot (`mods_src/TexturasHD`, shown on the launcher's Mods page): an empty mod with a `textures` folder and a LEIA-ME;
  extracting a PPSSPP-format pack there (US-version packs work too: textures are matched by content) makes the game use it. No pack is
  bundled (packs belong to their authors).
- New mod **Game Speed 1.3x** (`mods_src/AceleraJogo`, 4th entry on the launcher's Mods page): runs the whole game 1.3x faster while the
  music and sound effects keep their normal speed and pitch; the speed (100-300%) and "Music at normal speed" are in the game window's
  Mods menu. API version 7 adds `set_music_normal(on)`: the game's sound threads (outputs that wait for each block) wait the block's real
  duration, so they produce at the device's pace and play uncompressed. Plugin API version 6 adds `set_speed(percent)`: the frame limiter's step
  becomes 1/(59.94 x speed) s (monitor sync off outside 1x) and audio blocks are compressed in the same ratio so the buffer neither
  drains nor overflows. Comes switched off in the release package.
- Mouse in menus now also works on **cards**: in Deck Edit (the grid of card thumbnails) and in a **duel** (your hand, your monsters
  and Spell/Trap cards, the opponent's monsters, e.g. as attack targets), clicking a card moves the game's cursor there and selects it. Measured from F4 recordings: thumbnails 22x30 in a 28x34 grid with a 9-piece frame
  cursor; hand and field cards are 3D quads drawn twice (shared frame texture + art) with a 32x32 pointer on the chosen one; on the field the
  cursor changes row one step at a time (rows have gaps). In Deck Edit hovering moves the cursor too (deck and Trunk grids, like the main menu); if the cursor does not move (a window is open) nothing is confirmed.
- In a duel, **hovering** a card moves the game's cursor to it (like the d-pad), with the game's sounds. Hovering pauses itself while a
  small window may be open (after confirming with X, or when the cursor does not move) and comes back when the pointer moves or
  reappears, or with O.
- Duel field: your **empty** Monster and Spell/Trap zones are targets too (hover or click), so the cursor follows the mouse over the
  whole field, not only over cards. Pointer positions measured from F4 recordings (monster zones y=145, x 226-401 step ~44;
  Spell/Trap zones y=181, x 218-409 step ~47, both centred on x=314). The F4 recording is flushed to disk every second.
- Duel field: the opponent's Monster zones (y=99, x 236-391 step ~39) and Spell/Trap zones (y=75, x 242-385 step ~36) are targets
  too, empty or not. On the field the cursor moves one step at a time and checks it got closer: when choosing an attack target the
  game skips empty zones, so a zone it cannot reach is marked and skipped (without pausing the rest) until the cursor moves.
- **F4** off also writes the game's thread state (what each thread waits for) and the last recompiled functions to the session log,
  to investigate screens that stop responding but keep animating.
- **F4**: records what the game draws on each frame (2D sprites, text lines, 3D card quads) plus the buttons, every mouse movement
  (M) and what the mouse module understood (U), to `logs/<session>_mouse.txt`, to develop mouse support for screens that are not lists (Deck Edit, duel).
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
