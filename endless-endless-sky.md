# Endless Endless Sky

An ever-mutating fork of Endless Sky. Whenever the player turns the game off, an agent runs on its own overnight: it reads how they played, then modifies the game to suit what they want and who they are. That means new content (stories, ships, NPCs, dialogue) and also new features and mechanics.

## The pitch

> I've been recently tinkering with Endless Sky, which is a open source remake of a childhood favorite of mine, Escape Velocity. Mostly I've just been vibing in configs so that it plays better on the Steam Deck, though I want to explore adding story arcs and mechanics like a dodge button w/ i-frames.
>
> However, a deeper idea emerged. What if, whenever I turned off the game, at night an agent ran on its own and modified the game to add new content for me? Not just new data (stories, ships, NPC's), but new features or mechanics as well? I think this could be driven by analyzing my play-style (more combat, more trading, more exploring), but also via NPC's that pop-up to ask questions about morality or philosophy. The ugly truth would be that the LLM overseeing the late-night work is motivated only to get me to play more, but I still think that could lead to some interesting results.
>
> For example, if I go the route of a trader, and start trying to buy low/sell high across the galaxy, what if the next day there's a new mechanic in which I can buy futures? Or some planets start enforcing tariffs, and I have to either adjust my trade routes, or the next day I can start to bride local officials?
>
> Or if I want to start exploring new planet, new mini games emerge to self-sustain myself without contact with civilization?
>
> An ever-mutating game. I think I will call it Endless Endless Sky.

Goals:

- FOSS and self-hosted: a repo with Dockerfiles, prompts and startup scripts.
- Bring your own model. The container ships an agent harness (e.g. opencode or Pi) that connects to whatever model the user provides.
- Doesn't require users to maintain a public GitHub repo.

## Architecture: three tiers of mutation

### Tier 1: content as a plugin (no recompile)

Endless Sky's data language covers more than people assume:

- **Missions, conversations, events, ships, outfits, systems and governments** are all plain-text data in `data/` format.
- **Personality questions are already supported.** A `conversation` with `choice` branches that `set` conditions *is* a personality test. An NPC hails you and asks the trolley problem, and your answer becomes a condition like `eesky: utilitarian` in the save file.
- **Many "mechanics" can be faked in data.** Tariffs can be events that change commodity prices or add planet fines. Bribery can be missions gated on conditions. Survival minigames can be mission chains with `on enter`, `on visit` and timer triggers.
- **Delivery is trivial.** The agent writes a folder into the user's `plugins/` directory, and `GameData::LoadSources` picks it up. It can be validated headless with `endless-sky -p` and `--parse-assets` before shipping.

### Tier 1.5: a scripting layer (the big unlock)

Embed Lua (or similar) once, by hand, with hooks like:

- `on_jump`, `on_land`, `on_trade`, `on_daily_tick`, `on_ship_damaged`, `on_conversation_choice`
- a few new UI primitives (simple panels, prompts, readouts)

After that, most "new mechanics" become plugin scripts instead of C++ patches. That means no rebuild, no binary distribution, and a much smaller blast radius when the agent writes something broken. This is the most important architectural decision in the project.

### Tier 2: real mechanics (C++ patch, then rebuild)

Futures contracts, a dodge with i-frames and new UI panels need code changes beyond what scripting can reach, which makes them the slow, risky path. The agent writes a patch, builds it, runs the unit tests, integration tests and `-p`, and only ships if everything passes.

## Analytics

### Start with the save file

Save files are human-readable text and already record credits, cargo, ships, visited systems, conditions, kills and mission history. **Diffing the save between sessions** gives a rough play-style profile with zero C++ work.

### Play data: what to record

The goal is flexibility: the data should feed features nobody has thought of yet, not just the examples in this doc. To keep the net the right size, record **decisions and outcomes, not telemetry**. A data point belongs on the list if:

1. it reflects a choice the player made or something that happened to them,
2. it's cheap to count at the moment it happens, and
3. a mission writer could plausibly say "if X, then…" about it.

Use a few generic counters keyed by a small set of dimensions, rather than many bespoke variables:

```
stat: <verb>: <object>        e.g. stat: sold tons: Food
                                   stat: destroyed: Pirate
                                   stat: landed: Earth
```

Five dimensions cover nearly everything: commodity, government, system/planet, ship category and outfit category. New kinds of features then need new *combinations* of counters, not new engine code.

### Storage: one hook, two sinks

A single helper (e.g. `Stats::Record(verb, object, amount)`) called from each hook site writes to two places:

- **Conditions** (`stat: ...`), stored in the `ConditionsStore` and saved in the save file. Missions, conversations and events can gate on them immediately, and the save-diff sees them for free.
- **A JSONL event log** in the config dir: one line per event, with in-game date, real time, system, planet and amounts. The game only appends lines; it gets no new dependency.

The nightly job loads the JSONL into **SQLite** on the server, with views for the questions the agent asks ("profit per commodity over the last 3 sessions", "eesky missions offered but never accepted"). SQLite supports plain views, which are computed at read time and cost nothing on write. It does not have *materialized* views (precomputed results); you emulate those with tables refreshed by a script or triggers. At this data size (thousands of rows per session) plain views are fast enough, so materialization isn't needed. Keeping SQLite out of the game binary also keeps the Steam Deck AppImage build simple.

### Hook-site survey

A quick pass through `source/` found that most of the list funnels through a handful of existing functions:

| Hook site | Covers |
|---|---|
| `TradingPanel::Buy` | tons bought and sold, credits, realized profit (it already tracks cost basis and `profit`) |
| `PlayerInfo::HandleEvent` | every `ShipEvent` with actor and target governments: destroy, disable, board, capture, assist, scan, `PROVOKE` (attacking a non-hostile), `ATROCITY`; flagship and escort losses |
| `PlayerInfo::SetSystem` / `Visit` | jumps, first visits |
| `PlayerInfo::Land` / `TakeOff` | landings per planet, days since last landing |
| `Mission::Do(Trigger)` | the whole mission lifecycle: offer, accept, decline, defer, complete, fail, abort |
| `Politics::Fine` | fines levied, illegal cargo or passengers caught |
| `Politics::AddReputation` | reputation changes, peaks and lows |
| `PlayerInfo::Harvest` | minerals collected |
| `ShipyardPanel` / `OutfitterPanel` | ships and outfits bought and sold |
| `ConversationPanel::Goto` | conversation choices |
| `PlayerInfo::AdvanceDate` | daily snapshots: peak credits and net worth, days broke |
| `UI::Push` / `GameLoop` | panel opens; time per panel is skipped for now (needs a panel name; Release builds have no RTTI) |

### The list

Tiers: **A** = a one-line counter at an existing hook site (do first). **B** = needs a little extra state or a new hook. **C** = later, needs content tagging. **Skipped for now** = deliberately deferred (state tracking or per-shot hot-path code for little early value). *(exists)* = already readable as a condition today.

**Trade and economy**
- A: tons bought and sold, per commodity
- A: credits spent and earned, per commodity
- A: realized profit or loss (sell price minus cost basis); best single trade, biggest loss
- A: trades per planet and per system (home markets)
- A: distinct commodities ever traded (specialist vs. generalist)
- A: times sold at a loss
- A: fines paid; illegal cargo or passengers caught
- A: ships and outfits bought and sold, by category
- A: peak and lowest credits; days spent broke (daily snapshot)
- B: illegal cargo carried and sold, per commodity
- B: mortgages and loans taken and paid off (Bank panel)
- B: money spent on fuel, repairs and crew salaries
- B: cargo jettisoned
- B: times landed with cargo but didn't trade
- B: fines dodged by leaving
- *(exists)* credits, net worth, credit score, unpaid debts, unpaid fines, outfits in storage

**Combat**
- A: ships destroyed, per government and ship category
- A: ships disabled but spared (disabled minus destroyed/captured)
- A: ships boarded, plundered or captured, per government
- A: fights started (`PROVOKE`), atrocities (`ATROCITY`)
- A: civilians and merchants attacked
- A: flagship disabled or destroyed; escorts lost, per ship category
- A: ships assisted (helped disabled strangers)
- Skipped for now: fights fled (jumped out with hostiles in range)
- Skipped for now: kills while outnumbered or at low hull; close calls (hull under 10% and survived)
- B: crew lost in boarding, yours vs. theirs
- B: bounties and contract kills completed
- Skipped for now: damage dealt and taken by type (shield, hull, ion, heat...)
- Skipped for now: kills by weapon category (beam, missile, fighter, ramming)
- *(exists)* combat rating, enemy governments, person destroyed

**Travel and exploration**
- A: total jumps; jumps per system
- A: landings per planet; first visits per in-game day
- A: days since last landed (self-sufficiency streak)
- A: minerals harvested, per mineral
- A: uninhabited planets landed on
- B: furthest distance from start or home base
- B: systems visited that no mission led you to
- B: jump drive vs. hyperdrive jumps; wormholes used
- B: ran out of fuel; refueled by an NPC
- B: landed without using any services
- B: map opened, routes plotted, distant systems inspected
- *(exists)* visited systems and planets, entered system by, previous system and planet, hyperjumps to system/planet

**Ship and fleet**
- A: flagship changes and model history
- A: outfits installed by category (weapons, engines, cargo, utility)
- A: escorts bought vs. escorts lost (are ships disposable?)
- B: ship category preference over time
- B: peak fleet size
- B: loadout rebuilds (outfitter visits that changed N+ outfits)
- *(exists)* total ships, ships by model, flagship attributes, parked ships

**Missions and story**
- A: missions offered, accepted, declined, deferred, completed, failed, aborted, per mission and per category (job board, story, `eesky`)
- A: passengers carried; passenger missions refused
- A: **generated-content usage:** for each `eesky` mission, offered vs. accepted vs. completed vs. ignored (the feedback loop)
- B: mission types favored (delivery, passenger, escort, bounty, rush)
- B: deadlines missed; days taken vs. days allowed
- B: storylines started vs. finished, per faction
- C: missions viewed but not accepted

**Social, reputation and morality**
- A: reputation change per government since last session; peak and lowest ever
- A: conversation choices, per conversation (mission name + choice index)
- B: times refused landing
- B: hails, begging for help, demanding tribute
- B: helped disabled strangers vs. robbed them
- C: lies told and other choice tags (needs conversations to tag options, e.g. `eesky: choice: merciful`)
- C: philosopher answers as trait axes (utilitarian/deontological, loyal/independent, risk-taking/cautious, curious/practical). This is content, not engine work.
- *(exists)* reputation, tribute

**Risk, failure and resilience**
- A: deaths and game-overs
- B: reloads soon after a death
- B: entered a system with hostiles present
- B: days with negative cash flow
- *(exists)* pirate attraction, raid chance

**Time and rhythm** (JSONL only; these don't need to be conditions)
- A: real-world session start, end and length
- A: real-world time of day and day of week played
- A: in-game days per session
- B: days since last trade, fight or first visit
- B: activity mix per session (trading vs. fighting vs. traveling vs. idle)

**Attention and UI**
- Skipped for now: time spent per panel (flight, map, trading, outfitter, shipyard, bank, logbook, conversation)
- B: info screens opened (ship info, player info)
- B: spaceport news read; job board browsed without accepting
- B: settings changed (autopilot, difficulty-ish preferences)

**Meta and mutation feedback**
- A: `eesky` plugin version per session
- B: per mutation: first seen, interacted with, reuse count
- B: mutations with zero engagement after N sessions (candidates to retire through an in-game event)
- B: morning broadcast viewed
- *(exists)* installed plugin

### Implemented so far

`PlayerInfo::AddStat` (conditions only), `LogStat` (JSONL only) and `RecordStat` (both) are the helpers; `StatLog` writes `stats.jsonl` in the config dir. Each JSONL line has `time` (local, with UTC offset), `pilot`, `day`, `date`, `system`, `planet`, `verb`, `object`, `amount` and any extra fields. Integration tests don't write the log.

Conditions written (each `stat: <verb>` also has a `stat: <verb>: <object>` form where an object is listed):

| Area | Conditions (`stat: ...`) | Object |
|---|---|---|
| Trade | `commodity bought`, `commodity sold` (tons); `commodity spent`, `commodity earned`, `trade profit` (credits); `sold at a loss`; `traded at`; `commodities traded` (distinct); `peak/low: trade profit` | commodity, or planet |
| Shops | `outfit bought`, `outfit sold`, `outfit bought category`, `outfit sold category`; `ship bought`, `ship sold`, `ship bought category` | outfit, model or category |
| Fines | `fined`, `fine credits`, `condemned` | government |
| Combat (by your fleet) | `destroyed`, `disabled`, `boarded`, `captured`, `assisted`, `provoked`, `atrocity`, `scanned cargo`, `scanned outfits`, plus `<verb> category` | target government, or ship category |
| Combat (against you) | `flagship destroyed/disabled/captured/boarded`, `escort destroyed/disabled/captured/boarded` | attacker government |
| Travel | `jumped`, `jump method`, `systems discovered`, `landed`, `landed uninhabited`, `planets discovered`, `last landed` (day number), `peak/low: days between landings`, `took off` | system, planet, method or flagship model |
| Collecting | `harvested`, `collected commodity`, `collected outfit` | outfit or commodity |
| Missions | `mission offered/accepted/declined/deferred/completed/failed/aborted`, `passengers delivered`, `mission cargo delivered` | where offered (job, spaceport, landing, boarding...). Per-mission counts already existed as `<mission>: offered/active/done/failed/declined/aborted`. |
| Conversations | `conversation choice`; the JSONL line also has the node, choice index and raw choice text | mission name |
| Daily | `peak/low: credits`, `peak/low: net worth`, `days unable to pay bills`; a JSONL `daily` line with credits and net worth | |
| Risk | `died` | capturer's government |
| Hails | `hailed ship`, `hailed planet` | ship's government, or planet |
| Sessions | JSONL only: `game started` (with enabled plugins and versions), `game quit` (with total play time) | |

Not done yet from tier A: reputation change per session and its peaks and lows. The save-diff covers these for now, since reputation is in the save.

### Triggers

**Implemented:** `on hail` for mission NPCs. Hailing one of the NPC's ships runs its actions (usually a conversation) instead of the stock hail panel, once. Plugins can now start a conversation from space that the player chooses to answer.

Counters let missions *check* history; triggers let them *react* when something happens. Candidates that mirror the verbs above: on buy or sell commodity, on outfit or ship purchase, on kill, on board, on capture, on flagship disabled, on jump out, on fine, on reputation threshold crossed, on new peak (credits, distance) and on conversation choice.

### Personality NPCs

Random NPC encounters that hail the player and pose moral, philosophical or personality-test style questions. Each answer sets a condition. Over time these build a trait profile that informs dialogue tone, faction behavior and what kinds of mutations get proposed.

## The nightly agent loop

```
[game exits] -> sync save + event log to the server
     |
[container] analyze -> player profile
     (play-style vector + personality traits + history of past mutations
      and whether they were used)
     |
plan 1-3 mutations -> generate plugin (and/or Lua, and/or C++ patch)
     |
gate: endless-sky -p, --parse-assets, integration tests, old save still loads
     |
commit to a local git repo (mutation history = git log; rollback = git revert)
     |
publish: plugin bundle (+ new binary only if the engine changed)
```

Design notes:

- **Close the feedback loop.** Track whether last night's mutations got used. If the player ignored the futures market, the agent should learn from that. That's what makes the game self-tuning rather than just accreting features.
- **Patch notes are diegetic.** Each morning a news broadcast or a mysterious NPC explains what changed in the galaxy. The agent writes this as part of the plugin.
- **Save compatibility is a hard constraint.** Mutations are additive. Anything removed is removed via in-game events, so old saves never break.
- **The overseer's objective is a plain-text "constitution."** Rather than hiding the "maximize engagement" motive, put it in an editable file: "maximize engagement" vs. "make the player think" vs. "be cruel but fair". It could even be a character in the game who slowly reveals it's been reshaping the galaxy around you.

## Delivering updates to the device

Simplest first:

1. **Plugin-only updates (most nights).** The container serves a plugin zip over HTTP, or exposes it via Syncthing or rsync. A tiny launcher script on the device fetches it into `plugins/` before starting the game. With Tailscale SSH to the Steam Deck this is nearly free.
2. **Engine updates.** The container builds the AppImage (or platform binary) itself, using the same build the fork's CI already runs, and the launcher swaps the binary. Local builds mean no public repo is needed.
3. **Git as the transport.** The container hosts a bare git repo (or Gitea/Forgejo). The launcher does `git pull`, which gives history and rollback for free.

Pulling the latest public build from the user's own fork is an option, but it shouldn't be required.

## Bring your own model

- The container ships an agent harness (opencode, Pi, or similar) pointed at any OpenAI-compatible endpoint or the Anthropic API.
- A prompt pack teaches the agent the data format, the conditions system and the plugin layout.
- `endless-sky -p` acts as the agent's compiler and the test suite as its guardrail.

## Repo layout

One repo for now:

- **This fork** holds the engine changes (stat hooks, triggers, later Lua) plus an `eesky/` folder for the Dockerfile, prompts, constitution, analysis scripts and device launcher. The agent checks out and builds this repo to get `endless-sky -p`. Split the orchestrator out later only if someone wants the harness without the fork.
- **Per-player mutations** are generated plugins in a git-tracked `plugins/` folder on the player's server or device. They're never pushed to the shared repo, and they don't need GitHub.

## Roadmap

- **Phase 0 (engine readiness):** make the game good at accepting data-only updates before building the agent. The `Stats::Record` helper with both sinks, the tier-A hooks from the list above, a few new mission triggers, and agent-friendly validation (every error names its file; optionally JSON output; validate one plugin against a given save). Then a hand-written "philosopher NPC" plugin that exercises it all. Done so far: stats helper and tier-A hooks, `on hail`, and `eesky/plugins/eesky-philosopher` (the Stranger's five-meeting arc and grey ship, six one-off locals, and a beggar), with conventions in `eesky/README.md`, `eesky/validate.sh` and integration tests.
- **Phase 1:** the nightly agent: save-diff and JSONL ingest into SQLite, a profile model, one generated plugin per night validated with `-p`, and the feedback loop on mutation usage. Tier-B hooks as needed.
- **Phase 2:** the Lua hook layer, which moves most mechanics to Tier 1.5.
- **Phase 3:** C++ patches by the agent, gated hard by tests, for anything scripting can't reach.

## Licensing and upstream

- Endless Sky is GPL-3. A fork distributed as FOSS is fine.
- Upstream's `docs/CONTRIBUTING.md` forbids AI-generated or AI-assisted content, so nothing the agent produces (and none of this project's AI-assisted code) can be sent upstream.
