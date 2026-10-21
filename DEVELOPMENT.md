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
- **Run #8 (SUCCESS)**: Clean build of the entire project! Ninja compiled all units with `ccache`, linked `shadPS4.exe`, and successfully generated the release artifact `shadps4-deracine-pcvr`.

---

## 7. Step 4 Completed: 2D VR Reprojection Implementation (`sceHmdReprojectionStart2dVr`)

### 7.1 Symptom & Log Diagnosis
When booting *Déraciné* in the Quest headset via Virtual Desktop:
- 100% of game music, audio, and spatial sound played flawlessly.
- OpenXR tracked the head at 90.0 fps (`head tracked 100% of the time`).
- Hand controllers were tracked at 100% (`Controllers: standing in for the gamepad; the right one tracked 100% of the time`).
- **PROBLEM**: The screen inside the headset remained pitch black, and the OpenXR log reported:
  `Headset: the title delivered 0.0 frames a second (0 ms the longest wait for one, 0 waits of more than 50 ms), 0.0 were shown`.
- **Root Cause Uncovered**:
  In `shad_log.txt`:
  `[Lib.Hmd] <Error> (RHIThread) hmd_reprojection.cpp:277 sceHmdReprojectionStart2dVr: (STUBBED) called`
  was logged hundreds of times every second!

### 7.2 Root Cause Analysis
- In *Astro Bot*, the entire game exclusively renders in 3D VR reprojection via `sceHmdReprojectionStart` (`dntZTJ7meIU`).
- In *Déraciné* (Unreal Engine 4), splash logos, opening cinematics, menus, and 2D overlays use **2D VR theater / reprojection** via `sceHmdReprojectionStart2dVr` (`q3e8+nEguyE`).
- Because `sceHmdReprojectionStart2dVr` in shadPS4 was an empty stub returning `ORBIS_OK`, the emulator was throwing away every single 2D frame without delivering it to `Libraries::VideoOut::SubmitHmdFrame`.
- When the user pressed the Virtual Desktop menu button (3 horizontal lines on left controller), Virtual Desktop's dashboard overlay toggled, triggering session state transitions while 0 frames had been presented, leading to session termination.

### 7.3 Implemented Fix
1. **ELF Disassembly of UE4 Call Site (`0xcdb240 - 0xcdb2b4`)**:
   - Reconstructed the `OrbisHmdReprojection2dVrParam` struct:
     - `texture`: Guest `sce::Gnm::Texture*` (`AmdGpu::Image*`).
     - `sampler`: Guest sampler descriptor (`0x3531c40`).
     - `uv`: View UV coordinates (`scale_x, scale_y, offset_x, offset_y`).
     - `frame_label`: Sync label pointer.
     - `timeout`: 0xfa0 (4000).
   - Calling convention: `sceHmdReprojectionStart2dVr(param, flip_arg, option)` where `rsi = flip_arg` and `edx = option`.
2. **Implementation in `hmd_reprojection.cpp`**:
   - Copies the 2D frame texture to both eye slots (`frame.eye_textures[0]` and `frame.eye_textures[1]`).
   - Computes FOV from `param->uv` via `FovFromUv(param->uv)` with fallback to headset native FOV (`Runtime::Instance().GetConfig().fov`).
   - Retrieves the current head pose (`Runtime::Instance().GetHead().pose`) for stable reprojection.
   - Forwards to `Libraries::VideoOut::SubmitHmdFrame(video_out_handle, frame)`.
   - The presenter blits the frame to the OpenXR swapchain, delivering the 2D splash screens, logos, and title menu in the headset at full framerate.

---

## 8. Step 5 Completed: 3D VR Reprojection Implementation (`sceHmdReprojectionStartWithOverlay`)

