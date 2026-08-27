# TAS Live Test Harness Plan

## Objective

Create an automated, scriptable QA harness that drives the actual OpenApoc executable and validates behavior through engine probes, logs, screenshots, and optional save-state digests.

The engine-side harness is a local command socket. The TAS layer should be an external runner that starts the game, sends scripted commands, waits for expected states, evaluates assertions, and writes machine-readable artifacts.

This is not a replacement for CTest. It is a slower regression layer for real UI/gameplay flows.

## Non-Goals

- Do not commit original X-COM game data.
- Do not rely on host-level mouse automation as the primary mechanism.
- Do not start with pixel-perfect rendering comparisons.
- Do not make gameplay tests depend on user configuration files.

## Phase 1: Modernize The Runtime Wrapper

Move live-test infrastructure under `live-test/`.

Modernize the Vagrant environment:

- Replace `ubuntu/trusty64` with a current Ubuntu LTS or Debian stable box.
- Use distro packages for SDL2, Qt6, Boost, Vorbis, libunwind, CMake, compiler, gettext, and xmllint.
- Install Xvfb and Mesa software rendering.
- Set `SDL_AUDIODRIVER=dummy`.
- Set `LIBGL_ALWAYS_SOFTWARE=1`.
- Mount the repository at `/vagrant`.
- Mount or symlink local original-game data from the host without committing it.
- Build with the repo default `build/` directory.
- Run `ctest --test-dir build --output-on-failure`.

The wrapper should eventually expose:

```sh
live-test/bin/build
live-test/bin/test-unit
live-test/bin/test-live tests/tas/scripts/load-cityscape.json
```

## Phase 2: Use The Engine Harness Socket

The first engine-side control layer is an opt-in command socket under `Framework.Harness`.

Configuration:

- `Framework.Harness.Enable`: enable the harness listener.
- `Framework.Harness.Port`: command socket port.
- `Framework.Harness.WarpCursor`: optionally move the OS cursor to follow injected input.

The socket protocol supports:

- `STATUS`: read current stage, display size, mouse position, port, and stage detail.
- `CONTROLS` / `CONTROL <id> ...`: enumerate and drive live named UI controls.
- `UI [filter]`: inspect the live form/control tree with resolved rectangles.
- `CLICK`, `MOVE`, `DOWN`, `UP`, `SCROLL`, `KEY`, and `TEXT`: inject raw input for nameless widgets.
- `GS <query>`: read game-state probes through the game-layer query hook.
- `SCREENSHOT <path>` and `SAVE <path>`: write useful QA artifacts.
- `RESIZE`, `HELP`, and `QUIT`.

This gives the TAS runner a practical control surface without relying on host-level mouse automation. Named `CONTROL` actions should be preferred for ordinary UI because they fail if the expected control is absent; raw input remains available for map tiles and runtime widgets without stable names.

## Phase 3: Add The TAS Runner

Add an external runner under `live-test/` rather than embedding the whole script engine in the game.

Responsibilities:

- Start OpenApoc under Xvfb/headless rendering with `Framework.Harness.Enable=1`.
- Load and validate a scenario script.
- Apply fixture config and command-line options before boot.
- Connect to the harness socket.
- Send commands, wait for expected replies/states, and enforce timeouts.
- Evaluate assertions from `STATUS`, `GS`, `UI`, screenshots, logs, and save artifacts.
- Write artifacts and final result JSON.
- Request clean shutdown when the script completes or fails.

Recommended game invocation:

```sh
xvfb-run -s "-screen 0 1280x720x24" \
  ./build/bin/OpenApoc \
  --Config.Save=false \
  --Framework.AudioBackends=null \
  --Framework.TargetFPS=60 \
  --Framework.FrameLimit=1800 \
  --Game.SkipIntro=true \
  --Game.ASyncLoading=false \
  --Framework.Harness.Enable=1 \
  --Framework.Harness.Port=17321
```

The runner command should eventually look like:

```sh
live-test run tests/tas/scripts/load-cityscape.json
```

## Phase 4: Add TAS Runner Options

Add runner-level options, not game config options:

- `--script`: path to replay script.
- `--artifacts`: output directory for run artifacts.
- `--record`: optional path for recording mode.
- `--assert-log-clean`: fail when `LogError` occurs.
- `--fail-fast`: stop on first failed assertion.
- `--harness-port`: command socket port passed to the game.

