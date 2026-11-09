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
> copy of the game, built into a game folder by the official Hookshot Builder. Never share the files that build creates
> (`hookshot.exe`, the generated code, the shader caches or your saves).

## What this is

[Hookshot](https://github.com/NotRexed/Hookshot) is a static recompilation of The Legend of Zelda: Ocarina of Time
(Nintendo Switch 2) for PC. The game's code is translated and compiled on your own PC, and its graphics are rebuilt on
Vulkan by a renderer, `hookshot_vk.dll`.

This repository works on that **renderer**, with two goals:

- **Game performance**, in busy scenes where the official build drops well below 60 fps even on fast hardware.
- **Windows**, which is the only platform built and tested here. Linux is untouched and untested in this branch: use
  the official releases there.

What it ships is a **drop-in `hookshot_vk.dll`** for a game folder made by the official Hookshot Builder. The renderer
contains no game code, so it can be shared. The game executable can't be, because it is built on each PC from that
player's own copy. Each release names the game version it is for. Currently that is **Hookshot v2.0.5**.

### What it changes

Based on official Hookshot **v2.0.5**:

| Change | What it does | Setting (`hookshot_env.txt`) |
|---|---|---|
| **Write-back limit** | The renderer copied every buffer the GPU wrote back to the game's memory after each batch of work: 26-68 MB a frame, about half the GPU's time in busy scenes. Most of it (the foliage culling's lists) is never read back by the game. Ranges over 1 MB now stay on the GPU; small results the game does read, such as the auto-exposure meter, are still copied. | `RX_VK_WB_MAX_KB=1024` (needed; 0 = off) |
| **Sampler cache** | Each texture of each draw looked up its sampler through a lock, a copy and a hash, about 48,000 times a frame. Now cached until a sampler actually changes. | on; `RX_VK_SAMPLER_CACHE=0` = off |
| **Texture descriptor reuse** | When a draw uses the same shader and the same textures as the previous draw, its texture setup is copied instead of rebuilt. | on; `RX_VK_TEX_REUSE=0` = off |
| **Write collection** | The CPU-write tracking checks its pages under one lock. A small saving. | on; `RX_VK_COLLECT_BATCH=0` = off |
| **Plant textures** | Upstream pull request [#761](https://github.com/NotRexed/Hookshot/pull/761) merged: compute shaders prepared before the game find their storage buffer table. Fixes most of the flat, white or green near-plant textures ([#763](https://github.com/NotRexed/Hookshot/issues/763), [#811](https://github.com/NotRexed/Hookshot/issues/811)). | always on |
| **Updates for the renderer** | At start, the renderer checks this repository's releases and offers newer builds for the same game version. See [Updates](#updates). | on; `RX_VK_UPDATE_CHECK=0` = off |
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

The game's speed is tied to its frame rate, so the 60 fps cap stays. The gains show up as fewer and shorter drops in
busy areas. Your numbers will depend on your hardware; reports from other setups are welcome.

## Install

You need a working game folder made by the **official Hookshot Builder at the version the release names** (see the
official [Install and play](https://github.com/NotRexed/Hookshot#install-and-play) guide), on 64-bit Windows 10 or 11
with a Vulkan 1.3 graphics card that has mesh shaders.

1. Download `hookshot_vk.dll` and `hookshot_env.sample.txt` from the [latest release](../../releases/latest).
2. In your game folder's `bin\` folder, rename `hookshot_vk.dll` to `hookshot_vk.dll.official`. That is your way back.
3. Put the downloaded `hookshot_vk.dll` into `bin\`.
4. Open `hookshot_env.txt` in the game folder, next to `hookshot.exe` (create it if it isn't there), and copy in the
   settings you want from `hookshot_env.sample.txt`. At least `RX_VK_WB_MAX_KB=1024`: that is the biggest gain.
5. Start the game. The first start prepares the shaders again, which takes about a minute. This renderer translates
   shaders a little differently from the official one.

To undo, put `hookshot_vk.dll.official` back as `hookshot_vk.dll`.

## Updates

**This renderer updates itself.** At each start it asks this repository for its latest release. When there is a newer
build **for the same game version**, it asks you:

- **Yes:** it downloads the new `hookshot_vk.dll`, checks it against the release's `hookshot_vk.dll.sha256`, and uses it
  from the next start. The old file is kept as `hookshot_vk.dll.old` until then. If anything fails, nothing changes.
- **No:** you are asked again at the next start.
- **Cancel:** that version is skipped.

It only ever replaces the renderer, never the game, and never offers a build made for a different game version.
`RX_VK_UPDATE_CHECK=0` turns it off.

**The official launcher's update** is separate. It rebuilds the whole game folder from a new official release and puts
the official renderer back. When that happens, this renderer won't be offered for the new game version until there is a
matching release here. If you'd rather not be asked by the official launcher, set `HOOKSHOT_NO_UPDATE_CHECK=1`.

Releases are named `<game version>-perf.<n>`, for example `v2.0.5-perf.2`. The updater arrived in `perf.2`, so a copy of
`perf.1` has to be replaced by hand once.

## Settings

All settings go in `hookshot_env.txt` in the game folder, one `NAME=VALUE` per line (`#` starts a comment). The
`hookshot_env.sample.txt` in each [release](../../releases/latest) has them with explanations.

| Setting | Default | What it does |
|---|---|---|
| `RX_VK_WB_MAX_KB=1024` | off | The write-back limit, in KB. The main performance gain. |
| `RX_VK_SAMPLER_CACHE=0` | on | Turns the sampler cache off. |
| `RX_VK_TEX_REUSE=0` | on | Turns texture descriptor reuse off. Try this first if an object shows a clearly wrong texture. |
| `RX_VK_COLLECT_BATCH=0` | on | Turns the batched write collection off. |
| `RX_VK_UPDATE_CHECK=0` | on | Turns this renderer's update check off. |
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

- **Flat green stems on some small ground plants**, from certain camera angles and distances. Traced to the plants'
  culling pass (`102f5b0abb0f1e63`), not fixed yet. It's part of the same family as upstream
  [#763](https://github.com/NotRexed/Hookshot/issues/763). It was there before these changes, and #761 already removed
  most of it.
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
