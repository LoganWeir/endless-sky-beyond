# eesky

Content and tooling for Endless Endless Sky, the self-mutating version of this fork. The design is in [`../endless-endless-sky.md`](../endless-endless-sky.md).

## Layout

- `plugins/`: plugins written in the game's data language. Each folder is a normal Endless Sky plugin (`plugin.txt` plus `data/`).
- `validate.sh`: parses the game data plus every plugin here and exits non-zero on any error. Run it after changing a plugin.

## Installing the plugins

Link or copy a plugin folder into the `plugins/` folder inside the game's config directory:

| Platform | Config directory |
|---|---|
| macOS | `~/Library/Application Support/endless-sky/` |
| Linux, Steam Deck | `~/.local/share/endless-sky/` |

On a Mac, a symlink keeps it in sync with the repo:

```bash
ln -s "$PWD/eesky/plugins/eesky-philosopher" ~/Library/Application\ Support/endless-sky/plugins/
```

To the Steam Deck over Tailscale:

```bash
rsync -av eesky/plugins/eesky-philosopher deck@steamdeck:.local/share/endless-sky/plugins/
```

## Conventions for eesky content

Every plugin here, hand-written or generated, follows these rules so that its content can't collide with the base game or with other eesky content, and so the nightly agent can read what happened.

- **Namespace everything.** Mission names start with `eesky: `, and every condition starts with `eesky: ` (or `stat: ` for the engine's play statistics).
- **Traits are signed axes.** Each is a single condition that answers push up or down:

  | Condition | Positive | Negative |
  |---|---|---|
  | `eesky: trait: utilitarian` | outcomes matter most | principles matter most |
  | `eesky: trait: loyal` | loyal | independent |
  | `eesky: trait: bold` | bold | cautious |
  | `eesky: trait: merciful` | mercy | justice |
  | `eesky: trait: curious` | curious, philosophical | practical |
  | `eesky: trait: generous` | generous | tight-fisted |

- **Record the exact answer** as `eesky: answer: <encounter>` = the choice number (1-based), in addition to any trait changes. The stats log also records the choice text.
- **Record wants** that the player states outright as `eesky: wants: <thing>` (for example `danger`, `wealth`, `discovery`, `belonging`, `peace`, `challenge`, `strangeness`, `meaning`, `autonomy`). These are the most direct signal for what to generate next.
- **Rate-limit encounters.** Every philosopher encounter sets `eesky: philosopher: last` to `"days since epoch"` and requires at least 3 days since the last one, so they never pile up.
- **Use `non-blocking`, not `minor`,** for rare spaceport encounters. A `minor` mission is dropped whenever any other mission is offered at the same spaceport, so it would almost never appear at busy ports.
- **Provide a debug switch.** Each encounter's `to offer` accepts `has "eesky: debug: force <id>"` as an alternative to its random chance and cooldowns, and clears it in `on offer`. Integration tests use this to trigger content on demand.
- **Additive only.** Never delete or rename a mission that might be in someone's save. Retire content by making its `to offer` impossible.

## Engine features eesky content can use

These exist only in this fork, not upstream Endless Sky:

- **Play statistics** as `stat: ...` conditions (trades, combat, travel, missions, conversation choices, and more). The full list is in the design doc under "Implemented so far". For example, `branch fighter` + `"stat: destroyed" >= 10` lets a conversation react to how someone plays, and `&[stat: destroyed]` prints the number in text.
- **`on hail` NPC trigger.** In a mission's `npc` block, `on hail` runs its actions (usually a `conversation`) when the player hails one of that NPC's ships, instead of the normal hail panel. It runs once; later hails get the normal panel. Pair it with an `on encounter` `message` that tells the player to hail. See `The Grey Ship` in `plugins/eesky-philosopher/data/eesky stranger.txt`.

- **Jump flash images.** The flash shown on arriving in a system can show a random image instead of plain white, at the same strength and fade (about a second), scaled to cover the screen. It only appears when "Show hyperspace flash" is on in Preferences, which is off by default. Images come from two places, pooled across all plugins:
  - **Any image in a plugin's `images/jump flash/` folder** (including subfolders), found automatically. These load on demand: only the previous, current and next image are kept in memory, and the next one loads in the background after each jump, so a plugin can hold hundreds. See the separate `endless-sky-jump-flash` plugin repo for a ready-made one.
  - **A `"jump flash"` data node** listing sprites by name, for images that live elsewhere:

    ```
    "jump flash"
    	sprite "flash/aurora"
    ```

  Image names ending in `-`, `+`, `=`, `^` or `~` followed by digits are treated as animation frames, so name files like `photo_001.png`, not `photo-001.png`.

## Tests

Integration tests for these plugins live in `tests/integration/config/plugins/integration-tests/data/tests/tests_eesky_*.txt`. Tests whose names start with `eesky ` get the plugins in `eesky/plugins` copied into their config (see `tests/integration/RunIntegrationTest.cmake`); other tests don't, so the random encounters can't interfere with them.

```bash
ctest --preset macos-arm-integration -R '^eesky '
```
