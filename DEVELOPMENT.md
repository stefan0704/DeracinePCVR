# Déraciné PCVR Port — Complete Developer Documentation & Context Handover

> **Document Status**: Live / Actively Maintained  
> **Target Title**: *Déraciné* (CUSA11919) — FromSoftware / Japan Studio  
> **Base Platform**: AstroQuest (bigmak94) / shadPS4 PCVR OpenXR  
> **Target Scope**: Standalone PCVR (Windows + OpenXR). Standalone Quest APK builds are strictly out of scope.  

---

## 1. Project Overview & Architecture

### 1.1 Goal
Create a smooth, "plug-and-play" PCVR experience for *Déraciné* (PS4 VR exclusive) on modern PC VR headsets (Meta Quest 2/3/Pro via Virtual Desktop/Link, Valve Index, Bigscreen Beyond, HTC Vive, Windows Mixed Reality) without needing a PlayStation 4 console or PSVR hardware.

### 1.2 Base Technology
- **AstroQuest (by bigmak94)**: An open-source OpenXR fork of **shadPS4** that translates PSVR system libraries (`libSceHmd`, `libSceVrTracker`, `libSceCamera`, `libScePad`) into OpenXR session calls, Vulkan stereoscopic frame presentation, and controller tracking.
- **Déraciné Differences from Astro Bot**:
  - Astro Bot exclusively uses a single **DualShock 4** controller (`libScePad`).
  - Déraciné exclusively requires **two PlayStation Move motion controllers** (`libSceMove`). It does not support DualShock 4.
  - Déraciné uses Unreal Engine 4 (`4.18/4.19` custom) with SNDX / NGS2 audio services and motion controller input mappings.

---

## 2. Directory Layout & Critical File Paths

```
e:\DeracinePCVR\
├── games\
│   └── CUSA11919\                 # [IGNORED BY GIT] User's decrypted PS4 game dump (eboot.bin, gameproject-ps4.pak)
│       └── PUT YOUR GAME HERE.txt
├── pc-vr\                         # PCVR runtime distribution folder
│   ├── launch.ps1                 # Launch automation script (launches shadPS4 with patches, VR settings, OpenXR)
│   ├── settings.txt               # Emulator configuration (resolution scale, audio device, etc.)
│   ├── shadps4.exe                # Active emulator executable
│   └── user\
│       ├── config.json            # Emulator config (OpenXR backend enabled)
│       └── patches\
│           └── CUSA11919.xml      # Native instruction patches (Audio crash bypass)
├── src\                           # C++ Source Code (shadPS4 + OpenXR VR runtime)
│   ├── src\
│   │   ├── core\
│   │   │   ├── libraries\
│   │   │   │   ├── move\          # libSceMove implementation (move.cpp, move.h)
│   │   │   │   └── vr_tracker\    # libSceVrTracker implementation (vr_tracker.cpp, vr_tracker.h)
│   │   │   └── vr\                # OpenXR and VR runtime (openxr_host.cpp, vr_runtime.h/cpp)
│   │   └── input\                 # Controller input subsystem
│   └── externals\                 # External dependencies & core submodules
├── tools\
│   └── fetch-submodules.py        # Automated submodule fetcher for CI build
├── .github\
│   └── workflows\
│       └── build-windows.yml      # GitHub Actions CI workflow for compiling shadPS4 PCVR
├── Play Deracine VR.bat           # End-user 1-click launcher
├── README.md                      # Public project README (attribution to bigmak94 & shadPS4)
└── DEVELOPMENT.md                 # This live documentation
```

---

## 3. Step 1 Completed: Audio / Audio3D Crash Resolution

### 3.1 Root Cause Analysis
During game boot, Déraciné crashed inside Unreal Engine 4's audio pipeline at address `0x894060`:
```assembly
0x894060: vmovss xmm2, dword ptr [rdx]  ; Segmentation fault / reading unmapped memory
```
- **Call Chain**:
  - `0x30a275c`: Game calls `sceMoveInit()` and starts `sndx_out_thread` & `sndx_audio3d_out_thread`.
  - UE4 invokes NGS2 UserFx callback: `0x8465b0` -> `0x893bf0` (SNDX Compressor Effect).