### 8.1 Symptom & Log Diagnosis
After implementing `sceHmdReprojectionStart2dVr`:
- The user reported seeing initial frames ("unas particulas como si fuera una pantalla muy pequeña") during the first few seconds, but the display quickly returned to black while music continued to play.
- **Log Investigation of `shad_log.txt`**:
  - `sceHmdReprojectionStart2dVr` executed only for the initial ~9 frames during early engine boot.
  - As soon as Déraciné entered the main 3D world, a new dedicated thread spawned: `(ReprojectionAsyncTaskThreadPS4)`.
  - This thread called `sceHmdReprojectionStartWithOverlay` (`kcldQ7zLYQQ`) **5,033 times**!
  - `sceHmdReprojectionStartWithOverlay` in shadPS4 was an empty stub returning `ORBIS_OK`!
  - Every single 3D VR gameplay frame of Déraciné was being discarded.

### 8.2 Disassembly & Parameter Reconstruction
In `deracine.elf` at `0xcdd390` - `0xcdd5a5`:
- `rdi`: `const OrbisHmdReprojectionParam* param`
  - Offset `0x00`: Left eye texture pointer (`texture[0]`).
  - Offset `0x08`: Right eye texture pointer (`texture[1]`).
  - Offset `0x10`: GNM Sampler descriptor.
  - Offset `0x18..0x37`: UV coordinates for left and right eye (`scale_x, scale_y, offset_x, offset_y`).
- `rsi`: `const OrbisHmdReprojectionTrackerState* tracker_state` (6DoF head pose).
- `rdx`: `s64 flip_arg` (frame sequence).
- `rcx`: `const void* overlay_param` (optional 2D UI / HUD overlay layer, `nullptr` when no overlay).
- `r8`: `s32 option`.

### 8.3 Implemented Fix
1. **`hmd_reprojection.cpp`**:
   - Implemented `sceHmdReprojectionStartWithOverlay`, forwarding directly to `sceHmdReprojectionStart(param, tracker_state, flip_arg, option)` to queue the full stereoscopic 3D frame to `Libraries::VideoOut::SubmitHmdFrame`.
   - Also implemented `sceHmdReprojectionStartWideNear` and `sceHmdReprojectionStartWideNearWithOverlay` to forward to `sceHmdReprojectionStart`.
   - In `sceHmdReprojectionStart2dVr`, set `frame.fov = Core::Vr::Runtime::Instance().GetConfig().fov;` directly, preventing the 2D intro screen from appearing shrunken or distorted.

---

## 9. Step 6 Completed: Level Loading Crash Fix (`BufferCache::ObtainBuffer` & `MemoryManager`)

### 9.1 Symptom & Crash Log Analysis
Upon booting *Déraciné*, the user was able to:
- See the main title menu in full VR inside the headset.
- Change language settings using the VR Move motion controls.
- Start a new game.

However, during level streaming / transition to the gameplay map, the emulator crashed with an Access Violation (`0xC0000005`):
```text
[Render] <Warning> (shadPS4:GpuCommandProcessor) page_manager.cpp:254 UpdatePageWatchers: Tracking memory region 0x2034b2b000 - 0x2034b31000 which is not fully GPU mapped.
[Debug] <Critical> (shadPS4:GpuCommandProcessor) signals.cpp:195 SignalHandler: Unhandled Exception code 0xc0000005 at 0x7ff81333bc5b
  VCRUNTIME140.dll + 0x1bc5b (memcpy)
  shadps4.exe + 0xa2f6fa (BufferCache::ObtainBuffer)
  shadps4.exe + 0xa2ed49 (BufferCache::BindVertexBuffers)
  shadps4.exe + 0xaef1e5 (Liverpool / GpuCommandProcessor)
```

