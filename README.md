# Fin's Ego Lift Vocal Lift Super Mega Sonic 5000

## Open it on a Mac with Projucer (step by step)

1. Install **Xcode** from the Mac App Store, then open it once so it finishes setting up.
2. Download **JUCE 9** from juce.com/get-juce and unzip it, for example to your home folder as `JUCE`.
3. Open **Projucer** (inside the JUCE folder). If macOS blocks it, right-click it and choose **Open**.
4. Go to **Projucer → Global Paths…** and set:
   - **Path to JUCE**: your JUCE folder, for example `/Users/you/JUCE`
   - **JUCE Modules**: the `modules` folder inside it, for example `/Users/you/JUCE/modules`

   Nothing should be red. Close the window.
5. Double-click **FinsEgoLift5000.jucer**. At the top, pick **Xcode (macOS)** and click the Xcode icon.
6. In Xcode, choose the **Fins Ego Lift 5000 - All** scheme, then **Product → Scheme → Edit Scheme → Run → Build Configuration: Release**, and press **Cmd+B**.
7. Your builds are in `Builds/MacOSX/build/Release`:
   - **Fins Ego Lift 5000.app** is the standalone app. Open it and sing (use headphones).
   - **Fins Ego Lift 5000.component** is for Logic. Copy it to `/Library/Audio/Plug-Ins/Components`.
   - **Fins Ego Lift 5000.vst3** is for Ableton and others. Copy it to `/Library/Audio/Plug-Ins/VST3`.
   - **Fins Ego Lift 5000.aaxplugin** is for Pro Tools Developer, or for Pro Tools once it's signed.

If Xcode asks about signing, choose **Sign to Run Locally**.

---

## Open it in Projucer

1. Install **JUCE 9** from juce.com. This project uses JUCE 9 features, so older versions won't open it cleanly.
2. In Projucer, go to **Global Paths** (Projucer menu on Mac, File menu on Windows). Set **Path to JUCE** to your JUCE folder and **JUCE Modules** to the `modules` folder *inside* it. Any path still shown in red is wrong.
3. Open **FinsEgoLift5000.jucer**, pick the exporter (Xcode on Mac, Visual Studio 2022 on Windows) and click the IDE button.
4. In the IDE, build the **Release** configuration of the formats you want (Standalone, VST3, AU or AAX).

The `.jucer` and `CMakeLists.txt` build the same plug-in, so use whichever you prefer. Projucer writes its generated projects into `Builds/`.

---

## Download it

Go to this repo's **Releases** page (on the right, or `/releases`). Each build gives you:

| File | What it's for |
|---|---|
| `FinsEgoLift5000-Mac-App.zip` / `-Windows-App.zip` | The standalone app. Open it and sing. |
| `FinsEgoLift5000-Mac-VST3.zip` / `-Windows-VST3.zip` | The plug-in for Ableton, FL Studio, Cubase, Reaper, Studio One |
| `FinsEgoLift5000-Mac-AU.zip` | The plug-in for Logic and GarageBand |
| `...-AAX-unsigned.zip` | Pro Tools. Needs PACE signing or Pro Tools Developer (see below). |

**Singing into the app**
1. Unzip it and open it.
   - **Mac:** macOS blocks it the first time. Go to System Settings → Privacy & Security, click **Open Anyway**, then allow the microphone.
   - **Windows:** click **More info → Run anyway**.
2. Put headphones on so you don't get feedback.
3. Go to **Options → Audio/MIDI Settings** and pick your mic or interface.
4. Click the yellow "input muted" bar to unmute, and sing.

**Installing the plug-in**
- Mac VST3: copy it to `/Library/Audio/Plug-Ins/VST3`
- Mac AU: copy it to `/Library/Audio/Plug-Ins/Components`
- Windows VST3: copy it to `C:\Program Files\Common Files\VST3`

Then rescan plug-ins in your DAW.

---

The Pro Tools version of the purple vocal rack: AAX for Pro Tools, plus VST3, AU and a standalone app from the same code. The sound engine is a straight C++ port of the browser version, and the two produce the same output.

## What's been tested, and what hasn't

Tested here, on Linux:
- The C++ engine was compared sample by sample with the browser engine on all 10 presets, at Ego 50% and 100%. They match to between -80 and -140 dB, which is numerically identical.
- The engine self-test passes at 44.1, 48 and 96 kHz: no NaNs, no overs, no silence, and every tail decays.
- The VST3 builds with zero warnings and passes **pluginval at strictness 10**, including the editor, state save/restore, automation, thread-safety and parameter fuzzing tests.

Not tested:
- The AAX build. Avid only supports AAX on macOS and Windows, so it couldn't be compiled here.
- Pro Tools itself.

The AAX build uses the same processor code that passed pluginval, plus JUCE's AAX wrapper. Build it with the steps below, and expect to fix a compiler hiccup or two on the first Mac build if Xcode is newer than JUCE expects.

## 1. Build it

You need CMake 3.22 or newer. The first configure downloads JUCE 9.0.4, which includes Avid's AAX SDK, so nothing else needs downloading.

