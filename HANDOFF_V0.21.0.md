# Guía Maestra de Traspaso y Optimización (shadPS4 v0.21.0 — Déraciné PCVR)

> **Fecha de actualización**: 9 de noviembre de 2026  
> **Rama activa de trabajo (Optimización)**: `optimizacion-rendimiento-v0.21.0`  
> **Rama de respaldo intocable (Golden Build)**: `actualizacion-v0.21.0` (commit `1d2f3936`)  
> **Release publicada**: `v0.21.0-beta` (`DeracinePCVR-v0.21.0-beta.zip`)  
> **Propósito de este documento**: Servir de guía técnica exhaustiva y paso a paso para que un modelo más ligero y económico pueda seguir optimizando el rendimiento (FPS y tiempos de carga) y la calidad visual de *Déraciné* sin romper ninguna de las correcciones gráficas ya logradas.

---

## 1. Reglas Inviolables para el Agente

1. **Idioma**: Hablar siempre en **español** con el usuario.
2. **Identidad Git (Obligatorio antes de cualquier commit)**:
   ```powershell
   git config user.name "stefan0704"
   git config user.email "26739250+stefan0704@users.noreply.github.com"
   ```
3. **Control Estricto de Ramas**:
   - **TRABAJAR SIEMPRE EN**: `optimizacion-rendimiento-v0.21.0`.
   - **PROHIBIDO TOCAR `actualizacion-v0.21.0`**: Esa rama guarda el estado dorado (`1d2f3936`) donde todas las texturas, caras, flores y uvas funcionan al 100%. Es nuestro punto de restauración seguro.
   - **PROHIBIDO TOCAR `optimizacion-graficos`**.
4. **Comandos en PowerShell**:
   - En Windows PowerShell no uses `&&` para encadenar comandos (da error de sintaxis); usa `;` o ejecuta un script `.py` en la carpeta `scratch`.

---

## 2. Qué Hemos Arreglado y NO SE DEBE ROMPER JAMÁS

Durante la depuración de v0.21.0 resolvimos un fallo crítico donde la cara de la chica, la flor y las uvas tenían agujeros transparentes, triángulos negros o desaparecían al animarse. **La causa raíz era una condición de carrera en GPU entre 51 Compute Shaders (`DispatchDirect`) que acumulaban morph targets sobre el mismo buffer (`0x2038a80000`) sin barreras de memoria Vulkan entre sí, sumada a `NaN`s por división entre cero en pesos de morph nulos.**

### Archivos y bloques que NUNCA debes eliminar ni debilitar:

1. **[`src/src/video_core/buffer_cache/buffer.h`](file:///e:/DeracinePCVR/src/src/video_core/buffer_cache/buffer.h#L127-L164) (`Buffer::GetBarrier`)**:
   - La condición de salida temprana **DEBE** seguir siendo:
     ```cpp
     if (!src_is_write && !dst_is_write && dst_acess_mask == access_mask && stage == dst_stage) {
         return {};
     }
     ```
   - **Por qué**: Cuando un Compute Shader escribe en un buffer (`dst_is_write == true`) y el siguiente Compute Shader también escribe/lee en ese mismo buffer (`src_is_write == true`, `dst_is_write == true`), `dst_acess_mask == access_mask` es verdadero. Si quitas `!src_is_write && !dst_is_write`, Vulkan omite la barrera entre los 51 Compute Shaders y se vuelven a romper la cara de la chica, la flor y las uvas.

2. **[`src/src/video_core/renderer_vulkan/vk_rasterizer.cpp`](file:///e:/DeracinePCVR/src/src/video_core/renderer_vulkan/vk_rasterizer.cpp#L1201-L1224) (`Rasterizer::BindBuffers`)**:
   - Un mismo Compute Shader enlaza el mismo buffer (`0x2038a80000`) dos veces en la misma etapa (por ejemplo, slot 0 como `is_written=true` y slot 1 como `is_written=false`).
   - El bucle `first_for_buffer` / `any_written` garantiza que solo se llame a `vk_buffer->GetBarrier(...)` **una vez por `BufferId` único en cada etapa**, usando `eShaderRead | eShaderWrite` si cualquiera de sus bindings en esa etapa tiene `is_written == true`. **No elimines nunca esta deduplicación**.

3. **[`src/src/shader_recompiler/backend/spirv/emit_spirv_floating_point.cpp`](file:///e:/DeracinePCVR/src/src/shader_recompiler/backend/spirv/emit_spirv_floating_point.cpp#L140-L178) (`EmitFPRecip32`, `EmitFPRecipSqrt32`, `EmitFPSqrt`)**:
   - En GCN (PS4), `v_rsq_f32(0.0)` y `v_rcp_f32(0.0)` no corrompen los vértices como ocurre en Vulkan con `FastMath` en GPUs NVIDIA.
   - Mantén siempre los `OpFMax(..., 1e-30f)` y `OpSelect(OpFOrdEqual(v, 0.0f), 0.0f, ...)`. **Nunca uses `ctx.OpIsNan` ni `ctx.OpIsInf`** porque con `ExecutionModeSignedZeroInfNanPreserve` desactivado el driver de NVIDIA los optimiza a `false`.

4. **[`src/src/core/vr/hmd.cpp`](file:///e:/DeracinePCVR/src/src/core/vr/hmd.cpp#L283-L304) (Reloj de consola a 60 FPS e intercambio de manos con `L3`)**:
   - El motor de *Déraciné* está diseñado para simularse a **60 FPS fijos** (`kNativeFrameUs = 16667`, `native_frame_rate = 0`), mientras que el visor VR puede ir a 72, 80, 90 o 120 Hz mediante reproyección en `hmd_reprojection.cpp`. **No desbloquees los FPS internos de la consola por encima de 60**.
   - El botón `L3` (pulsar el joystick izquierdo) intercambia las manos en vivo (`ToggleRuntimeHandSwap()`) y el botón `R3` vuelca los render targets en `pc-vr/rt_dump/capture_XX`.

---

## 3. Qué Optimizaciones Ya Están Aplicadas (Commit `26e4a83f`)

1. **Inicialización de `Buffer::access_mask` a solo lectura en [`buffer.h:181-185`](file:///e:/DeracinePCVR/src/src/video_core/buffer_cache/buffer.h#L181-L185)**:
   - Como `BufferCache::SynchronizeBuffer` ya emite su propio `pre_barrier` y `post_barrier` fuera del RenderPass al subir datos desde CPU, inicializar `access_mask` a `all_read_flags` evita que la primera lectura de cualquier buffer estático subido por CPU emita una barrera redundante y llame a `scheduler.EndRendering()`.
2. **Guardas `IsRegionGpuModified` restauradas en [`buffer_cache.cpp:256,323`](file:///e:/DeracinePCVR/src/src/video_core/buffer_cache/buffer_cache.cpp#L256-L328)**:
   - `BindVertexBuffers` y `BindIndexBuffer` solo piden barrera si el buffer fue modificado por la GPU (`IsRegionGpuModified == true`, que siempre es cierto para los buffers de morph targets escritos por Compute Shaders).
3. **Caché de Pipelines de Vulkan activada y persistente en [`pc-vr/launch.ps1:931-942`](file:///e:/DeracinePCVR/pc-vr/launch.ps1#L931-L942)**:
   - Ya no se borra `user\cache\CUSA11919` en cada arranque y `pipeline_cache_enabled = true`.
4. **Ajustes en [`pc-vr/settings.txt`](file:///e:/DeracinePCVR/pc-vr/settings.txt)**:
   - `resolution=2160`, `msaa=4`, `sharpen=0.35`, `copy_gpu_buffers=0`, `swap_hands=1`, `fps=60`.

---

## 4. Hoja de Ruta Paso a Paso para Seguir Optimizando (Para el Modelo Ligero)

Aplica estas optimizaciones **de una en una**, haciendo commit, push a `optimizacion-rendimiento-v0.21.0`, esperando a que GitHub Actions compile (`build-windows.yml`) y descargando el ejecutable con `python tools/download-artifact.py`.

---

### Optimización Paso 1 (IMPACTO MUY ALTO EN CPU Y GPU): Evitar `FindBuffer` / `CreateBuffer` para buffers pequeños de solo lectura en `Rasterizer::BindBuffers`

#### Diagnóstico exacto
En [`src/src/video_core/renderer_vulkan/vk_rasterizer.cpp:1126-1135`](file:///e:/DeracinePCVR/src/src/video_core/renderer_vulkan/vk_rasterizer.cpp#L1126-L1135), `Rasterizer::BindBuffers` hace dos pasadas sobre `stage.buffers`:
- En la **primera pasada** (línea 1130), llama incondicionalmente a `buffer_cache.FindBuffer(vsharp.base_address, size)` para **todos** los buffers de cada draw call.
- Sin embargo, en la **segunda pasada** (líneas 1192-1195), llama a `buffer_cache.ObtainBuffer` o `buffer_cache.ObtainUniformBuffer`.
- Si miras [`buffer_cache.cpp:451`](file:///e:/DeracinePCVR/src/src/video_core/buffer_cache/buffer_cache.cpp#L451) y [`buffer_cache.cpp:480`](file:///e:/DeracinePCVR/src/src/video_core/buffer_cache/buffer_cache.cpp#L480), cuando un buffer es de **solo lectura, pequeño (`size <= CACHING_PAGESIZE`, es decir `<= 16 KB`) y no ha sido modificado por GPU (`!IsRegionGpuModified`)**, `ObtainBuffer` **NO usa el `buffer_id` de `slot_buffers`**, sino que copia los datos directamente al `stream_buffer` (un ring buffer rápido mapeado en memoria).
- **El problema**: Como la primera pasada de `BindBuffers` llamó a `FindBuffer(vsharp.base_address, size)` sin comprobar eso, `FindBuffer` llama a `CreateBuffer` para cada constante temporal que Unreal Engine 4 escribe en su ring buffer de CPU, lo cual:
  1. Crea `VkBuffer`s inútiles en VMA que jamás se usan.
  2. Ejecuta `JoinOverlap` (`buffer_cache.cpp:946`), que llama a **`scheduler.EndRendering()`** rompiendo el RenderPass en mitad del fotograma.
  3. Registra las páginas del ring buffer de CPU en `memory_tracker` activando protección de escritura (`VirtualProtect`), lo que provoca **miles de excepciones `STATUS_ACCESS_VIOLATION` (Page Faults) por segundo en Windows** cuando la CPU del juego escribe las constantes del siguiente fotograma.

#### Cómo implementarlo exactamente
En [`src/src/video_core/renderer_vulkan/vk_rasterizer.cpp`](file:///e:/DeracinePCVR/src/src/video_core/renderer_vulkan/vk_rasterizer.cpp#L1122-L1228):
1. Define una constante centinela al principio de `BindBuffers`:
   ```cpp
   constexpr VideoCore::BufferId kStreamFastPathId{0xFFFFFFFEu};
   ```
2. En el primer bucle (`líneas 1126-1135`), si `!desc.is_written && size <= VideoCore::BufferCache::CACHING_PAGESIZE && !buffer_cache.IsRegionGpuModified(vsharp.base_address, size)`, **no llames a `buffer_cache.FindBuffer`**:
   ```cpp
   for (const auto& desc : stage.buffers) {
       const auto vsharp = desc.GetSharp(stage);
       if (!desc.IsSpecial() && vsharp.base_address != 0 && vsharp.GetSize() > 0) {
           const u64 size = memory->ClampRangeSize(vsharp.base_address, vsharp.GetSize());
           if (!desc.is_written && size <= VideoCore::BufferCache::CACHING_PAGESIZE &&
               !buffer_cache.IsRegionGpuModified(vsharp.base_address, size)) {
               buffer_bindings.emplace_back(kStreamFastPathId, vsharp, size);
           } else {
               const auto buffer_id = buffer_cache.FindBuffer(vsharp.base_address, size);
               buffer_bindings.emplace_back(buffer_id, vsharp, size);
           }
       } else {
           buffer_bindings.emplace_back(VideoCore::BufferId{}, vsharp, 0);
       }
   }
   ```
3. En el segundo bucle (`líneas 1192-1224`), cuando `buffer_id == kStreamFastPathId`, pasa `VideoCore::BufferId{}` a `ObtainBuffer` / `ObtainUniformBuffer` y **no llames a `vk_buffer->GetBarrier(...)`** (ya que `stream_buffer` es host-coherent y no necesita barreras de buffer):
   ```cpp
   const auto lookup_id = (buffer_id == kStreamFastPathId) ? VideoCore::BufferId{} : buffer_id;
   const auto [vk_buffer, offset] =
       is_storage ? buffer_cache.ObtainBuffer(vsharp.base_address, size, desc.is_written,
                                              desc.is_formatted, lookup_id)
                  : buffer_cache.ObtainUniformBuffer(vsharp.base_address, size, lookup_id);
   const u32 offset_aligned = Common::AlignDown(offset, alignment);
   const u32 adjust = offset - offset_aligned;
   ASSERT(adjust % 4 == 0);
   push_data.AddOffset(binding.buffer, adjust);
   buffer_infos.emplace_back(vk_buffer->Handle(), offset_aligned, size + adjust);
   if (buffer_id != kStreamFastPathId) {
       bool first_for_buffer = true;
       for (u32 j = 0; j < i; ++j) {
           if (std::get<0>(buffer_bindings[j]) == buffer_id) {
               first_for_buffer = false;
               break;
           }
       }
       if (first_for_buffer) {
           bool any_written = desc.is_written;
           for (u32 j = i + 1; j < buffer_bindings.size(); ++j) {
               if (std::get<0>(buffer_bindings[j]) == buffer_id &&
                   stage.buffers[j].is_written) {
                   any_written = true;
                   break;
               }
           }
           if (auto barrier = vk_buffer->GetBarrier(
                   any_written ? (vk::AccessFlagBits2::eShaderRead |
                                  vk::AccessFlagBits2::eShaderWrite)
                               : vk::AccessFlagBits2::eShaderRead,
                   vk::PipelineStageFlagBits2::eAllCommands)) {
               buffer_barriers.emplace_back(*barrier);
           }
       }
   }
   ```

---

### Optimización Paso 2 (IMPACTO MEDIO-ALTO EN GPU): Afinar `PipelineStageFlags2` en las barreras de Compute Shaders

#### Diagnóstico exacto
En [`src/src/video_core/renderer_vulkan/vk_rasterizer.cpp:1221`](file:///e:/DeracinePCVR/src/src/video_core/renderer_vulkan/vk_rasterizer.cpp#L1221), `BindBuffers` pasa siempre `vk::PipelineStageFlagBits2::eAllCommands` a `GetBarrier`.
Cuando los 51 Compute Shaders de morph targets se ejecutan seguidos en `DispatchDirect()`, usar `eAllCommands` obliga a la GPU a vaciar **todas** las unidades del pipeline (incluyendo transferencias y rasterización previa) en cada uno de los 51 dispatches, en lugar de sincronizar únicamente la etapa `eComputeShader` entre un dispatch y el siguiente.

#### Cómo implementarlo exactamente
En [`src/src/video_core/renderer_vulkan/vk_rasterizer.cpp:1217-1223`](file:///e:/DeracinePCVR/src/src/video_core/renderer_vulkan/vk_rasterizer.cpp#L1217-L1223), usa `eComputeShader` cuando `stage.stage == Shader::Stage::Compute`:
```cpp
const auto dst_stage = (stage.stage == Shader::Stage::Compute)
                           ? vk::PipelineStageFlagBits2::eComputeShader
                           : vk::PipelineStageFlagBits2::eAllCommands;
if (auto barrier = vk_buffer->GetBarrier(
        any_written ? (vk::AccessFlagBits2::eShaderRead |
                       vk::AccessFlagBits2::eShaderWrite)
                    : vk::AccessFlagBits2::eShaderRead,
        dst_stage)) {
    buffer_barriers.emplace_back(*barrier);
}
```
*(Nota: `Buffer::GetBarrier` en `buffer.h:142-145` ya amplía automáticamente `dst_stage` a `eAllCommands` cuando `!dst_is_write`, por lo que la transición desde el último Compute Shader hacia el Vertex Shader sigue cubriendo todas las etapas gráficas sin perder seguridad).*

---

### Optimización Paso 3 (IMPACTO EN FLUIDEZ Y CALIDAD VISUAL): Equilibrio entre MSAA y Resolución en `pc-vr/settings.txt`

Si el usuario nota que en escenas pesadas todavía cuesta mantener los 60 FPS nativos en su GPU:
1. **Probar `msaa=2` con `resolution=2340` y `sharpen=0.40`** en [`pc-vr/settings.txt`](file:///e:/DeracinePCVR/pc-vr/settings.txt):
   - Pasar de `msaa=4` a `msaa=2` reduce a la mitad el coste de ancho de banda de memoria de todos los render targets MSAA y del paso de `Resolve()`, permitiendo subir la resolución vertical de `2160` a `2340` con **mejor tasa de fotogramas y más detalle real por píxel**.
2. **Vista de escritorio (`desktop_view`)**:
   - En [`pc-vr/settings.txt:31`](file:///e:/DeracinePCVR/pc-vr/settings.txt#L31), cambiar `desktop_view=stereo` a `desktop_view=spectator` ahorra composición en la ventana del monitor de PC sin afectar en nada a lo que se ve dentro del visor VR.

---

### Optimización Paso 4 (TIEMPOS DE CARGA E INICIO): Verificar la Caché de Pipelines en Disco

En [`pc-vr/launch.ps1:931`](file:///e:/DeracinePCVR/pc-vr/launch.ps1#L931) ya hemos activado `pipeline_cache_enabled = $true` y dejado de borrar `pc-vr/user/cache/CUSA11919`.
- **Importante**: La **primera** vez que se abre un nivel tras este cambio, el emulador compila los shaders y los guarda en `pc-vr/user/cache/CUSA11919`. A partir de la **segunda** vez que se abre el juego, lee esa caché directamente de disco y el tiempo de carga se reduce drásticamente.
- Nunca vuelvas a añadir `Remove-Item ... user\cache\CUSA11919` en `launch.ps1` a menos que modifiques archivos dentro de `src/src/shader_recompiler/`.

---

## 5. Flujo de Trabajo Resumido para el Modelo Ligero

Cada vez que apliques uno de los pasos anteriores en `src/`:
1. Verifica en qué rama estás (`git branch --show-current` debe ser `optimizacion-rendimiento-v0.21.0`).
2. Aplica el cambio con `edit_file` (o un script Python en `scratch` si el archivo tiene saltos de línea CRLF).
3. Haz commit y push:
   ```powershell
   git config user.name "stefan0704"; git config user.email "26739250+stefan0704@users.noreply.github.com"; git add -u; git commit -m "perf(vulkan): <descripcion>"; git push origin optimizacion-rendimiento-v0.21.0
   ```
4. Programa un temporizador con la herramienta `schedule` (`DurationSeconds: 660`) para esperar a que termine el workflow `Build Windows PCVR` en GitHub Actions.
5. Comprueba el estado con `python "C:\Users\stefa\.gemini\antigravity\brain\82c36dc8-0d84-4abf-ad74-3d7c0e52f847\scratch\check_ci.py"` y, cuando marque `completed success`, descarga el binario actualizado ejecutando:
   ```powershell
   python tools/download-artifact.py
   ```
6. Pide al usuario que lo pruebe con `Play Deracine VR.bat`.