### 9.2 Root Cause Analysis
1. In Unreal Engine 4, the CPU render/resource threads commit memory pools asynchronously via `sceKernelMemoryPoolCommit` (in 64KB chunks).
2. During level loading, vertex buffers and uniform buffers are allocated in pooled memory. A vertex buffer range was placed at `0x2034b2b000 - 0x2034b31000`. The memory up to `0x2034b30000` was committed (`Mapped`), but `0x2034b30000 - 0x2034b31000` was still `PoolReserved` (uncommitted memory in the host address space).
3. In `MemoryManager::ClampRangeSize`, an optimization `static constexpr u64 MinSizeToClamp = 1_GB;` was bypassing clamping for any buffer under 1GB, returning the unclamped `size = 0x6000` directly.
4. In `BufferCache::ObtainBuffer` and `BufferCache::ObtainUniformBuffer`, small read-only buffers (`size <= CACHING_PAGESIZE`) were copied via `stream_buffer.Copy` and direct `std::memcpy(data, reinterpret_cast<const void*>(device_addr), size);`.
5. When `std::memcpy` crossed the boundary at `0x2034b30000` into uncommitted memory, Windows triggered `0xC0000005: EXCEPTION_ACCESS_VIOLATION`. Because `shadPS4` registers a Vectored Exception Handler (VEH) with `FirstHandler = 1`, the VEH intercepted the page fault before SEH, identified it as an unhandled access to uncommitted memory, and immediately terminated the process.

### 9.3 Implemented Fix
1. **`src/src/core/memory.cpp`**:
   - Hardened `MemoryManager::ClampRangeSize` to validate address mappings and contiguous mapped VMAs for all requested buffer sizes, ensuring buffers never claim bounds beyond committed memory.
   - Hardened `MemoryManager::CopySparseMemory` against unmapped gaps, out-of-bounds addresses, and `vma_map.end()`, guaranteeing that unmapped/uncommitted pages are safely zero-filled with `std::memset` without triggering hardware access violations.
2. **`src/src/video_core/buffer_cache/buffer_cache.cpp`**:
   - Replaced raw `std::memcpy` calls in `BufferCache::ObtainBuffer` and `BufferCache::ObtainUniformBuffer` with `memory->CopySparseMemory(device_addr, data, size)`. This ensures that even if a guest draw command references partially committed or sparse pooled pages during level streaming, the host copy never faults.

---

## 10. Step 7 Completed: Object Interaction Crash Fix (`TextureCache::ResolveDepthOverlap`) & Quest Grip Grab Mapping

### 10.1 Symptom & Log Diagnosis
Upon reaching in-game 3D VR gameplay:
- The user successfully transitioned past the initial loading screen into the 3D world.
- The user walked over to an interactable object and grabbed it.
- **Button Mapping Issue**: The user reported that grabbing was bound to the index trigger instead of the Quest controller grip button (hand squeeze).
- **Crash Symptom**: Approximately one second after grabbing the object, the emulator crashed:
  ```text
  [Debug] <Critical> (shadPS4:GpuCommandProcessor) signals.cpp:195 SignalHandler: Unhandled Exception code 0xc0000005 at 0x700000b80e57
    shadps4.exe + 0xb80e57 (TextureCache::ResolveDepthOverlap)
    shadps4.exe + 0xb82bde (TextureCache::ResolveOverlap)
    shadps4.exe + 0xb87f75 (TextureCache::FindOrCreateView)
    shadps4.exe + 0xaed994 (Liverpool / GpuCommandProcessor)
  ```

### 10.2 Root Cause Analysis
1. **Dangling Reference in `TextureCache::ResolveDepthOverlap`**:
   - In `texture_cache.cpp` at line 301, `auto& cache_image = slot_images[cache_image_id];` was captured by reference.
   - When object highlight/stencil/depth passes executed upon grabbing an object, `recreate = true` was triggered.
   - At line 348, `slot_images.insert(...)` was called to create the new depth image. When `slot_images` (a `Common::SlotVector<Image>`) expanded its capacity, it reallocated its heap storage and freed the previous buffer.
   - On the very next line (`new_image.usage = cache_image.usage;`), the code dereferenced `cache_image` (`r14 + 0x2b0`), accessing the freed memory block. This caused an immediate Access Violation `0xC0000005` at `0xb80e57`.
2. **Move Controller Input Mapping**:
   - In `openxr_host.cpp`, Quest index trigger was mapped to PS Move trigger (bit 1 + analog trigger) and Quest grip was mapped to PS Move button (bit 2).
   - In VR conventions and *Déraciné*, players expect the hand grip button to perform the physical grabbing action (PS Move trigger), and the index trigger/stick to perform movement/teleportation (PS Move button).