Default game behavior remains no-op unless `Framework.Harness.Enable=1`.

## Phase 5: Define The Script Format

Use JSON first. JSONL can be added later for record mode.

Script sections:

- `name`
- `fixture`
- `steps`
- `assertions`

Core actions:

- `wait`
- `wait_for_stage`
- `send`
- `control`
- `key`
- `text`
- `mouse_move`
- `mouse_down`
- `mouse_up`
- `mouse_click`
- `screenshot`
- `save`
- `quit`

Core assertions:

- `probe equals`
- `probe contains`
- `probe greater_than`
- `artifact exists`
- `screenshot nonblank`
- `log.errors == 0`

Example:

```json
{
  "name": "load-cityscape",
  "fixture": {
    "save": "tests/tas/fixtures/week1-city.zip",
    "config": {
      "Game.SkipIntro": true,
      "Game.ASyncLoading": false,
      "Config.Save": false
    }
  },
  "steps": [
    {"send": "STATUS", "until": {"field": "stage", "equals": "CityView"}, "timeout_ms": 10000},
    {"send": "GS gamestate.current_city.exists", "expect": {"equals": "true"}},
    {"send": "SCREENSHOT build/tas/load-cityscape/screenshots/city-loaded.png"}
  ],
  "assertions": [
    {"probe": "stage.name", "equals": "CityView"},
    {"probe": "gamestate.current_city.exists", "equals": true},
    {"probe": "log.errors", "equals": 0},
    {"artifact": "city-loaded.png", "nonblank": true}
  ]
}
```

## Phase 6: Stabilize Probes

Create a stable probe contract. Scripts must not know C++ object layouts. The current engine hook is `GS <query>`; the query names should become the compatibility boundary used by scenario scripts.

Initial probes:

- `stage.name`
- `stage.stack`
- `frame.number`
- `log.errors`
- `log.warnings`
- `gamestate.current_city.exists`
- `gamestate.current_city.id`
- `gamestate.current_base.name`
- `gamestate.player.balance`
- `gamestate.messages.count`
- `ui.visible_form`
- `ui.focused_control`
- `ui.control.<name>.visible`
- `screenshot.hash`
- `save.digest`

Implementation notes:

- `STATUS` already exposes stage, display, mouse, port, and stage-detail information.
- Logger should count error and warning events.
- `GameState` can expose high-level read-only values.
- `UI` and `CONTROLS` expose active forms and named controls.
- Renderer can write screenshots and compute nonblank/hash checks.
- Save digests can use existing serialization, ideally normalized to avoid timestamp noise.

## Phase 7: Add First Scripts

Start with boring tests:

1. `boot-main-menu`
   - Skip intro.
   - Wait for `MainMenu`.
   - Assert no log errors.
   - Screenshot nonblank.
   - Quit.

2. `load-cityscape`
   - Load a known save with `--Game.Load`.
   - Wait for `CityView`.
   - Assert current city exists.
   - Assert no log errors.
   - Screenshot nonblank.

3. `open-base-screen`
   - Load city save.
   - Use named `CONTROL` actions where available and raw input only where necessary.
   - Assert expected stage or form.

## Phase 8: Record Mode

Recording should capture:

- Input events.
- Frame numbers.
- Stage transitions.
- Optional probe snapshots.
- Optional screenshots at user-marked checkpoints.

Record mode should not automatically create assertions. It should produce a draft script that a developer edits.

## Risks

- Rendering can vary across OpenGL drivers. Start with Xvfb/Mesa software rendering.
- Async loading can make frame-exact scripts brittle. Disable it for early tests.
- UI coordinate scripts are fragile. Prefer named waits and stable checkpoints.
- Original game data cannot be committed. Keep fixtures as references or small legal save files only.
- Save serialization may contain ordering/timestamp noise. Normalize before digesting.

## Acceptance Criteria

- Fresh `build/` can run all CTest tests.
- `live-test` environment can build OpenApoc from repo root.
- One TAS smoke script boots to main menu under Xvfb.
- Harness writes `result.json`, log, probes, and screenshot artifacts.
- Failure output identifies the frame, failed assertion, and nearest screenshot/log context.
