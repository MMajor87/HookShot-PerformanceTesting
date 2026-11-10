<h1 align="center">
  <br>
  <a>
  <img src="assets/hookshot.png" alt="Hookshot" width="200">
</a>

  <br>
  <b>Hookshot: Performance Testing</b>
  <br>
</h1>

<h4 align="center">A Windows-focused testing branch of <a href="https://github.com/NotRexed/Hookshot">Hookshot</a> that works on game performance.
</h4>

<p align="center">
  <a href="#what-this-is">What this is</a> |
  <a href="#results">Results</a> |
  <a href="#install">Install</a> |
  <a href="#updates">Updates</a> |
  <a href="#settings">Settings</a> |
  <a href="#reporting-a-problem">Reporting a problem</a> |
  <a href="#known-issues">Known issues</a> |
  <a href="#build-the-renderer-from-source">Build</a> |
  <a href="#credits">Credits</a> |
  <a href="#license">License</a>
</p>

> [!IMPORTANT]
> This is **not** the official Hookshot repository. For the game itself, its builder, Linux support and the full
> project, go to **[NotRexed/Hookshot](https://github.com/NotRexed/Hookshot)**.
>
> Like the official project, this repository **does not contain the game, keys or an executable**. You need your own
> copy of the game, built into a game folder by the Hookshot Builder (this repository's or the official one). Never share the files that build creates
> (`hookshot.exe`, the generated code, the shader caches or your saves).

## What this is

[Hookshot](https://github.com/NotRexed/Hookshot) is a static recompilation of The Legend of Zelda: Ocarina of Time
(Nintendo Switch 2) for PC. The game's code is translated and compiled on your own PC, and its graphics are rebuilt on
Vulkan by a renderer, `hookshot_vk.dll`.

This repository works on that **renderer**, with two goals:

- **Game performance**, in busy scenes where the official build drops well below 60 fps even on fast hardware.
- **Windows**, which is the only platform built and tested here. Linux is untouched and untested in this branch: use
  the official releases there.

Each release ships the **full Hookshot Builder**, set up to build from this repository, and the renderer on its own as a
**drop-in `hookshot_vk.dll`**. The Builder makes the game on your PC from your own copy, so the game executable and the
generated code are never shared. Each release names the game version it is for. Currently that is **Hookshot v2.0.14**.

### What it changes

Based on official Hookshot **v2.0.14** (since `v2.0.14-perf.1`; earlier releases here were based on v2.0.11, v2.0.10 and v2.0.6). See the
[official release notes](https://github.com/NotRexed/Hookshot/releases) for what changed in the game itself:

| Change | What it does | Setting (`hookshot_env.txt`) |
|---|---|---|
| **Write-back limit** | The renderer copied every buffer the GPU wrote back to the game's memory after each batch of work: 26-68 MB a frame, about half the GPU's time in busy scenes. Most of it (the foliage culling's lists) is never read back by the game. Ranges over 1 MB now stay on the GPU; small results the game does read, such as the auto-exposure meter, are still copied. | `RX_VK_WB_MAX_KB=1024` (needed; 0 = off) |
| **Sampler cache** | Each texture of each draw looked up its sampler through a lock, a copy and a hash, about 48,000 times a frame. Now cached until a sampler actually changes. | on; `RX_VK_SAMPLER_CACHE=0` = off |
| **Texture descriptor reuse** | When a draw uses the same shader and the same textures as the previous draw, its texture setup is copied instead of rebuilt. | on; `RX_VK_TEX_REUSE=0` = off |
| **Write collection** | The CPU-write tracking checks its pages under one lock. A small saving. Upstream has its own version of this since v2.0.10, so it has no switch any more. | on |
| **Plant textures** | Upstream pull request [#761](https://github.com/NotRexed/Hookshot/pull/761) merged: compute shaders prepared before the game find their storage buffer table. Fixes most of the flat, white or green near-plant textures ([#763](https://github.com/NotRexed/Hookshot/issues/763), [#811](https://github.com/NotRexed/Hookshot/issues/811)). | always on |
| **Texture budget on big cards** | Upstream caps texture memory at 6 GB. In Hyrule Castle that cap made the renderer evict textures and make them again every frame while walking (the renderer at 16-17.6 ms, 40-45 fps on a 16 GB card). Where the driver allows 12 GB or more, the budget is now 60% of that (about 9 GB on a 16 GB card). Smaller cards keep upstream's rule (45% on cards of 8 GB or less since v2.0.14). | on; `RX_VK_VRAM_MB=<MB>` sets it |
| **Freeing overwritten render targets** | The castle makes its scene's render targets again almost every frame, each a little further along a running block of memory, so each new one overlaps the last few. The renderer only counted a render target as overwritten when a newer one started at exactly the same address, so the old ones piled up (about 6 GB) and pushed the scene's textures out of the budget. A render target that a newer one has drawn over in any part is now freed after 8 frames unused. Castle: render targets 6 GB to about 1 GB, texture lookups 1.5-4 ms to under 1 ms a frame, renderer 15-17 ms to 12-14 ms a frame (with `RX_VK_ASTC_BC_ALL=1`, VRAM 13.8 to 9.8 GB). | on; `RX_VK_EVICT_OVERLAPPED=0` = off |
| **Depth image reuse** | Upstream has its own render target image pool since v2.0.12 (`RX_VK_TARGET_POOL_MB`, 256 MB), which covers this; ours was dropped in favour of it. | upstream's |
| **Faster memory imports** | Upstream fixed the same import hitches since v2.0.12 by committing a block's pages in one call instead of asking Windows region by region; ours was dropped in favour of it. | upstream's |
| **Smaller CPU savings** | A list of game-memory words was merged and sorted several times a frame even when nothing in it had changed (~4% of the renderer thread), and a setting was read from the environment at every compute dispatch. | always on |
| **Evicting render targets (opt-in)** | Textures the GPU drew into are normally kept a minute after their last use. With this setting they can go after about a second, written back to the game's memory first where possible. It helped the castle before the two changes above; with them it should rarely be needed. | off; `RX_VK_EVICT_RENDERED=1` = on |
| **Button prompts in this Builder** | The official Builder's Python lacks `archspec`, which the prompt drawing (astc-encoder-py) imports, so a game built on Windows showed only the Switch's icons. This Builder's Python includes it, so the Xbox, PlayStation and Steam Deck prompts are built. | always on |
| **Builder and updates from this repository** | The Builder's setup, the Builder's own update and the game's update check all use this repository, and every Builder part is mirrored in its `builder-parts` release. New renderer builds install themselves in seconds; a new game version is a full rebuild. See [Updates](#updates). | on; `RX_VK_UPDATE_CHECK=0` = off |
| **F9 frame capture** | The capture writes its own `frame_log.txt` and a draw-stats report, and `RX_VK_DEEP` adds deeper captures for chosen shaders. See [Reporting a problem](#reporting-a-problem). | off until set |

The sampler cache was also sent upstream as pull request [#901](https://github.com/NotRexed/Hookshot/pull/901), and the
measurements behind the write-back limit are in upstream [#384](https://github.com/NotRexed/Hookshot/issues/384).

## Results

Measured on Windows 11 with an RTX 4080 (16 GB), an i9-14900KF and 32 GB of RAM at 2560x1440, resolution 100%, standing
at the same busy outdoor spot (about 3,100-3,500 draws a frame). Every row has #761 and the `HOS_PIN` setting below:

| Build | fps | Renderer CPU time a frame | GPU time spent copying back |
|---|---|---|---|
| Official v2.0.5 renderer | ~45 | 15.2 ms | 7.1 ms (68.5 MB a frame) |
| + write-back limit | 55-56 | 15.2 ms | 0.8 ms (8.6 MB a frame) |
| + sampler cache and texture reuse | **59-60** (the game's cap) | 13.8 ms | not measured |

**Hyrule Castle Town**, the same machine, an automated route in the market fountain (looking around, turning the camera,
stepping in and out of the water), 3 runs each with `RX_VK_ASTC_BC_ALL=1`:

| Build | fps (mean) | Worst 5 s | Frames over 22 ms | Renderer CPU time a frame | Worst frame |
|---|---|---|---|---|---|
| `v2.0.6-perf.2` | 58.0 | 55.8 | 3.3% | 12.3 ms | 54-65 ms |
| `v2.0.6-perf.3` | **58.8** | **56.3** | **2.8%** | **11.6 ms** | **~40 ms** |

The game's speed is tied to its frame rate, so the 60 fps cap stays. The gains show up as fewer and shorter drops in
busy areas. Your numbers will depend on your hardware; reports from other setups are welcome.

## Install

You need 64-bit Windows 10 or 11, a Vulkan 1.3 graphics card with mesh shaders, and **your own copy of the game at the
supported version** (program `0400486024DEC000`, update v131072, as a `.nup` file or `romfs` and `exefs` folders). The
official [What you need](https://github.com/NotRexed/Hookshot#what-you-need) list has the details; it all applies here.

### New game folder (recommended)

1. Download **`HookshotBuilder.exe`** from the [latest release](../../releases/latest) and run it. It downloads the rest
   of the Builder (about 470 MB) from this repository, checks every part, and opens it.
2. Pick your game, choose where to install, and press **Build the game**. It takes about 10 to 45 minutes, depending on
   your CPU and memory.
3. Open `hookshot_env.txt` in the game folder, next to `hookshot.exe` (create it if it isn't there), and copy in the
   settings you want from the release's `hookshot_env.sample.txt`. At least `RX_VK_WB_MAX_KB=1024` (the biggest gain)
   and `RX_VK_ASTC_BC_ALL=1` (smoother Hyrule Castle).
4. Press **Play**. The first start prepares the shaders, which takes a while; later starts are faster.

This folder then follows this repository: new renderer builds and new game versions are offered from here.

### Renderer only, in an official game folder

If you already have a folder made by the **official** Builder at the version the release names:

1. Download `hookshot_vk.dll` and `hookshot_env.sample.txt` from the [latest release](../../releases/latest).
2. In the game folder's `bin\` folder, rename `hookshot_vk.dll` to `hookshot_vk.dll.official`. That is your way back.
3. Put the downloaded `hookshot_vk.dll` into `bin\`, and copy the settings you want into `hookshot_env.txt` as above.
4. Start the game. The first start prepares the shaders again (about a minute): this renderer translates them a little
   differently from the official one.

To undo, put `hookshot_vk.dll.official` back as `hookshot_vk.dll`. The renderer updates itself in such a folder too, but
the official launcher's own update offer still comes from the official project (see below).

## Updates

There are two kinds of update, and each is asked about once:

- **A new renderer build for the same game version** (`v2.0.6-perf.1` to `perf.2`): the renderer replaces itself in a
  few seconds, described below.
- **A new game version** (for example v2.0.6 to v2.0.10): a full rebuild of the game folder by the Builder, as with the
  official project. In a folder made by this repository's Builder, the game's launcher and the Builder offer it from
  this repository.

**The renderer updates itself.** At each start it asks this repository for its latest release. When there is a newer
build **for the same game version**, it asks you:

- **Yes:** it downloads the new `hookshot_vk.dll`, checks it against the release's `hookshot_vk.dll.sha256`, and uses it
  from the next start. The old file is kept as `hookshot_vk.dll.old` until then. If anything fails, nothing changes.
- **No:** you are asked again at the next start.
- **Cancel:** that version is skipped.

It only ever replaces the renderer, never the game, and never offers a build made for a different game version.
`RX_VK_UPDATE_CHECK=0` turns it off.

**In a folder made by the official Builder,** the launcher's update offer still comes from the official project. It
rebuilds the whole folder from a new official release and puts the official renderer back; this renderer then won't be
offered for the new game version until there is a matching release here. Set `HOOKSHOT_NO_UPDATE_CHECK=1` to stop that
offer, or rebuild the folder with this repository's Builder.

Releases are named `<game version>-perf.<n>`, for example `v2.0.6-perf.1`. Each one holds the Builder's setup
(`HookshotBuilder.exe`) and its `hookshot-setup.txt`, the renderer with its `.sha256`, and `hookshot_env.sample.txt`.
The Builder's larger parts (its .NET runtime, Python, the compiler and so on) live in the `builder-parts` pre-release.
The renderer's updater arrived in `perf.2`, so a copy of `perf.1` has to be replaced by hand once.

## Settings

All settings go in `hookshot_env.txt` in the game folder, one `NAME=VALUE` per line (`#` starts a comment). The
`hookshot_env.sample.txt` in each [release](../../releases/latest) has them with explanations.

| Setting | Default | What it does |
|---|---|---|
| `RX_VK_WB_MAX_KB=1024` | off | The write-back limit, in KB. The main performance gain. |
| `RX_VK_SAMPLER_CACHE=0` | on | Turns the sampler cache off. |
| `RX_VK_TEX_REUSE=0` | on | Turns texture descriptor reuse off. Try this first if an object shows a clearly wrong texture. |
| `RX_VK_UPDATE_CHECK=0` | on | Turns this renderer's update check off. |
| `RX_VK_VRAM_MB=<MB>` | 60% of what the driver allows (12 GB+), else upstream's | A fixed texture budget. |
| `RX_VK_EVICT_OVERLAPPED=0` | on (8 frames) | Turns off freeing render targets that a newer one has drawn over. A number sets how many frames unused first. Try `0` if something flickers or shows an old frame. |
| `RX_VK_EVICT_RENDERED=1` | off | Lets render targets be evicted after about a second unused. Rarely needed now; remove it if black, garbage or rainbow textures appear. |
| `RX_VK_ASTC_BC_ALL=1` | off on cards with 6 GB or more | **Recommended.** Keeps the finer compressed textures smaller in VRAM too: in Hyrule Castle 2.5 GB instead of 5.8 GB of the 9 GB budget, so textures stop being evicted and loaded again while walking. No visible difference in testing. |
| `HOS_PIN=MainThread=FFFF,Presentation=FFFF,ModuleSystemWorker=FFFF` | off | Intel CPUs with performance and efficiency cores (12th gen and newer, 8 performance cores): keeps the game's busiest threads on the performance cores. A few fps in testing. |
| `HOOKSHOT_NO_UPDATE_CHECK=1` | off | Stops the official launcher's update offer. |

The NVIDIA shader cache size (NVIDIA Control Panel, Manage 3D settings) should be 10 GB or Unlimited. The default 1 GB
fills up and makes the game rebuild shaders while you play.

## Reporting a problem

Please [open an issue](../../issues) with your graphics card and driver, CPU, which release you use, where you were in
the game, and `hookshot_log.txt` from the game folder.

**For a missing or wrong texture,** an F9 capture shows exactly which shader drew it and what it was given. Add these
lines to `hookshot_env.txt`, with your own game folder, and create the `dumps` folder:

```
RX_VK_DUMP_KEY=1
RX_VK_DUMP_DIR=C:\Path\To\Hookshot\dumps
RX_VK_DRAW_STATS=C:\Path\To\Hookshot\dumps
```

Then look at the problem in the game and press **F9**. That saves the next frame step by step into
`dumps\dump_<n>\`, with a `frame_log.txt`, and writes `dumps\draw_stats_<n>.txt`. Zip the newest `dump_<n>` folder and
the newest `draw_stats_<n>.txt`, attach them to the issue, and say what colour the problem looks (flat green, white,
black, magenta...). We may then ask for a second capture with `RX_VK_DEEP=<shader key>`, which also saves that shader's
buffers and textures. Those captures are large.

## Known issues

- **Hyrule Castle** is much smoother since `v2.0.6-perf.2`. A few short hitches remain (about 30-40 ms, 2-3 a minute
  around the fountain), inside the graphics driver when the game's memory is first used ([#2](../../issues/2)). Upstream lists lower fps in Castle Town and Hyrule Field as a known v2.0.6 issue too.
- **Texture descriptor reuse** has had limited play time. If an object ever shows a clearly wrong texture, try
  `RX_VK_TEX_REUSE=0`, and please report whether that fixes it.
- Everything else that is open upstream applies here too, apart from what this branch fixes. See the
  [official issues](https://github.com/NotRexed/Hookshot/issues).

## Build the renderer from source

Only the renderer is built here; the game itself comes from the official Hookshot Builder. You need:

- This repository, checked out.
- The tools folder of an installed official Hookshot Builder (in
  `%LOCALAPPDATA%\HookshotBuilder\builders\<builder>\tools`), which has the compiler (llvm-mingw), the package cache
  (`cpm`) and Python.
- A game folder built with the official Builder at the matching version (its `.hookshot_exefs` and `romfs` folders).

Then, from Git Bash in the repository folder (replace the paths):

```sh
B="<builder tools folder>"; W="<a work folder>"; G="<your game folder>"
HOOKSHOT_MINGW="$B/llvm-mingw" RX_CPM="$B/cpm" OOT_WORK="$W" \
OOT_EXEFS="$G/.hookshot_exefs" OOT_ROMFS="$G/romfs" OOT_DUMP="$W/out" \
RX_RELEASE=1 RX_VK_ONLY=1 PATH="$B/llvm-mingw/bin:$PATH" \
"$B/python/python.exe" -I nx2/build.py --standalone --jobs 8
```

The renderer lands in `<work folder>/sa/build/hookshot_vk.dll`. A first build takes about a minute; later ones only
recompile what changed.

Changes to `main` go through pull requests, and every pull request needs the owner's approval before it can be merged.
See [CONTRIBUTING.md](CONTRIBUTING.md) for the upstream project's code conventions, which this branch follows.

## Credits

Everything here builds on the Hookshot project and the people behind it. From the official project's credits:

- **NotRexed**: Hookshot, BlueShell (Mario Kart World), Skybound (Tears of the Kingdom) and the a64-kit tools.
- **[Eden](https://git.eden-emu.dev/eden-emu/eden)** and **Eglantine**: the shader recompiler and Switch 2 shader work.
- **suyu**: the graphics decoders and the AArch64 translator.
- **[seedds/nxnative](https://github.com/seedds/nxnative)**: reference for how task and mesh shaders work.
- **[EvilBoyce / evilmfkr](https://github.com/evilmfkr/Hookshot)** (EvilOOT): the GPU culling that brings back Kokiri's foliage
  (trees, ferns, grass, falling leaves, fireflies), the ambient light probes, the pond's water, the game's own fog, the freeze
  after Load (Horizon's mutex handoff) and the exposure flicker fix.
- **[zortos293/Lichii](https://github.com/zortos293/Lichii)**: reference for Ampere shader lowering, and its compare-exchange retry fix.
- **[mstorsjo/llvm-mingw](https://github.com/mstorsjo/llvm-mingw)**: the bundled Windows compiler (no Visual Studio needed).
- **[kinnay/zbic](https://github.com/kinnay/zbic)**, **Opus** and **SDL**: decompression, audio and controllers.
- **[sostenesfreitas/ocarina-recomp](https://github.com/sostenesfreitas/ocarina-recomp)**: the original README's layout, and its
  notes confirmed the mesh stage's control section.
- **[MythosOfs/Orchid](https://github.com/MythosOfs/Orchid)**: the fog and exposure workaround for Ocarina of Time.
- **Mesa (NAK)**: the reference for Ampere's instruction encoding.
- Everyone who sent fixes upstream: SkippySteve (Linux and the AppImage, and many more), monsterlag, klimovich008, hckrman101
  (the exposure flicker and fog fixes, build speed, CI), shayderrr, gab9281, Wpaif, nicholasgower, ZurielAtr (pull request #761,
  included here), Joey-Magnus, Zorkats, d3zessete, chrispository, vzeol, leonidasmark, Segmentree, ShyVortex, hiKareeem and
  i900gaming, among others.

## License

Like Hookshot, this is free software under the [GNU General Public License, version 3 or later](LICENSE)
(GPL-3.0-or-later). It includes code from Eden, suyu and Orchid, which are under the GPL too. Third-party parts under other
licenses that work with the GPL (Sirit, the Vulkan Memory Allocator, SDL, Opus, stb, zbic and the a64-kit tools) keep their
own notices in their folders.

This is an unofficial fan project, not affiliated with or endorsed by Nintendo or by the official Hookshot project. The
Legend of Zelda and Ocarina of Time are trademarks of Nintendo.