### 10.3 Implemented Fix
1. **`src/src/video_core/texture_cache/texture_cache.cpp`**:
   - In `ResolveDepthOverlap`, re-fetched `source_image` from `slot_images[cache_image_id]` after `slot_images.insert` and updated all subsequent references, eliminating the dangling reference.
   - In `FindDepthTarget`, re-fetched `image` from `slot_images[image_id]` after `slot_images.insert` for stencil attachment linking.
2. **`src/src/core/vr/openxr_host.cpp`**:
   - Mapped Quest controller Grip (`left_grip` / `right_grip`) to PS Move Trigger (`left_move_trigger` / `right_move_trigger` analog and bit 1 digital click), enabling intuitive hand grab/squeeze interaction.
   - Mapped Quest controller Trigger (`left_trigger` / `right_trigger`) to PS Move Button (`(1 << 2)`), enabling natural index finger point, teleport, and menu selection.

---

## 11. Step 8 Completed: Ergonomic Quest Controller Scheme for *Déraciné* (Move Emulation)

### 11.1 Context & Hardware Requirements
*Déraciné* was designed for two PlayStation Move controllers which lacked analog joysticks:
- Camera snap-turning was executed via the face buttons on the Right Move controller (Cross = Turn Left, Circle = Turn Right).
- Movement / Teleportation was executed via the center Move button on the Right Move controller.
- Crouching was executed via the center Move button on the Left Move controller.
- Object grabbing was executed via the Move Trigger (analog + digital click).
- Inspecting held item details was executed via the Square button on the hand holding the item.
- Inventory was toggled via the Cross button on the Left Move controller.

### 11.2 Ergonomic Quest Mapping Implemented
In `src/src/core/vr/openxr_host.cpp`:
- **Left Quest Controller**:
  - **Grip**: Left Move Trigger (`trigger_data` + `1 << 1`) $\rightarrow$ Grab items in left hand.
  - **Index Trigger OR Thumbstick Click**: Left Move Center Button (`1 << 2`) $\rightarrow$ Crouch / Agacharse.
  - **Button X** (`circle`): Left Move Cross (`1 << 6`) $\rightarrow$ Object Menu / Inventory (Menú de Objetos - single toggle).
  - **Button Y** (`triangle`): Left Move Square + Triangle (`(1 << 7) | (1 << 4)`) $\rightarrow$ Time Watch (Reloj de Tiempo) / Inspect left item.
  - **Menu Button**: Left Move Start/Options (`1 << 3`).
- **Right Quest Controller**:
  - **Grip**: Right Move Trigger (`trigger_data` + `1 << 1`) $\rightarrow$ Grab items in right hand.
  - **Index Trigger OR Left Thumbstick UP (`move.y > 0.5f`)**: Right Move Center Button (`1 << 2`) $\rightarrow$ Move / Teleport.
  - **Thumbstick Horizontal (`finger.x`)**:
    - Left (`finger.x < -0.5f`): Right Move Cross (`1 << 6`) $\rightarrow$ Snap Turn Left.
    - Right (`finger.x > 0.5f`): Right Move Circle (`1 << 5`) $\rightarrow$ Snap Turn Right.
  - **Button B or A** (`square` or `cross`): Right Move Square + Triangle (`(1 << 7) | (1 << 4)`) $\rightarrow$ Inspect held item in right hand / interact.
  - **Thumbstick Click / Up**: Disabled.

---

## 12. Step 9 Completed: Move 6DoF Calibration (Pitch Angle & Hand Position Offset) & Inventory Fix

