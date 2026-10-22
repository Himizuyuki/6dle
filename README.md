# 6dle

Wordle with six letter words, written entirely in C, with a GTK interface and
a **neural network solver** you can watch play, one guess at a time.

| Play | Solver |
| --- | --- |
| ![Playing a game](docs/play.png) | ![The solver after three guesses](docs/solver.png) |

- **Play** in a GTK window (or in the terminal, the original interface).
- **Watch the solver**: press *Step* to make it guess. Next to the board it
  shows how many words are still possible and its **5 most likely next
  guesses with their probability in %**.
- The solver is a **feed-forward neural network** trained by imitation of an
  information-theory (entropy) solver. It wins **98.9%** of games in
  **3.86 guesses** on average, the same as the expert it learned from.
- Everything is C11: the game, the GUI, the neural network (forward pass,
  backpropagation, Adam), the training tool and the tests. The only library
  is GTK 3.
- `make test` runs 35 unit tests under AddressSanitizer + UBSan, plus an
  end-to-end train-and-solve run.

---

## Contents

1. [Quick start](#quick-start)
2. [Rules](#rules)
3. [Using the GUI](#using-the-gui)
4. [Terminal version](#terminal-version)
5. [Make targets](#make-targets)
6. [Project layout](#project-layout)
7. [How the code fits together](#how-the-code-fits-together)
8. [The neural network solver](#the-neural-network-solver)
9. [Training](#training)
10. [Results](#results)
11. [Tests](#tests)
12. [Changes from the original version](#changes-from-the-original-version)
13. [Troubleshooting](#troubleshooting)
14. [Ideas for later](#ideas-for-later)
15. [Credits](#credits)

---

## Quick start

### Requirements

| What | Package (Debian / Ubuntu / WSL) | Needed for |
| --- | --- | --- |
| C compiler, make | `build-essential` | everything |
| pkg-config | `pkg-config` | finding GTK |
| GTK 3 headers | `libgtk-3-dev` | the GUI only |
| clang-format *(optional)* | `clang-format` | `make format` |

```bash
sudo apt install build-essential pkg-config libgtk-3-dev
```

On **WSL**, GUI apps work out of the box on Windows 11 (WSLg). The terminal
version, the trainer and the tests don't need GTK or a display.

### Build and run

```bash
make            # builds build/6dle, build/6dle-term and build/6dle-train
make run        # GTK game
make run-term   # terminal game
make test       # unit tests + end-to-end check
```

A trained model ships in `models/solver.nn`, so the solver works right away.
`make train` rebuilds it from scratch (about 4 minutes on 12 threads).

The binaries locate `data/wordBank.txt` and `models/solver.nn` through absolute
paths compiled in by the Makefile, so they run from any directory. If you move
the repository, run `make clean && make`, or pass `--words` / `--model`.

---

## Rules

- Find the hidden **6 letter** word in **7 guesses**.
- Every guess must be a word from the word list and can't be repeated.
- After each guess, every tile is coloured:
  - **green**: right letter, right place;
  - **yellow**: the letter is in the word, but somewhere else;
  - **grey**: the letter is not in the word, or there are no more copies of it.
- Repeated letters follow the standard Wordle rules. Greens are assigned
  first, then yellows from left to right, and each letter of the secret
  colours at most one tile. For example, guessing `TEETER` against `PLANET`
  gives `T` yellow, `E` (5th) green, and the other `E`s, the second `T` and
  `R` grey.
- The on-screen keyboard shows the best colour known for each letter. A
  letter that has been green stays green.

The secret word is drawn **uniformly from the whole word list**
(`data/wordBank.txt`, 22,159 words). The same list is the dictionary of
accepted guesses. There is no separate "solutions" list any more, so any
valid word can be the answer, including rare ones.

---

## Using the GUI

Launch with `make run` or `./build/6dle [--words FILE] [--model FILE]`.
Switch modes with the **Play / Solver** switcher in the title bar.

### Play tab

| Action | How |
| --- | --- |
| Type a letter | physical keyboard or the on-screen keys |
| Delete a letter | `Backspace` or `⌫` |
| Submit the guess | `Enter` or `ENTER` |
| New game | **New game** button, or `Enter` once a game is over |

The line above the board shows guesses left and errors: *Not enough letters*,
*Not in word list*, *Already guessed* (the row is also outlined in red).
At the end it shows *Congrats…* or the secret word. Rejected words never
use up a guess.

### Solver tab

The network plays a secret word while you watch.

| Control | What it does |
| --- | --- |
| **Step ▶** | the network makes its next guess (one guess per click) |
| **New word** | starts over with a new random secret |
| text box | type a word, then press **Enter** (or **New word**) to make the solver look for that particular word |

The panel on the right shows:

- **Secret word**: the word being solved. You are a spectator here, so it is
  visible.
- **Words still possible**: how many words of the list are still consistent
  with all the colours so far. Secrets are drawn uniformly, so each of them is
  equally likely to be the answer, and the panel gives that chance
  (`1 / count`).
- **Most likely next guesses**: the network's top 5 candidates with the
  probability it assigns to each one being its next guess (a softmax over
  all still-possible words, so the full distribution sums to 100%). The
  first one is what **Step** plays.
- To the right of each played row: how many words were still possible after
  that guess (`612 left`, `18 left`, … `solved!`).

---

## Terminal version

The original box-drawing interface, rebuilt on the same game engine.

```bash
./build/6dle-term                   # play (letters, Backspace, Enter)
./build/6dle-term --solve           # watch the solver on a random word
./build/6dle-term --solve puzzle    # ... or on a chosen word
./build/6dle-term --words FILE --model FILE
```

`--solve` prints every step with the top 5 guesses and their probabilities:

```
Secret word: puzzle

22159 possible words. Top guesses: tories 12.1% railes 9.9% caries 9.4% ...
  guess 1: TORIES
612 possible words. Top guesses: decane 19.3% candle 15.6% dangle 7.1% ...
  guess 2: DECANE
18 possible words. Top guesses: fumble 72.4% jumble 10.8% bumble 5.4% ...
  guess 3: FUMBLE
6 possible words. Top guesses: guzzle 53.5% puggle 37.1% puzzle 4.6% ...
  guess 4: GUZZLE
2 possible words. Top guesses: puzzle 54.0% wuzzle 46.0%
  guess 5: PUZZLE

Found puzzle in 5 guesses
```

(Guesses are coloured in a real terminal.) The exit status is 0 if the word
was found, so `--solve` is also handy in scripts.

---

## Make targets

| Target | Description |
| --- | --- |
| `make` / `make all` | build the GUI, terminal game and trainer into `build/` |
| `make gui` / `term` / `trainer` | build only one of them |
| `make run` / `make run-term` | build and start the GUI / terminal game |
| `make test` (alias `make check`) | sanitized unit tests, then the end-to-end check |
| `make train` | retrain `models/solver.nn` (extra flags via `TRAIN_ARGS="..."`) |
| `make eval` | measure the shipped model and the expert over 2000 random games |
| `make format` | apply `.clang-format` to every source file |
| `make clean` | delete `build/` |

Objects and binaries live in `build/` (ignored by git). Header dependencies
are tracked (`-MMD -MP`), so editing a header rebuilds what includes it.

---

## Project layout

```
6dle/
├── Makefile
├── README.md
├── .clang-format              code style (make format)
├── data/
│   └── wordBank.txt           22,159 six letter words (dictionary + secrets)
├── models/
│   └── solver.nn              trained network (1.1 MB, see "Model file")
├── docs/                      screenshots used by this README
├── src/
│   ├── core/                  the game itself, no I/O
│   │   ├── words.[ch]         load / validate / look up the word list
│   │   ├── pattern.[ch]       colouring a guess (the Wordle rules)
│   │   ├── game.[ch]          game state: guesses, win/loss, keyboard colours
│   │   └── rng.[ch]           small seedable PRNG (reproducible games/training)
│   ├── solver/
│   │   ├── knowledge.[ch]     what the feedback says: remaining words + NN inputs
│   │   ├── expert.[ch]        entropy solver used as the teacher
│   │   ├── nn.[ch]            multi-layer perceptron, backprop, Adam, save/load
│   │   └── solver.[ch]        the neural network policy (what the GUI uses)
│   ├── gui/                   GTK 3 front end
│   │   ├── main.c             application, window, keyboard input
│   │   ├── app.h              shared state of the two tabs
│   │   ├── play.c             Play tab
│   │   ├── solve.c            Solver tab (Step button, top 5 panel)
│   │   ├── board.[ch]         the grid of tiles
│   │   ├── keyboard.[ch]      on-screen keyboard
│   │   └── style.[ch]         CSS (colours, tile sizes)
│   └── term/
│       └── main.c             terminal front end (+ --solve)
├── tools/
│   └── train.c                trains / evaluates the network (multi-threaded)
└── tests/
    ├── test.h, main.c         tiny test framework and runner
    ├── test_*.c               one file per module
    └── data/small.txt         33-word fixture list (with deliberate junk lines)
```

---

## How the code fits together

```
                ┌───────────── src/core ─────────────┐
 wordBank.txt ─▶│ words ──▶ game ◀── pattern ── rng  │
                └──────▲─────────────────▲───────────┘
                       │                 │
        ┌──────────────┴───┐     ┌───────┴──────────────── src/solver ──┐
        │ src/gui (GTK)    │     │ knowledge ─▶ features ─▶ nn (MLP)    │
        │ src/term         │────▶│     │                      │         │
        └──────────────────┘     │     └─ candidates ─▶ solver (policy) │
                                 │ expert (entropy) ─▶ tools/train.c ─▶ models/solver.nn
                                 └──────────────────────────────────────┘
```

- **`core`** knows the rules and nothing else. A `Game` holds the secret,
  the guesses and their `Pattern`s, the status, and the keyboard colours.
  `game_guess()` validates a word and returns `GUESS_OK`,
  `GUESS_BAD_LENGTH`, `GUESS_NOT_A_WORD`, `GUESS_REPEATED` or
  `GUESS_GAME_OVER`. Both front ends drive this one engine, so they can't
  disagree on the rules.
- **Patterns**: the colours of a guess are packed in base 3 (digit *i* is
  tile *i*: 0 grey, 1 yellow, 2 green). That gives 3⁶ = 729 values, and
  `728` means all green. The solver compares, counts and stores them as
  plain integers.
- **Word list**: loaded once into a sorted array of `char[7]`. Lookup is a
  binary search, and picking the secret is one uniform random index.
- **Front ends** only translate input into `game_guess()` / `solver_*()`
  calls and draw the result.

---

## The neural network solver

### What problem the network solves

At each turn the solver knows the colours of its previous guesses and has to
choose the next word. The design is split into two parts:

1. **Exact logic: which words are still possible.** A word *w* survives if
   and only if `pattern(guess, w)` equals the colours actually received, for
   every past guess. This is the definition of consistency, so repeated
   letters are handled correctly with no special cases
   (`knowledge_apply`).
2. **Learned judgement: which possible word is the best guess.** That is the
   network's job. It ranks the remaining words, and a softmax turns the scores
   into probabilities.

The network can only choose among still-possible words (impossible words get
probability 0). So it never wastes a guess on a word that cannot be the
answer, and every word in the "top 5" could be the answer. In Wordle terms,
this is *hard mode*.

### Why a feed-forward network (FNN / MLP)?

Everything the colours tell you can be summarised in a **fixed-size
description of the current knowledge**: which letters are green where, which
letters are excluded where, minimum and maximum letter counts, and statistics
of the remaining words. The order in which the clues arrived doesn't matter.
The state is "Markov", so a plain multi-layer perceptron is the right tool.

| Alternative | Why not here |
| --- | --- |
| RNN / LSTM / Transformer | only useful if fed the raw guess history; the summary above already contains everything, so they would add cost for nothing |
| CNN | there is no spatial or translation structure to exploit in a 6-letter word |
| Reinforcement learning (policy gradient, DQN) | possible, but slow and unstable to train from scratch; imitating a strong teacher reaches the same play in minutes |

### Inputs: 626 numbers, all in [0, 1]

| Range | Size | Meaning |
| --- | --- | --- |
| 0–155 | 6×26 | letter known **green** at a position |
| 156–311 | 6×26 | letter known **not** to be at a position (yellow or grey there) |
| 312–389 | 26×3 | the word has **at least** 1 / 2 / 3 copies of the letter |
| 390–415 | 26 | the **exact** count of the letter is known (e.g. 0 for a grey letter) |
| 416–571 | 6×26 | share of possible words with that letter at that position |
| 572–597 | 26 | share of possible words containing the letter |
| 598–623 | 26 | share of possible words containing it twice or more |
| 624 | 1 | log(possible words) / log(dictionary size) |
| 625 | 1 | turn / 7 |

### Architecture

```
626 inputs ─▶ 256 ReLU ─▶ 256 ReLU ─▶ 208 outputs ("query" q)

score(w) = q · φ(w)          φ(w) = 208 binary word features:
                               letter at each position (156)
                               letter present (26)
                               letter present twice or more (26)

P(guess = w) = softmax over the still-possible words of score(w)
```

The output layer does not have one neuron per word (that would be 22k
outputs and 5.7M weights). Instead the network outputs a query vector, and
each word is scored by summing the entries of its own letter features. This
keeps the model at **279,760 parameters (1.1 MB)**. It also generalises
across words that share letters, and it works with **any word list**: a
model trained on one list can play on another.

This form also suits the job. A good Wordle guess splits the remaining words
evenly, which depends mostly on how common each letter is at each position
among them, and that is roughly additive over letters.

### What the percentages mean

The percentages in the GUI are the network's probability that each word is
the best **next guess**: how confident it is, and what its alternatives are.
They sum to 100% over all remaining words, and the top word is the one it
plays.

They are not the probability that a word *is the secret*. Secrets are drawn
uniformly, so every remaining word is equally likely to be the answer,
exactly `1 / remaining`. The panel shows that number separately ("each one is
the secret with x% chance"). A non-uniform "answer probability" would only
make sense with non-uniform secrets, such as a word frequency prior (see
[ideas](#ideas-for-later)).

---

## Training

`make train` runs `build/6dle-train`. It is all C, with one pthread per CPU.

1. **The expert.** For every possible guess *g* among the remaining words, the
   expert computes the **entropy** of the colour pattern it would produce over
   the remaining words. A high entropy means the guess splits them into many
   small groups, so it gains the most information on average. The opening
   move is the same in every game, so its exact entropies are computed once
   over all 22,159 × 22,159 pairs. That takes about 2 s on 12 threads, and the
   best opener is **TORIES** (7.62 bits). Later moves are scored on at most
   2,000 sampled answers.
2. **Soft targets.** The expert's preferences become a probability
   distribution `p(g) ∝ exp((H(g) − H_max) / 0.1 bits)`. Two words 0.1 bits
   apart differ by a factor *e*. The top 24 words are stored for each
   position. These targets tell the network "this word, but those are almost
   as good", which teaches it more than the single best answer would.
3. **Imitation learning.** The expert plays 20,000 random games, sampling its
   moves from the soft targets so that the positions vary. The network then
   minimises the cross-entropy between its softmax and the soft targets
   (8 epochs).
4. **DAgger** (2 rounds). The *network* plays 10,000 games, the expert labels
   every position it reaches, and training continues on all the data
   (4 epochs per round). This teaches the network to recover from positions
   that only its own mistakes lead to.
5. Adam (lr 1e-3, decayed linearly towards 10% within each phase),
   mini-batches of 32 games, He initialisation, seed 1.

Typical run (12 threads): data generation ~25 s, training ~3.5 min,
**~4 min total**.

```bash
make train                                        # defaults above
make train TRAIN_ARGS="--games 40000 --epochs 12" # bigger run
./build/6dle-train --help                         # every option
```

| Option | Default | |
| --- | --- | --- |
| `--games N` | 20000 | expert games to imitate |
| `--epochs N` | 8 | passes over them |
| `--dagger N` | 2 | DAgger rounds |
| `--dagger-games N` | 10000 | games per round |
| `--dagger-epochs N` | 4 | epochs per round |
| `--hidden N` | 256 | width of both hidden layers |
| `--threads N` | all CPUs | worker threads |
| `--seed N` | 1 | random seed |
| `--eval N` | 2000 | evaluation games at the end |
| `--eval-only FILE` | | only evaluate an existing model |
| `--expert` | | also evaluate the expert |
| `--words FILE` / `--out FILE` | | word list / output model |

The same seed and thread count always produce the same model. A different
thread count changes how the work is split and summed, so the model differs
slightly (with the same strength).

### Model file

`models/solver.nn` is a small binary file. It holds the magic
`6DLEMLP1`, the number of layers (`uint32`), the layer sizes (`uint32`) and
then all weights and biases (`float32`), in host byte order (little-endian on
x86 and ARM). Each layer stores its weights input-major, then its biases.
`mlp_load` rejects files that are truncated, oversized or have trailing
bytes.

---

## Results

10,000 random secrets (`./build/6dle-train --eval-only models/solver.nn --expert --eval 10000`):

| Strategy | Win rate | Avg. guesses (wins) | Time per move |
| --- | --- | --- | --- |
| Random still-possible word | 97.67% | 4.36 | — |
| Entropy expert (teacher) | 98.84% | 3.857 | first move ≈ 30 s of CPU, then ms |
| **Neural network** | **98.86%** | **3.856** | **≈ 0.4 ms** (one core) |

Guess distribution of the network:

| 1 | 2 | 3 | 4 | 5 | 6 | 7 | lost |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 243 | 3513 | 4273 | 1289 | 407 | 160 | 114 |

The network plays as well as its teacher at a tiny fraction of the cost. The
expert's first move alone needs about half a billion pattern computations.

The remaining ~1.1% of losses are "trap" families where many words differ by
a single letter: `BATTER/BETTER/BUTTER/NATTER/RITTER…`,
`FIVERS/HIVERS/JIVERS…`, `MASHER/RASHER/LUSHER…`, `CONING/LOPING/CAPING…`. Seven
guesses, each of which must stay a possible answer, cannot cover them all.
Getting past that requires sacrificing guesses that cannot win (see
[ideas](#ideas-for-later)).

---

## Tests

```bash
make test                   # everything
./build/run-tests pattern   # only tests whose name contains "pattern"
```

The tests use a small home-made framework (`tests/test.h` +
`tests/main.c`, ~130 lines, no dependencies) with `CHECK`, `CHECK_INT`, `CHECK_STR`, `CHECK_NEAR` and
`REQUIRE`. The test binary is built with **AddressSanitizer +
UndefinedBehaviorSanitizer** (including leak detection), so a memory error
fails the run even if every check passes.

| Suite | What it verifies |
| --- | --- |
| words | junk lines (wrong length, digits, apostrophes, CRLF, tabs, two words) are skipped; case folding; sorting; de-duplication; **the file is never modified**; lookups; the real list has 22,159 words |
| pattern | hand-checked colourings including repeated letters (green beats an earlier yellow; yellows left to right); **200,000 random pairs checked against a reference implementation**; encoding round-trips for all 729 patterns |
| game | validation (length, dictionary, repeats, game over), win and loss, **keyboard colours never get downgraded**, secrets are uniform over the whole list |
| knowledge | **a repeated grey letter doesn't eliminate the secret**; greens are kept in place; property test: after random guesses, the remaining words are *exactly* the brute-force consistent set; feature layout and ranges |
| expert | entropy of known splits (0 bits, log₂ n, 3/1 split); soft targets form a distribution |
| nn | forward pass checked by hand; **backpropagation checked against numerical gradients**; learns XOR; save/load round-trip; corrupt and truncated files rejected |
| solver | probabilities sum to 1, top-k sorted; only possible words suggested; legal games to the end; gradient check through the word scoring and softmax; one training step lowers the loss; **the shipped model wins ≥ 95% of 300 games** (it gets ~99%) |

After the unit tests, `make test` also trains a tiny model on
`tests/data/small.txt` with the real trainer, then has `6dle-term --solve` use
it. This proves the whole pipeline (train → save → load → solve) works.
The whole suite takes a few seconds.

The GUI has no automated test (it needs a display). Its logic lives in
`core/` and `solver/`, which are tested.

---

## Changes from the original version

The original project (terminal game + unfinished solver) was reviewed. These
are the problems found and how each was handled:

| Problem in the original code | Now |
| --- | --- |
| `Tloader` ran `./remove_invalid_words.sh` through `system()`: shell injection, depends on the working directory, and **rewrote the word list file** in place | the loader validates lines in C, never writes the file (tested) |
| `addWord` trusted its input: any character outside `a-z` (e.g. a 6-character line such as `can't!`, or a short chunk from `fscanf("%6s")`) indexed `child[c - 'a']` **out of bounds** | strict validation: a line is exactly 6 ASCII letters or it is skipped |
| `removeWord` freed the whole path of a word even when other words shared it (losing and leaking them), and freed `child` arrays that stayed reachable → **use-after-free / double free** on the next `freeTree` | the trie is gone; a sorted array with binary search is simpler and faster here |
| `findRandom` chose a random letter at each level, so words under rare prefixes were picked far more often than others (**biased secret**); the solver's copy of that walk could end in a branch emptied by pruning and return a garbage word | uniform index into the list |
| Solver: grey letter → `removeChar` deleted **every** word containing it, which is wrong for repeated letters (e.g. `TEETER` vs `PLANET` eliminated the answer) | exact filtering with `pattern(guess, w) == feedback` (regression tested) |
| Solver: green handling compared letters with the colour code `'g' - 'a'` instead of the guessed letter, and inspected root children instead of the right depth | same: exact filtering (regression tested) |
| Solver: yellow only removed the letter at that position, without requiring it elsewhere | same |
| Solver pruned the **game's own** word tree, corrupting word validation; pruning left dead branches the random walk could fall into | the solver keeps its own candidate list; the game is never touched |
| `blackenKeys` ran *before* colouring and could turn a known green key grey | keyboard colours only ever improve (tested) |
| `game.h` included `<aio.h>` and many unused headers; `struct SendInfo` copied data for nothing; any command-line argument started the solver | cleaned up; explicit `--solve` flag |
| Makefile: `all = game` was a variable, not a target; `clean` failed if files were missing; no header dependencies; tests used Criterion with a wrong path (`../loader/`) and were never run | new Makefile with `build/`, dependency tracking, `make test` |
| `obj/` contained sources (not objects); the `sdl/` stub didn't build (`errx("%p", SDL_GetError)` printed the function pointer) | `src/{core,solver,gui,term}`, `tools/`, `tests/`, `data/`, `models/`; GTK 3 GUI instead of the SDL stub |
| Secrets came from `solutions.txt` (960 lines: 12 duplicates, and 32 words that weren't even accepted as guesses) | secrets drawn from `wordBank.txt` only, as requested; `solutions.txt` removed (it remains in git history) |

Kept from the original: six letters, **7 guesses**, the rule against
repeated guesses, the QWERTY keyboard display, the terminal look (box
drawing, colours, *Want to play some more?*), the word list and the
`.clang-format` style.

---

## Troubleshooting

**`Package gtk+-3.0 was not found`**: install `libgtk-3-dev` (and
`pkg-config`). `make term`, `make trainer` and `make test` work without GTK.

**The window doesn't appear on WSL**: you need WSLg (Windows 11, or recent
Windows 10 with the Store version of WSL). Check that `echo $DISPLAY`
prints something. Running with `GDK_BACKEND=x11 ./build/6dle` sometimes
helps.

**"No solver model found"** in the Solver tab: `models/solver.nn` is missing
or doesn't match. Run `make train`, or point to a model with
`./build/6dle --model path/to/solver.nn`.

**Different theme**: the app uses GTK's built-in `Adwaita:dark` theme so the
board colours always look right. Set `GTK_THEME` yourself to override it.

**Moved the repository**: the default data paths are absolute and compiled
in, so run `make clean && make`.

---

## Ideas for later

- **Non-candidate probe guesses**: the network could also score words that
  can't be the answer, which is how the remaining "trap" losses are avoided.
  The output layer already supports any word; only the masking and the
  teacher would change.
- **Word frequency prior**: draw secrets with a preference for common words
  and train the network to predict *which word is the secret*. The
  percentages would then be true answer probabilities.
- **GTK 4** port (needs `libgtk-4-dev`). The GTK code is isolated in
  `src/gui/`.
- **Tile flip animations** with CSS transitions.

---

## Credits

The word list comes from SOWPODS:
https://www.wordgamedictionary.com/sowpods/download/sowpods.txt
(all 6 letter words).

Inspired by Wordle by Josh Wardle.
