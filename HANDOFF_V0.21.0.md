# Traspaso y Documentación Técnica: Actualización a shadPS4 v0.21.0

> **Fecha**: Noviembre 2026  
> **Rama de trabajo**: `actualizacion-v0.21.0` (origen: `origin/actualizacion-v0.21.0`)  
> **Juego objetivo**: *Déraciné* (CUSA11919) — PlayStation VR exclusivo en PCVR  
> **Versión base upstream**: shadPS4 `v0.21.0` ("Until Mavethee Dawn")  
> **Estado**: Compilación CI en GitHub Actions superada con éxito (Run `41464795210`). Binario desplegado en `pc-vr/shadps4.exe`. Listo para pruebas en visor VR.

---

## 1. Reglas Críticas para Nuevos Agentes

1. **Idioma de comunicación**: Siempre en **español** con el usuario.
2. **Autor y Committer de Git**: Siempre usar obligatoriamente:
   ```bash
   git config user.name "stefan0704"
   git config user.email "26739250+stefan0704@users.noreply.github.com"
   ```
   *(GitHub rechaza pushes si el email no es el noreply oficial de la cuenta, error `GH007`).*
3. **Aislamiento de Ramas**:
   - Todo el trabajo de esta versión está en `actualizacion-v0.21.0`.
   - **NUNCA** modificar ni hacer push directo a la rama `optimizacion-graficos` salvo petición explícita del usuario.
4. **Mapeo de Mandos**:
   - Nunca alterar `swap_hands=1` en `pc-vr/settings.txt`. El juego exige este orden específico para los mandos PS Move emulados.
5. **Enfoque paso a paso**: Un cambio lógico por commit, verificando compilación o comportamiento.

---

## 2. Arquitectura del Proyecto

Déraciné PCVR es un fork especializado de **shadPS4** (heredado inicialmente de AstroQuest por bigmak94) que implementa:
1. **OpenXR PCVR nativo**:
   - Soporte para Meta Quest 2/3/Pro (Virtual Desktop / Meta Link / Steam Link), Valve Index, Bigscreen Beyond, HTC Vive, etc.
   - Traduce `libSceHmd` y `libSceVrTracker` directamente a sesiones de OpenXR y presentación estereoscópica Vulkan.
2. **Emulación 6DoF de PlayStation Move (`libSceMove`)**:
   - A diferencia de otros juegos de PSVR (como Astro Bot que usan DualShock 4), *Déraciné* requiere obligatoriamente **dos mandos PlayStation Move**.
   - Emulación 6DoF con compensación de palanca de la esfera luminosa (`move_pitch=-45°` o `-52°`, compensación de eje $Z$ y posición local para alinear las manos con los mandos físicos del visor).
3. **Pipeline Vulkan y Liverpool**:
   - Presentación de imágenes estéreo mediante `vk_presenter.cpp` y buffers compartidos OpenXR.
   - Gestión de tareas y fences de GPU para evitar deadlocks en `liverpool.cpp`.

---

## 3. Resumen de Cambios y Errores Corregidos (v0.21.0)

Al fusionar la base de código de shadPS4 v0.21.0 con las extensiones VR de Déraciné, surgieron discrepancias de API y dependencias que se solucionaron una a una:

### 3.1. Reestructuración de Módulos VR en CMake (`src/CMakeLists.txt`)
- **Problema**: El linker producía errores de símbolos indefinidos: `Hmd::RegisterLib`, `VrTracker::RegisterLib`, `HmdSetupDialog::RegisterLib`, `Hmd::OnVblank`.
- **Causa**: Había dos sentencias `set(VR_LIBS ...)` consecutivas en `src/CMakeLists.txt`. La segunda sobreescribía a la primera en lugar de concatenarse, eliminando `hmd.cpp`, `hmd_reprojection.cpp`, `hmd_distortion.cpp`, `hmd_setup_dialog.cpp` y `vr_tracker.cpp`.
- **Solución**: Se unificaron en una única lista completa de `VR_LIBS`.

### 3.2. Adaptación de `vk_presenter.cpp` a la API Vulkan de v0.21.0
- **Descargas de textura**: La API `Download()` cambió a `HostCached()`.
- **Transición de imágenes**: `image.Transit(...)` fue reemplazado por `runtime.Transit(...)` seguido de `FlushBarriers()`.
- **Swizzle de componentes**: `vk::ComponentSwizzle` fue sustituido por `AmdGpu::CompSwizzle`.
- **Splash Screen**: La carga de pantalla de inicio cambió de `GetSplashPath()` a `ElfInfo::GetSplashData()` combinado con `RefCountedTexture::DecodePngTexture`.
- **Funciones de depuración/trazado eliminadas en v0.21.0**: Se crearon stubs seguros para `DumpGpuImages`, `TraceSync`, `IsSyncTraceEnabled`, `StartDrawTrace`, `DrawTraceActive`, `EyesUpscaled`, `UpscaledRadius` y `AllowsViewFormat`.

### 3.3. Detección de Hilos GPU y Tareas en Liverpool (`liverpool.h` y `liverpool.cpp`)
- **Problema**: El código VR necesita saber si la llamada se ejecuta desde el hilo gráfico para evitar deadlocks de fences (`IsGpuThread()` e `InGfxTask()`).
- **Solución**: Se restauraron `IsGpuThread()`, `InGfxTask()` y la bandera atómica `in_gfx_task` envolviendo `task.resume()`.