### 12.1 Diagnosis from Quest In-Headset Screenshots
1. **Ray Angle ("Rayo")**: The in-game blue interaction ray projected upwards at a ~35°–40° angle instead of straight forward, because OpenXR `grip/pose` represents the angled grip of the Quest controller inside the fist, whereas *Déraciné* assumed the longitudinal axis of a PlayStation Move wand.
2. **Hand Offset ("Manos")**: The in-game fairy hand models rendered 6–8 cm below the physical Quest controllers because Sony PSVR tracking tracked the spherical glowing bulb located at the top of the Move wand (~15 cm above the grip).
3. **Inventory Bounce**: Sending `(1 << 6) | (1 << 5)` simultaneously on Quest X caused *Déraciné* to toggle and cancel/re-toggle every frame, causing the menu to bounce back open when trying to close.

### 12.2 Implemented Fix
1. **Move 6DoF Pose Calibration in `openxr_host.cpp`**:
   - **Local Position Offset**: Added $+12.0\text{ cm}$ vertical ($+Y$) and $+6.5\text{ cm}$ forward ($-Z$) translation in the controller's local orientation frame (`Rotate(orientation, local_offset)`), lifting the in-game hands directly into the physical Quest controller grip handle.
   - **Local Pitch Rotation**: Applied a $-52^\circ$ pitch rotation around local $X$ (`rot_pitch = (sin(θ/2), 0, 0, cos(θ/2))`), bringing the interaction ray down to align parallel with the Quest pointer.
   - **Configurability**: Exposed `move_pitch=-52`, `move_y=0.120`, `move_z=-0.065`, `move_x=0.0` directly in `settings.txt` (and parsed via `launch.ps1` into `SHADPS4_MOVE_*` env vars) so any further fine-tuning takes effect instantly on launch.
2. **Clean Inventory Input**: Set Quest X exclusively to `(1 << 6)` (Cross), ensuring crisp, single-action open and close.

---

## 13. Step 10 Completed: Eliminating Move Wand Lever Arm via Pre-Rotated Sphere Projection

### 13.1 Diagnosis from Side-View Screenshot (Pointing Left)
When pointing forward, the hand model appeared relatively close. However, when turning the wrist 90° to point to the left, the virtual hand detached and floated ~20–25 cm to the right in the air.
- **Root Cause Uncovered**:
  - The Sony PS Move tracking system reports the 3D position of the **illuminated glowing sphere** at the tip of the wand.
  - *Déraciné* (UE4) assumes the received `device_pose` is the Sphere, and internally applies a fixed backwards translation vector along the wand's axis ($-15\text{ cm}$) to position the hand model on the handle.
  - In our previous implementation:
    1. The position offset was rotated by the raw Quest grip orientation *before* pitch rotation was applied.
    2. The position fed to Déraciné was the palm, not the sphere.
    3. When the game engine subtracted 15 cm backwards along the pitch-rotated wand, turning the controller left rotated this vector to the right, creating a 25 cm lever arm arc.

### 13.2 Implemented Fix
In `src/src/core/vr/openxr_host.cpp`:
1. **Rotation First**: Applied the pitch rotation ($-45^\circ$) to `hand_state.pose.orientation` *first*, obtaining the true orientation of the Move wand and aligning the blue interaction ray with the Quest pointer.
2. **Sphere Projection**: Projected `hand_state.pose.position` forward along this rotated wand axis by $+13.5\text{ cm}$ ($-Z = -0.135\text{ m}$) and $0.0\text{ cm}$ vertical ($+Y = 0.000\text{ m}$) to the virtual Move Sphere, placing the in-game hand model directly on the Quest controller grip.
3. **Exact Cancellation**: When Déraciné applies its internal backwards offset from the sphere along the wand axis, both $+13.5\text{ cm}$ and $-13.5\text{ cm}$ cancel out identically in any orientation ($\text{Palm} + R\cdot\vec{v} - R\cdot\vec{v} = \text{Palm}$). The virtual hand remains permanently locked to the physical Quest controller regardless of wrist roll, pitch, or yaw.

---

## 14. Step 12 Completed: Visual Polish (Face Morphs & 2D Loading Screens)

