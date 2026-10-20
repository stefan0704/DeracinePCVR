// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include "common/logging/log.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/time.h"
#include "core/libraries/libs.h"
#include "core/libraries/move/move.h"
#include "core/libraries/move/move_error.h"
#include "core/vr/vr_runtime.h"

namespace Libraries::Move {

static bool g_library_initialized = false;
static constexpr s32 MoveHandles[2] = {0x30b0100, 0x30b0200};
static bool g_opened_handles[2] = {false, false};

s32 PS4_SYSV_ABI sceMoveInit() {
    if (g_library_initialized) {
        return ORBIS_MOVE_ERROR_ALREADY_INIT;
    }
    LOG_INFO(Lib_Move, "Move controllers library initialized");
    g_library_initialized = true;
    g_opened_handles[0] = false;
    g_opened_handles[1] = false;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMoveOpen(Libraries::UserService::OrbisUserServiceUserId user_id, s32 type,
                             s32 index) {
    LOG_DEBUG(Lib_Move, "called: user_id={}, type={}, index={}", user_id, type, index);
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    for (int i = 0; i < 2; ++i) {
        if (!g_opened_handles[i]) {
            g_opened_handles[i] = true;
            LOG_INFO(Lib_Move, "Opened Move controller handle {:#x} (hand {})", MoveHandles[i], i);
            return MoveHandles[i];
        }
    }
    return ORBIS_MOVE_ERROR_MAX_CONTROLLERS_EXCEEDED;
}

static int GetHandIndex(s32 handle) {
    for (int i = 0; i < 2; ++i) {
        if (MoveHandles[i] == handle) {
            return i;
        }
    }
    return -1;
}

s32 PS4_SYSV_ABI sceMoveGetDeviceInfo(s32 handle, OrbisMoveDeviceInfo* info) {
    LOG_TRACE(Lib_Move, "called for handle {:#x}", handle);
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    if (info == nullptr) {
        return ORBIS_MOVE_ERROR_INVALID_ARG;
    }
    const int hand = GetHandIndex(handle);
    if (hand == -1) {
        return ORBIS_MOVE_ERROR_INVALID_HANDLE;
    }
    info->sphere_radius = 0.0225f; // 45mm sphere radius in meters
    info->accelerometer_offset[0] = 0.0f;
    info->accelerometer_offset[1] = 0.0f;
    info->accelerometer_offset[2] = 0.0f;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMoveReadStateLatest(s32 handle, OrbisMoveData* data) {
    LOG_TRACE(Lib_Move, "called for handle {:#x}", handle);
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    if (data == nullptr) {
        return ORBIS_MOVE_ERROR_INVALID_ARG;
    }
    const int hand = GetHandIndex(handle);
    if (hand == -1) {
        return ORBIS_MOVE_ERROR_INVALID_HANDLE;
    }

    std::memset(data, 0, sizeof(OrbisMoveData));
    const auto input = Core::Vr::Runtime::Instance().GetHandInput(hand);
    data->button_data.button_data = input.buttons;
    data->button_data.trigger_data = input.trigger;
    data->timestamp = Libraries::Kernel::sceKernelGetProcessTime();
    data->count = 1;
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMoveReadStateRecent(s32 handle, s64 timestamp, OrbisMoveData* data,
                                        s32* out_count) {
    LOG_TRACE(Lib_Move, "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    if (timestamp < 0 || data == nullptr || out_count == nullptr) {
        return ORBIS_MOVE_ERROR_INVALID_ARG;
    }
    const s32 result = sceMoveReadStateLatest(handle, data);
    if (result == ORBIS_OK) {
        *out_count = 1;
    }
    return result;
}

s32 PS4_SYSV_ABI sceMoveGetExtensionPortInfo(s32 handle, void* data) {
    LOG_TRACE(Lib_Move, "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    if (data == nullptr) {
        return ORBIS_MOVE_ERROR_INVALID_ARG;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMoveSetVibration(s32 handle, u8 intensity) {
    LOG_TRACE(Lib_Move, "called handle {:#x}, intensity={}", handle, intensity);
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    const int hand = GetHandIndex(handle);
    if (hand == -1) {
        return ORBIS_MOVE_ERROR_INVALID_HANDLE;
    }
    Core::Vr::Runtime::Instance().SetHandVibration(hand, intensity);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMoveSetLightSphere(s32 handle, u8 red, u8 green, u8 blue) {
    LOG_TRACE(Lib_Move, "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMoveResetLightSphere(s32 handle) {
    LOG_TRACE(Lib_Move, "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMoveClose(s32 handle) {
    LOG_DEBUG(Lib_Move, "called for handle {:#x}", handle);
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    const int hand = GetHandIndex(handle);
    if (hand >= 0 && hand < 2) {
        g_opened_handles[hand] = false;
    }
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceMoveTerm() {
    LOG_DEBUG(Lib_Move, "called");
    if (!g_library_initialized) {
        return ORBIS_MOVE_ERROR_NOT_INIT;
    }
    g_opened_handles[0] = false;
    g_opened_handles[1] = false;
    g_library_initialized = false;
    return ORBIS_OK;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("j1ITE-EoJmE", "libSceMove", 1, "libSceMove", sceMoveInit);
    LIB_FUNCTION("HzC60MfjJxU", "libSceMove", 1, "libSceMove", sceMoveOpen);
    LIB_FUNCTION("GWXTyxs4QbE", "libSceMove", 1, "libSceMove", sceMoveGetDeviceInfo);
    LIB_FUNCTION("ttU+JOhShl4", "libSceMove", 1, "libSceMove", sceMoveReadStateLatest);
    LIB_FUNCTION("f2bcpK6kJfg", "libSceMove", 1, "libSceMove", sceMoveReadStateRecent);
    LIB_FUNCTION("y5h7f8H1Jnk", "libSceMove", 1, "libSceMove", sceMoveGetExtensionPortInfo);
    LIB_FUNCTION("IFQwtT2CeY0", "libSceMove", 1, "libSceMove", sceMoveSetVibration);
    LIB_FUNCTION("T8KYHPs1JE8", "libSceMove", 1, "libSceMove", sceMoveSetLightSphere);
    LIB_FUNCTION("zuxWAg3HAac", "libSceMove", 1, "libSceMove", sceMoveResetLightSphere);
    LIB_FUNCTION("XX6wlxpHyeo", "libSceMove", 1, "libSceMove", sceMoveClose);
    LIB_FUNCTION("tsZi60H4ypY", "libSceMove", 1, "libSceMove", sceMoveTerm);
};

} // namespace Libraries::Move