### 3.4. Host Shaders y Push Descriptors en Post-Processing (`pp_pass.cpp`)
- **Problema**: `IsPushDescriptorSupported` y `HostShaders` no existían en v0.21.0.
- **Solución**: Se sustituyó por `instance.MaxPushDescriptors() > 0` y la compilación directa `CompileSPV(FS_TRI_VERT, device)` y `CompileSPV(POST_PROCESS_FRAG, device)`.

### 3.5. Stubs en `vk_gpu_profile.h`
- **Problema**: `GpuProfile::NoteFrameDone()` daba error de símbolo no resuelto en el enlazado.
- **Solución**: Se inlinaron stubs en el encabezado: `NoteFrameDone() {}` y `Enabled() { return false; }`.

### 3.6. Limpieza de Watchdogs Obsoletos en `hmd_reprojection.cpp`
- **Problema**: `Core::GuestCpu::NoteGuestProgress()` ya no existía en el núcleo x86 (era legacy de Android/FEX).
- **Solución**: Se eliminaron las llamadas en las líneas 269 y 366 de `hmd_reprojection.cpp`.

### 3.7. Encabezados y Clases Faltantes
- `stall_log.h`: Restaurado en `src/src/video_core/amdgpu/stall_log.h` y registrado en `CMakeLists.txt`.
- `Core_Vr`: Restaurada la categoría de registro en `classes.h` y `classes.cpp`.
- `controller.h` / `controller.cpp`: Restaurados `SetBlowing`, `SetHeadsetPlays`, `ApplyHeadsetState`.
- `dr_flac.h`: Se des-ignoró en `.gitignore` para permitir incluir el códec de audio FLAC.

---

## 4. Descarga de Artefactos de CI (Solución al Error HTTP 401)

### El Problema
Al descargar un artefacto mediante la API de GitHub (`https://api.github.com/repos/.../actions/artifacts/{id}/zip`), GitHub responde con una redirección HTTP 302 a una URL prefirmada de Azure Blob Storage (`*.blob.core.windows.net`).  
Las librerías HTTP estándar (como `urllib.request` en Python o herramientas que reenvían cabeceras a ciegas) reenvían el encabezado `Authorization: Bearer <github_token>` a Azure. Azure rechaza la petición con:
```
HTTP Error 401: Server failed to authenticate the request. Please refer to the information in the www-authenticate header.
```

### La Solución Implementada
Un script en Python que intercepta la redirección y **elimina la cabecera `Authorization`** al conectar a Azure:
```python
# Ubicación: tools/download-artifact.py (o scratch/download_artifact.py)
class NoRedirectHandler(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None  # Detiene la redirección automática

# Captura el código 302, extrae e.headers.get("Location") y descarga desde Azure sin Authorization
```

---

## 5. Estado Actual del Repositorio

- **Rama actual**: `actualizacion-v0.21.0`
- **Últimos commits**:
  - `e5287acf`: `feat(bin): update pc-vr/shadps4.exe to v0.21.0` (Binario compilado de 60.29 MB colocado en `pc-vr/shadps4.exe`)
  - `0965e6d4`: `fix(hmd): remove obsolete GuestCpu watchdog progress calls in hmd_reprojection`
  - `7d9ca0a7`: `fix(build): restore VR libraries in CMakeLists.txt and inline NoteFrameDone in GpuProfile`
  - `430ccf9a`: `fix(vulkan): update PostProcessingPass to use MaxPushDescriptors and CompileSPV for v0.21.0`
  - `8870ed9c`: `fix(amdgpu): restore IsGpuThread and InGfxTask on Liverpool for present fence handling`
  - `8ddafcde`: `fix(vulkan): stub DumpGpuImages and TraceSync, update splash loading for v0.21.0`
- **Configuración activa en `pc-vr/settings.txt`**:
  - `swap_hands=1` (Intacto)
  - `readbacks=relaxed`
  - Calibración 6DoF de mandos activa

---

## 6. Qué Debe Hacer el Siguiente Agente

Cuando el usuario pruebe el juego ejecutando `Play Deracine VR.bat`:

1. **Si el juego arranca y funciona**:
   - Verificar estabilidad de FPS, sincronización estroboscópica/Vulkan en el visor.
   - Comprobar que los mandos Move sigan respondiendo con la ergonomía calibrada (gatillos, giro snap, menú de objetos en botón X).
   - Comprobar que las caras no sufran glitch de vértices (`readbacks=relaxed`).
2. **Si el juego crashea o congela**:
   - Solicitar o revisar el archivo de log generado en `pc-vr/user/log/shadps4.log`.
   - Analizar si el fallo se produce en:
     - **OpenXR Presentation / Vulkan**: Comprobar si alguno de los stubs en `vk_presenter.cpp` fue llamado durante la sesión de renderizado.
     - **Memory / BufferCache**: Comprobar si `ObtainBuffer` o `ResolveDepthOverlap` requieren sincronización adicional en v0.21.0.
     - **Liverpool / GPU Timeout**: Revisar `stall_log.h` si el hilo GPU se queda bloqueado esperando vblank o swapchain.
3. **Flujo de Modificaciones y Recompilación**:
   - Si se requiere modificar código C++ en `src/`:
     1. Realizar el cambio mínimo necesario.
     2. Hacer commit con el autor `26739250+stefan0704@users.noreply.github.com`.
     3. Pushear a `actualizacion-v0.21.0`.
     4. Esperar el build de GitHub Actions (workflow `build-windows.yml`).
     5. Descargar el nuevo artefacto con `python tools/download-artifact.py`.
     6. Probar de nuevo.