### 14.1 Character Face Geometry Collapse ("Vertex Explosion")
- **Symptom**: Black geometric holes / cutouts on character faces (such as over the eyebrows, nose, mouth, and eyelids) that appear and disappear as characters speak or animate.
- **Root Cause**: In Unreal Engine 4 on PS4, facial animations rely on GPU-written Morph Targets / Blend Shapes. With `readbacks_mode: 0` (`Disabled`) in `config.json`, shadPS4 does not synchronize GPU-modified vertex buffer ranges back to CPU memory, causing vertices to desync and collapse inside the head mesh.
- **Fix**:
  1. Enabled **Relaxed Readbacks** (`readbacks_mode: 1`) in `pc-vr/user/config.json`.
  2. Exposed `readbacks=relaxed` in `pc-vr/settings.txt`.
  3. Added automatic sync in `pc-vr/launch.ps1` so `readbacks_mode` is always kept active on every launch.

### 14.2 2D VR Loading Screen Double Vision
- **Symptom**: During loading screens, each eye displayed two duplicate flat screens side-by-side.
- **Root Cause**: In Déraciné (UE4), the engine outputs 2D loading screens into the stereoscopic side-by-side render target (Left eye in left half, Right eye in right half). In `sceHmdReprojectionStart2dVr`, `frame.fov` was previously hardcoded to the headset's symmetric native FOV (`GetConfig().fov`). Because symmetric FOV encompasses the full texture, the entire side-by-side image was displayed inside both the left and right eyes.
- **Fix**:
  - Restored `frame.fov = FovFromUv(param->uv)` in `sceHmdReprojectionStart2dVr`.
  - The asymmetric UV offsets (`scale_x ~ 0.225`, `offset_x ~ 0.31`) cleanly project only the left half to the left eye and only the right half to the right eye, fusing both into one centered, comfortable 2D cinema screen without double vision.

---

## 15. Next Actions Checklist

1. [x] **Verify CI Build**: Completed in Run #8 (`39511236719`). Generated `shadps4-deracine-pcvr` artifact.
2. [x] **Fix 2D VR Intro/Menus**: Implemented `sceHmdReprojectionStart2dVr`.
3. [x] **Fix 3D VR Gameplay (5000+ frames)**: Implemented `sceHmdReprojectionStartWithOverlay` in `hmd.h` and `hmd_reprojection.cpp`.
4. [x] **Fix Level Loading Crash (0xC0000005 in ObtainBuffer)**: Implemented `CopySparseMemory` in `BufferCache::ObtainBuffer` & `ObtainUniformBuffer`, hardened `ClampRangeSize` in `MemoryManager`.
5. [x] **Fix Object Grab Crash (0xC0000005 in ResolveDepthOverlap)**: Re-fetched references in `TextureCache::ResolveDepthOverlap` and `FindDepthTarget`.
6. [x] **Fix Quest Controller Grab Mapping**: Mapped Grip to Grab (Move Trigger).
7. [x] **Full Ergonomic Control Scheme for Déraciné**: Implemented camera snap on right stick, move on right trigger & left stick up, crouch on left trigger & left stick click, inventory on Quest X, inspect on Quest Y/B.
8. [x] **Move 6DoF Calibration & Single-Input Inventory**: Applied pitch rotation (-52°) and hand position offset, fixed Quest X inventory bounce.
9. [x] **Runtime Config in settings.txt**: Enabled live configuration of `move_pitch`, `move_y`, `move_z` in `settings.txt`.
10. [x] **Eliminate Lever Arm via Sphere Projection**: Reordered rotation first, then forward projection to Move Sphere (`offset_z=-0.135`, `offset_y=0.000`).
11. [x] **Branch Isolation (`optimizacion-graficos`)**: Clean branch established for graphics and rendering polish.
12. [x] **Fix Face Glitches**: Relaxed readbacks (`readbacks_mode: 1`).
13. [x] **Fix Loading Screen Double Vision**: Asymmetric `FovFromUv` in `sceHmdReprojectionStart2dVr`.
14. [ ] **In-Headset Verification**: Verify loading screen shows a single centered cinema display and character faces have no black cutouts.