**Mac.** Install Xcode from the App Store, then CMake (`brew install cmake`). Then run:

```bash
cmake -S . -B build -G Xcode
cmake --build build --config Release
```

**Windows.** Install Visual Studio 2022 with the "Desktop development with C++" workload, and CMake. Then run:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The plug-ins land in `build/FinsEgoLift_artefacts/Release/` in the `AAX`, `VST3` and `AU` folders (AU is Mac only). Mac builds are universal, covering Apple Silicon and Intel.

**No Mac or PC handy for building?** Push this folder to a GitHub repository. `.github/workflows/build.yml` builds the Mac and Windows versions on GitHub's machines, and you download them from the Actions tab. That workflow hasn't been run yet.

## 2. Get it into Pro Tools

Retail Pro Tools only loads AAX plug-ins that are digitally signed with PACE's tools. An unsigned build will show up as "not a valid plug-in". There are two routes.

**To test while you work (free):**
1. Sign up as an AAX developer at developer.avid.com.
2. Email devauth@avid.com to ask for a Pro Tools Developer activation code.
3. Download Pro Tools Developer from your Avid developer account. It loads unsigned plug-ins.

**To use it in your normal Pro Tools, or give it to anyone else:**
1. Email audiosdk@avid.com with the subject **"PACE Eden Signing Tools Request"**. Include a short description of the plug-in, a screen recording if you can, your name or company, and a phone number.
2. Avid approves the request and passes you to PACE, who send the signing tools (wraptool). You'll need an iLok USB key to hold the signing certificate.
3. Sign the built plug-in. PACE gives you the exact command and your wrap ID. On a Mac it looks roughly like:

   ```bash
   wraptool sign --verbose --account YOUR_PACE_ACCOUNT --wcguid YOUR_WRAP_GUID \
     --signid "Developer ID Application: Your Name" \
     --in  "build/FinsEgoLift_artefacts/Release/AAX/Fins Ego Lift 5000.aaxplugin" \
     --out "build/FinsEgoLift_artefacts/Release/AAX/Fins Ego Lift 5000.aaxplugin"
   ```

**Install** by copying `Fins Ego Lift 5000.aaxplugin` to:
- Mac: `/Library/Application Support/Avid/Audio/Plug-Ins`
- Windows: `C:\Program Files\Common Files\Avid\Audio\Plug-Ins`

Then restart Pro Tools. It shows up under **Reverb** in the plug-in menu, as "Fins Ego Lift 5000".

## Using it in Pro Tools

- **Insert mode.** Use it as a **mono → stereo** insert on a mono vocal track to get the full width, the ping-pong delay and the stereo reverb. Mono → mono and stereo → stereo also work.
- **Latency.** The plug-in reports its 47 ms delay, so Pro Tools' Delay Compensation lines everything up automatically. Leave Delay Compensation on.
- **Tempo.** The delay follows the session tempo by default. Turn off "Follow session" in Echo Chamber to set your own BPM.
- **Bypass.** Pro Tools' bypass button is wired to **Humble mode**. The bypassed vocal stays time-aligned, so A/B comparisons are fair.
- **Automation.** Every knob, switch and The Ego macro can be automated. In automation lanes each control shows its real job first, then the joke, for example "Drive (Tortured artist)".
- **Presets.** The 10 presets are in the plug-in's own menu and in the host's program list. Save your own versions with Pro Tools' preset menu, which stores everything, including The Ego.
- **Resizing.** The window scales from 600 to 1800 pixels wide.

## Licensing, before you sell it

- **JUCE** is free under the AGPLv3, which means releasing your source code, or under a commercial JUCE licence. The free JUCE Starter tier covers you up to $20,000 a year in revenue, and paid tiers cover more. Check juce.com for the current terms before release.
- **Avid's AAX SDK** is used under Avid's AAX licence agreement, which you accept with your developer account, or under GPLv3. PACE signing is provided free through Avid.
- **The names.** Rename the company ("Ego Labs") and the plug-in codes in `CMakeLists.txt` before a public release. Then do a clean build, because hosts identify plug-ins by those codes.

## Project layout

| Path | What it is |
|---|---|
| `Source/FelEngine.h` | The whole DSP engine. No JUCE dependency. |
| `Source/FelModel.h` | The Ego macro, preset loading, value formatting |
| `Source/FelTables.h` | Knobs, sections and presets. Generated from `browser/shared.js`. |
| `Source/PluginProcessor.*` | Parameters, presets, state, latency, bypass, host tempo |
| `Source/PluginEditor.*` | The purple faceplate |
| `tests/engine_test.cpp` | Renders every preset at 3 sample rates and checks it stays sane (`FelEngineTest` target) |
| `tests/compare.cpp`, `tests/js_ref.js` | The C++ vs browser comparison |
| `browser/` | The browser engine, the source of truth for presets and knob definitions |

To change presets or knob jokes, edit `browser/shared.js`, then regenerate the tables:

```bash
node tools/gen_tables.js browser Source/FelTables.h
```
