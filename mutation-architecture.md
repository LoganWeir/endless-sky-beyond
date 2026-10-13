# Mutation architecture (draft)

Working notes on the nightly mutation side of Endless Endless Sky: how agents turn play data into new content, mechanics, and engine features. Drafting phase, no decisions are final. The overall design is in [`endless-endless-sky.md`](endless-endless-sky.md); conventions for eesky content are in [`eesky/README.md`](eesky/README.md).

## Where things stand (2026-10-11)

Built and working:

- **Play data.** `stat:` conditions plus `stats.jsonl` from the engine (see "Implemented so far" in the design doc). Mission log lines carry passengers, cargo tons, deadline (rush), payment, and visibility.
- **Pulling data.** `eesky/pull.sh` rsyncs `stats.jsonl` and the saves from the Steam Deck (`deck@steamdeck` over Tailscale) into `~/eesky-data`, then runs `eesky/summarize.py`, which prints a play summary (`--json` for agents).
- **Content.** `eesky/plugins/eesky-philosopher`: the Stranger arc, grey ship (`on hail`), six locals, and the beggar. Answers set `eesky: trait:`, `eesky: wants:`, and `eesky: answer:` conditions.
- **Engine hooks added so far:** play statistics, `on hail` for mission NPCs, and `jump flash` images (any plugin's `images/jump flash/` folder).
- **Validation.** `eesky/validate.sh` (parse with the plugins), and integration tests named `eesky ...` that force content with `eesky: debug: force ...` conditions.

Not built yet: everything below.

## What the first real data said

From 4 sessions (2.6 hours) on the Deck, pilot "hey there":

- **Mission runner.** 18 job board jobs accepted, 18 completed, 0 failed. (27 apparent failures were invisible bookkeeping missions like message buoys; these are now excluded from the counts.)
- **People and cargo about evenly:** 11 passenger jobs vs 7 cargo jobs; 57 passengers and 214 tons delivered.
- **Likes rush jobs:** 5 of 7 cargo jobs were rush deliveries.
- **Not trading, barely fighting:** no commodity trades; one death; 7 outfit scans.
- **Money isn't a motivator:** peak 2.09M credits, never below 500k.
- **Philosopher answers:** bold +3 (a risk-taker), plus curious, generous, loyal, utilitarian +1 each. Gave the beggar 1,000. Told the Stranger "that depends on who decides what 'better' means."

A good mutation agent should read this as: lean into courier and rush work, add risk and choices rather than money, and occasionally probe trading.

## Three tiers of mutation

1. **New content (data only):** stories, NPCs, missions, conversations, events. A plugin folder; no rebuild.
2. **New mechanics (plugin level):** systems faked or composed from the data language, conditions, and existing hooks (contracts, guilds, reputations, timers, fines).
3. **New systems (C++):** new hooks, screens, or rules (fleet or planet management, ship interiors, crew relationships). Needs a rebuild and a new AppImage.
4. Later, once image generation is available: new planets, ships, and weapons with art.

## Three shapes for the pipeline

**A. Assembly line.** Analyst -> Dreamer -> Engineer -> Builder -> Validator, run in sequence each night. Simple, debuggable, cheap. Risk: a single dreamer develops habits and the output gets samey.

**B. Writers' room.** Several dreamers with distinct personas pitch in parallel, a critic panel scores the pitches, and a showrunner picks 1 to 3. Personas:

- the **Economist** (markets, contracts, risk)
- the **Storyteller** (characters, arcs)
- the **Trickster** (rule-bending, surprises)
- the **Cartographer** (places, exploration)
- the **Philosopher** (dilemmas, new questions)

The persona mix can be tuned from the constitution ("more Trickster this week").

**C. Evolution.** Ship two competing mutations, let usage decide, keep the winner, and retire the loser through an in-game event. Each mutation has a lifespan and has to earn renewal through engagement. This is what makes the galaxy self-tuning instead of just accumulating features.

**Current leaning:** B to generate ideas, C to decide what survives, with A's validation gates in between.

## The cast

| Agent | Sees | Produces |
|---|---|---|
| **Analyst** | `summarize.py --json`, recent saves, the mutation ledger | A player profile with evidence, e.g. "mission runner, prefers rush cargo (5/7), money isn't a motivator (2M credits), bold +3" |
| **Dreamers** (personas) | Profile, constitution, canon, capability catalog summary | Structured pitches (schema below) |
| **Engineer** | A pitch, plus the capability catalog | A feasibility verdict: which tier, which existing hooks it uses, what is missing |
| **Builder** | An approved pitch, the repo | Plugin files, plus an `eesky ...` integration test that forces the content and asserts its conditions |
| **Playtester** | The built plugin | Runs `validate.sh`, the forced integration test headless, and loads yesterday's save. Pass or fail |
| **Herald** | What shipped tonight | The diegetic morning broadcast (an NPC or news item explaining what changed in the galaxy) |
| **Showrunner** (supervisor) | Everything, plus the constitution | Picks pitches, sets the night's budget, keeps arcs coherent, owns the ledger |

Tool access should follow the role: the Analyst and Dreamers are read-only, only the Builder writes files, and only the Playtester runs the game.

## Ideas that make it work

1. **A capability catalog instead of free reading.** The Engineer shouldn't wander the codebase every night. A script generates a catalog from the source: every mission trigger, NPC trigger, `GameAction` key, condition prefix and derived condition, `stat:` counter, and top-level data node type. A pitch is then tier-classified mechanically: everything it needs is in the catalog -> tier 1; it needs a combination the data language can fake -> tier 2; it needs something missing -> tier 3. This is deterministic and the natural next thing to build.

2. **Tier 3 produces generic hooks, not one-off features.** When a pitch needs C++, the Builder writes the smallest general engine hook that makes it possible (as `on hail` and `jump flash` were), and the content uses the hook. Over months the engine grows a vocabulary, and each new hook moves a whole class of future ideas down to tier 1. Rule of thumb: a C++ mutation should be useful to at least two hypothetical pitches.

3. **Every mutation is a hypothesis.** A pitch states what it expects, e.g. "the player takes at least 2 courier guild contracts within 5 sessions, and `stat: rush missions completed` rises." The ledger records the prediction and the next night's Analyst grades it. Mutations that fail their hypothesis are retired or reworked. This is the feedback loop, in a form an LLM can reason about.

4. **A canon document.** A shared lore file the agents read and append to: named NPCs, factions, ongoing arcs, what the Stranger has said. Without it, night 30 contradicts night 3.

5. **Seasons, not just nights.** The Showrunner plans arcs that span about a week: tonight plants a rumor, in three days a contract appears, a week later it pays off. Each night's change stays small while the story still builds.

6. **An exploration quota.** Without one, the system only feeds the player more of what they already do. The constitution sets a ratio, e.g. 70% "lean into the profile" and 30% "try something they haven't touched." This player has never traded, so a trading hook every few nights tests whether they'd enjoy it.

## How the philosopher questions factor in

- **Stated wants steer directly.** `eesky: wants: danger` shifts what the Dreamers pitch. `eesky: wants: autonomy` (from "Leave me alone") should lower the mutation rate itself, so the system respects the answer.
- **Traits set tone and dilemmas.** A bold player gets risk-reward choices in contracts. A merciful player meets disabled pirates who beg. A utilitarian gets trolley-shaped cargo decisions.
- **Revealed vs stated preferences.** The engine knows what the player does (`stat: destroyed`, `stat: assisted`) and what they said. When they diverge, the Stranger notices: "You told me mercy matters. You've left 14 disabled ships to drift." The data practically writes this mutation.
- **Ask where the profile is unsure.** The Philosopher dreamer writes new questions aimed at whatever the profile is least certain about. If it can't tell whether money motivates the player, the next stranger's dilemma tests exactly that.
- **The Stranger as the overseer's voice.** The Showrunner can speak through the Stranger: explaining changes, admitting its motives ("someone who is paid to keep you flying"), and reacting to `eesky: asked who`.

## Draft dreamer prompt

```
You are a Dreamer for Endless Endless Sky. Persona: {persona}.
Constitution: {constitution.md}
Player profile (with evidence): {analyst output}
Canon: {canon.md}
Mutation ledger (last 14 nights, with hypothesis results): {ledger}
Capability catalog summary: {catalog}

Pitch 1-2 mutations. For each, return:
  title
  tier guess (1 content, 2 plugin mechanic, 3 engine)
  the player signal it responds to (cite the stat or answer)
  hypothesis: the expected stat change, and over how many sessions
  what it costs or risks the player
  how it is retired if it fails
  a one-paragraph in-game broadcast announcing it

Rules: additive only; every mission and condition is namespaced "eesky:";
it must be forceable with an "eesky: debug: force <id>" condition; nothing
outside the constitution's limits.
```

Other prompts still to draft: the Analyst (profile with evidence and uncertainty), the Engineer (verdict against the catalog), the Builder (follow `eesky/README.md` conventions, write the forced test), and the Showrunner (selection, budget, arcs).

## Worked example: the courier and the hot jump

- **Signal:** mission runner, prefers rush cargo, bold +3, plenty of money.
- **Economist, tier 1:** a courier guild with multi-leg rush chains (pick up in system A, deliver to B and then C) and escalating pay. Missing a deadline costs guild standing, tracked as a condition. Pure data.
- **Trickster, "hot jump," needs tier 3:** a rush job that requires a dangerous jump. The Engineer's verdict would go roughly:
  - A jump-range outfit is data. A data-defined jump drive variant with extra range is mostly possible today.
  - "Arrive with shields down" needs a hook. Something generic, such as an outfit attribute `"arrival shield loss"` applied when exiting hyperspace.
  - "Republic fines you if scanned" can be faked in data: the hot jump sets a condition with a timer and Republic scans trigger a fine through a mission NPC's `on 'scan outfits'`, or the outfit is simply illegal (outfit fines already exist).
  - Verdict: one small engine hook plus content.
- **Showrunner:** ships the guild tonight, plants a rumor about "hot-jump rigs" sold by a shady outfitter, and schedules the hot jump for later in the week, once the hook has passed review.

## Guardrails

- A nightly budget: time, tokens, and at most one tier-3 change per week.
- A morning approval mode at first: proposals land somewhere reviewable (e.g. a page on the phone) and ship only once approved.
- One git commit per night in the per-player mutation repo, so rollback is a single revert.
- One plugin folder per mutation, so any mutation can be switched off in Preferences > Plugins.
- Save compatibility: never delete a mission that may be in a save; retire content by making its `to offer` impossible.

## Open questions

- Which agent harness and model setup: Claude Agent SDK, opencode, or Pi; one model for everything or a cheaper one for the Analyst?
- Where it runs: the Mac, a home server, or a container; and how the Deck gets each night's plugins (rsync at launch, or a git pull by a launcher script).
- What the constitution says, and who can edit it (only Logan, or the Stranger in-game?).
- How much of the ledger and canon to keep in context as they grow (summaries, or retrieval).
- When to move from morning approval to fully automatic.
- How to measure "used" for each kind of mutation (offered vs accepted vs completed, revisits, time spent).

## Next steps

1. Build the capability catalog generator (a deterministic script over `source/`).
2. Write `constitution.md` and an empty `canon.md`.
3. Define the pitch and ledger formats (JSON or Markdown with frontmatter).
4. Prototype the assembly line (shape A) end to end on one night: Analyst -> one Dreamer -> Engineer -> Builder -> Playtester, producing one tier-1 mission plugin from `~/eesky-data`.
5. Add personas and evolution once the single path works.