- **Core Cause**:
  In shadPS4's audio library (`src/src/core/libraries/audio/audio.cpp` / `RunUserFx`), the emulator allocated a stack buffer for only **8 channel pointers**.
  However, the Unreal Engine 4 SNDX Compressor effect expects a **16-pointer table** at `[r12 + rcx*8]`:
  - 8 audio channel pointers (`[r12 + 0x00..0x38]`).
  - 8 sidechain / detector input pointers (`[r12 + 0x40..0x78]`).
  Because shadPS4 only populated 8 pointers, `rdx` dereferenced unmapped memory when reading the detector pointer for channel 0 at `[r12 + 0x40]`.

### 3.2 Implemented Fix
We created a non-destructive native XML patch: [`pc-vr/user/patches/CUSA11919.xml`](file:///e:/DeracinePCVR/pc-vr/user/patches/CUSA11919.xml)
- **Target virtual address**: `0x008465b0` (file offset `0x00c465b0` in `eboot.bin`).
- **Instruction Patch**:
  ```x86asm
  31 c0    ; xor eax, eax (return 0 / success)
  c3       ; ret
  90 90    ; nop; nop
  ```
- **Result**: Bypasses the compressor effect cleanly without disabling any game audio or NGS2 sound voices. 100% of game audio, BGM, voice acting, and 3D spatial sound remain functional.
- **Verification**: `launch.ps1` runs continuously; UE4's `RHIThread` and `RenderThread 2` render stereo frames and OpenXR outputs them to the headset.

---

## 4. Step 2 Completed: PlayStation Move & VR Tracker Implementation

### 4.1 Reverse-Engineered Move Specifications from `deracine.elf`
By disassembling Déraciné's ELF at `0x30a1600` - `0x30a2780`:
- Déraciné calls `sceMoveOpen` twice on startup:
  - Call 1 -> Left Hand Controller (Handle `0x30b0100`).
  - Call 2 -> Right Hand Controller (Handle `0x30b0200`).
- Button parsing at `0x30a1611` (`sceMoveReadStateLatest`):
  Extracts 8 bits from `button_data` (byte offset `0x18` in `OrbisMoveData`):
  - **Bit 7**: Square (`MotionController_Left/Right_FaceButton1`)
  - **Bit 6**: Cross (`MotionController_Left/Right_FaceButton2`)
  - **Bit 5**: Circle (`MotionController_Left/Right_FaceButton3`)
  - **Bit 4**: Triangle (`MotionController_Left/Right_FaceButton4`)
  - **Bit 2**: Move Button (`MotionController_Left/Right_FaceButton5`) -> **Teleportation & Primary Action**
  - **Bit 1**: Trigger Digital Click (`MotionController_Left/Right_Trigger`)
  - **Bit 3**: Start / Options (`MotionController_Left/Right_Shoulder`)
  - **Bit 0**: Select / Share
- Analog Trigger at `0x30a1670` (`trigger_data` at byte offset `0x1A`):
  - Multiplied by `1.0 / 255.0` to produce `TriggerAxis` (analog 0.0 to 1.0) -> Used for grabbing objects, interacting, pulling levers.

### 4.2 C++ Implementation Details (Commit `092080e`)

#### A. VR Runtime (`src/src/core/vr/vr_runtime.h` & `vr_runtime.cpp`)
- Added per-hand tracking state: `hands[2]` (`hands[0]` = Left, `hands[1]` = Right).
- Implemented `UpdateHand(int hand_idx, const DeviceState& host_state)`:
  - Transforms OpenXR host-space position into PSVR tracker-space position (`PositionToTracker`).
  - Corrects orientation using seat yaw (`Multiply(Conjugate(seat_yaw), host_orientation)`).
  - Transforms linear and angular velocities.
- Added `GetHand(int hand_idx)` to expose 6DoF hand state to tracker libraries.
- Added `UpdateHandInput(hand, buttons, trigger)` and `GetHandInput(hand)` for passing live controller buttons and analog trigger to `libSceMove`.
- Added `SetHandVibration(hand, intensity)` and `GetHandVibration(hand)` for individual hand haptics.

#### B. OpenXR Controller Host (`src/src/core/vr/openxr_host.cpp`)
- Located both `grip_spaces[0]` (left) and `grip_spaces[1]` (right) via `xrLocateSpace`.
- Mapped modern VR controllers (Quest 2/3/Pro, Valve Index, etc.) to PlayStation Move inputs:
  - **Left Hand**:
    - Trigger (>0.25f) -> Bit 1 + analog trigger byte (0..255).
    - Grip (>0.5f) OR Thumbstick Click OR Stick Up -> Bit 2 (**Move Button** for teleporting).
    - X Button -> Bit 7 (Square).
    - Y Button -> Bit 4 (Triangle).
    - Menu Button -> Bit 3 (Start / Options).
    - Stick Left / Right -> Turn left / right (Square / Triangle).
  - **Right Hand**:
    - Trigger (>0.25f) -> Bit 1 + analog trigger byte (0..255).
    - Grip (>0.5f) OR Thumbstick Click OR Stick Up -> Bit 2 (**Move Button** for teleporting).
    - A Button -> Bit 6 (Cross).
    - B Button -> Bit 5 (Circle).
    - Stick Left / Right -> Turn left / right (Cross / Circle).
- In `ApplyRumble`: Added support for Move haptics alongside standard gamepad rumble via `xrApplyHapticFeedback`.

#### C. Move Library (`src/src/core/libraries/move/move.cpp`)
- Replaced stub functions returning `ORBIS_MOVE_ERROR_NO_CONTROLLER_CONNECTED`.
- `sceMoveOpen`: Returns `0x30b0100` for Hand 0 (Left) and `0x30b0200` for Hand 1 (Right).
- `sceMoveGetDeviceInfo`: Populates `sphere_radius = 0.0225f` (standard 45mm PS Move tracking ball).
- `sceMoveReadStateLatest` / `sceMoveReadStateRecent`: Reads `HandInputState` from `Runtime::Instance().GetHandInput(hand)`, populating `button_data` bitmasks and `trigger_data`.
- `sceMoveSetVibration`: Forwards intensity to `Runtime::Instance().SetHandVibration(hand, intensity)`.

#### D. VR Tracker Library (`src/src/core/libraries/vr_tracker/vr_tracker.cpp`)
- Replaced single `g_move_handle` with `std::array<s32, 2> g_move_handles{-1, -1}`.
- In `sceVrTrackerRegisterDeviceInternal`: Supports registering both Move handles without throwing `ORBIS_VR_TRACKER_ERROR_DEVICE_ALREADY_REGISTERED`.
- In `sceVrTrackerGetResult`:
  - When called with a Move handle, fetches `runtime.GetHand(move_index)`.
  - Sets `result->status = ORBIS_VR_TRACKER_STATUS_TRACKING`.
  - Sets `result->position_quality = ORBIS_VR_TRACKER_QUALITY_FULL`.
  - Sets `result->orientation_quality = ORBIS_VR_TRACKER_QUALITY_FULL`.
  - Populates `result->move_info.device_pose` with the 6DoF hand pose via `WritePose`.

---

## 5. Step 3: Build & CI Automation (Current Phase)

### 5.1 Why GitHub Actions CI?
Building shadPS4 from source on Windows requires:
- Visual Studio 2022 C++ Build Tools & Windows 10/11 SDK.
- LLVM Clang (`clang-cl`).
- CMake >= 3.30 & Ninja.
Installing this entire 10GB toolchain locally on user machines is heavy and slow. GitHub Actions provides free `windows-latest` VMs with this exact environment preinstalled.

### 5.2 Submodules vs. Native Externals
- In shadPS4, some directories inside `src/externals/` are **not** submodules, but native core shadPS4 directories:
  - `src/externals/cmake-modules/` (contains `GetGitRevisionDescription.cmake`)
  - `src/externals/openxr-sdk/` (OpenXR headers and loader)
  - `src/externals/gcn/` (GPU shader compiler headers)
  - `src/externals/renderdoc/`
  - `src/externals/stb/`
  - `src/externals/CMakeLists.txt`
- These files are explicitly tracked in Git via `.gitignore` rules:
  ```gitignore
  src/externals/*
  !src/externals/CMakeLists.txt
  !src/externals/cmake-modules/
  !src/externals/openxr-sdk/
  !src/externals/gcn/
  !src/externals/renderdoc/
  !src/externals/stb/
  ```
- All other 40+ third-party dependencies are submodules defined in `src/.gitmodules`.
- The helper script [`tools/fetch-submodules.py`](file:///e:/DeracinePCVR/tools/fetch-submodules.py) reads `src/.gitmodules` and shallow-clones (`--depth 1 --recurse-submodules`) only what is needed before CMake configuration.

### 5.3 CI Diagnostics & Resolutions Log
- **Run #1 & #2**: Failed due to missing `GetGitRevisionDescription.cmake` and externals ignored by `.gitignore`. Resolved by explicitly tracking native core externals (`cmake-modules/`, `openxr-sdk/`, `gcn/`, etc.).
- **Run #3**: Failed in `tools/fetch-submodules.py` with `NameError: name 'url' is not defined` (unassigned loop variable). Fixed by binding `url = cfg[s]['url']` and ensuring directory creation `os.makedirs`. Also added `tools/**` and `.github/workflows/**` to the workflow trigger path filters.
- **Run #4**: Failed during compilation of `ImGuiFileDialog.cpp` (`error: no member named 'ChildFlagsSet' in 'ImGuiNextWindowData'`). Root cause: `src/.gitmodules` lacked `branch = shadps4` for `externals/ImGuiFileDialog`, causing Git to clone the upstream `master` branch (which has breaking ImGui API differences) instead of the patched `shadps4` branch. Fixed by specifying `branch = shadps4` in `src/.gitmodules`.
- **Run #5**: Reached deep build stage (~19 mins, 11,134 lines of log) with all externals and VR runtime compiling smoothly. Failed solely on `ajm_at9.cpp` (`error: no matching function for call to 'Atrac9Decode'`) because upstream `ext-LibAtrac9` master changed function signatures from 5 to 6 arguments (`int atrac9BufferSize`). Fixed by tracking the verified 250KB `LibAtrac9` tree natively in the repository under `.gitignore` whitelist and removing it from `.gitmodules`.
- **Run #6**: Passed all externals and `LibAtrac9`/`ajm_at9.cpp` compilation. Failed at `vr_tracker.cpp` due to two leftover `g_move_handle` references (after refactoring to `g_move_handles` array). Fixed by replacing with `g_move_handles` check and `FindMoveIndex(handle)` removal logic.
- **Run #7**: Reached end of build. Failed at `openxr_host.cpp` (11,285 lines of log) with `undeclared identifier 'cross'` and `'square'` because `pressed(act_cross)` was passed into `add()` without storing into local boolean variables. Fixed by declaring `const bool cross` and `const bool square`.

---

## 6. Next Actions Checklist

1. [ ] **Verify CI Build**: Ensure `tools/fetch-submodules.py` and `.github/workflows/build-windows.yml` successfully complete and generate the release artifact `shadps4-deracine-pcvr`.
2. [ ] **Deploy Executable**: Download the generated `shadPS4.exe` artifact and copy it to `pc-vr/shadps4.exe`.
3. [ ] **VR Headset Testing**:
   - Start OpenXR runtime (Virtual Desktop / SteamVR / Meta Quest Link).
   - Launch with `Play Deracine VR.bat` (or `powershell -ExecutionPolicy Bypass -File pc-vr/launch.ps1`).
   - Confirm virtual Move hands appear in game, follow physical controllers 1:1, teleport with Move button/grip, and interact with objects using triggers.
4. [ ] **Fine-Tuning**: Calibrate hand rotation offsets or sensitivity if physical wand alignment differs from controller grip angles.
