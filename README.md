# Déraciné - PC VR

**Déraciné (PlayStation 4 / PlayStation VR)** adapted for PC VR via OpenXR, running on a specialized fork of the [shadPS4](https://github.com/shadps4-emu/shadPS4) emulator.

Play FromSoftware's PSVR adventure with any modern PC VR headset:
- **Meta Quest 3 / Quest 2 / Quest Pro** (via Virtual Desktop VDXR or Meta Quest Link / AirLink)
- **Valve Index / HTC Vive** (via SteamVR OpenXR runtime)
- **Bigscreen Beyond, Pimax**, or any standard PC VR headset with an active OpenXR runtime.

> **NO GAME FILES ARE INCLUDED OR DISTRIBUTED.** You must provide your own legally dumped copy of *Déraciné* (CUSA11919, CUSA10860, CUSA09051, or CUSA11833) from your own PlayStation 4.

---

## Acknowledgements & Credits

This project is directly built upon the pioneering VR and OpenXR architecture created by **bigmak94** in **[AstroQuest](https://github.com/bigmak94/AstroQuest)** (the PC VR and Quest adaptation of *ASTRO BOT Rescue Mission*), as well as the work of the **[shadPS4 emulator project](https://github.com/shadps4-emu/shadPS4)**.

Special thanks and full credit to:
- **[bigmak94 / AstroQuest](https://github.com/bigmak94/AstroQuest)**: For designing and implementing the Vulkan-to-OpenXR presentation pipeline (`openxr_host.cpp`, `vk_vr_exporter.cpp`), PSVR reprojection, optical tangents, and head-tracking bridge for shadPS4.
- **[shadPS4 Development Team](https://github.com/shadps4-emu/shadPS4)**: For developing the open-source PS4 emulator.

**Déraciné PC VR** extends this foundation by:
1. Implementing full **PlayStation Move motion controller emulation** (`libSceMove` and `libSceVrTracker`), as *Déraciné* strictly requires two Move controllers.
2. Mapping modern VR motion controllers (Meta Quest Touch, Valve Index Knuckles) to dual 6DoF Move controllers with haptic feedback.
3. Adding support and fixes for **Unreal Engine 4** under PSVR emulation.

---

## Requirements

1. **PC**:
   - Windows 10 or 11 (64-bit).
   - Dedicated GPU with Vulkan 1.3 support.
   - 16 GB RAM.
   - [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).
2. **VR Headset & OpenXR Runtime**:
   - **Virtual Desktop**: Ensure Virtual Desktop Streamer is active on PC with VDXR set as OpenXR runtime (recommended at 120 Hz).
   - **SteamVR**: Set SteamVR as active OpenXR runtime in SteamVR Settings > OpenXR.
   - **Meta Quest Link**: Set Meta Quest Link as active runtime in Oculus Desktop App Settings > General.
3. **Motion Controllers**:
   - Two VR motion controllers (Meta Quest Touch, Valve Index Knuckles, Vive Wands).

---

## How to Play

1. **Place your game dump** in the `games` folder:
   - Either the extracted game folder (e.g. `games\CUSA11919\` containing `eboot.bin`, `gameproject\`, etc.)
   - Or your dumped `.pkg` package file (the launcher will automatically offer to unpack it).
2. **Start your VR headset** and connect to your PC.
3. **Run `Play Deracine VR.bat`**:
   - Choose your desired resolution and target frame rate.
   - Press **Play**.
4. Put on your headset and pick up both motion controllers.

---

## Controls Mapping (PlayStation Move -> VR Controllers)

*Déraciné* strictly requires two motion controllers:

| PS Move Controller | Quest Touch / Index Knuckles | Function in Déraciné |
|---|---|---|
| **Move Button (T)** | Grip / Primary Trigger | Interact, inspect memories, time watch |
| **Trigger** | Index Trigger | Grab items, confirm menu selections |
| **Face Buttons** | A / B (Right), X / Y (Left) | Cross, Circle, Square, Triangle |
| **Options / Start** | Menu Button (Left controller) | In-Game Pause / Options |
| **Tracking** | 6DoF OpenXR Aim & Grip Poses | Hand position and orientation |

---

## Development & Technical Details

For comprehensive developer documentation, reverse engineering findings, memory offsets, and architecture details, see:
👉 **[DEVELOPMENT.md](DEVELOPMENT.md)**

