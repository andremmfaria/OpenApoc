# TAS Live Test Harness Plan

## Objective

Create a deterministic live-test harness that drives the actual OpenApoc executable through scripted input and validates behavior through read-only engine probes, logs, screenshots, and optional save-state digests.

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

## Phase 2: Add TAS Configuration Options

Add config options in a new `TAS` namespace:

- `TAS.Script`: path to replay script.
- `TAS.Artifacts`: output directory for run artifacts.
- `TAS.Record`: optional path for recording mode.
- `TAS.AssertLogClean`: fail when `LogError` occurs.
- `TAS.FailFast`: stop on first failed assertion.

Default behavior should be no-op unless `TAS.Script` or `TAS.Record` is set.

Recommended invocation:

```sh
xvfb-run -s "-screen 0 1280x720x24" \
  ./build/bin/OpenApoc \
  --Config.Save=false \
  --Framework.AudioBackends=null \
  --Framework.TargetFPS=60 \
  --Framework.FrameLimit=1800 \
  --Game.SkipIntro=true \
  --Game.ASyncLoading=false \
  --TAS.Script=tests/tas/scripts/load-cityscape.json \
  --TAS.Artifacts=build/tas/load-cityscape
```

## Phase 3: Implement The TAS Runner

Add a small `TasRunner` owned by `Framework`.

Responsibilities:

- Load and validate the script.
- Apply fixture config overrides before boot.
- On each frame, inject scheduled actions.
- Evaluate scheduled assertions.
- Write artifacts and final result JSON.
- Request clean shutdown when script completes or fails.

Integration point:

- Call the runner once per frame in `Framework::run()`.
- Inject input by creating framework `Event` objects and calling `Framework::pushEvent()`.
- Avoid using `SDL_PushEvent()` unless a specific SDL-path test requires it.

## Phase 4: Define The Script Format

Use JSON first. JSONL can be added later for record mode.

Script sections:

- `name`
- `fixture`
- `timeline`
- `assertions`

Core actions:

- `wait`
- `wait_for_stage`
- `key_down`
- `key_up`
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

## Phase 5: Add Read-Only Probes

Create a stable probe registry. Scripts must not know C++ object layouts.

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

- `Framework` can expose stage and frame probes.
- Logger should count error and warning events.
- `GameState` can expose high-level read-only values.
- Forms can expose active form and named control visibility.
- Renderer can write screenshots and compute nonblank/hash checks.
- Save digests can use existing serialization, ideally normalized to avoid timestamp noise.

## Phase 6: Add First Scripts

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
   - Use deterministic input to open a base screen.
   - Assert expected stage or form.

## Phase 7: Record Mode

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
