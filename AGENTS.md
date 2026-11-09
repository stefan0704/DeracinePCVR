# AGENTS.md — Instrucciones Críticas para Agentes de IA

Bienvenido a **DeracinePCVR**. Este repositorio es un port para PCVR del juego exclusivo de PlayStation VR *Déraciné* (CUSA11919), basado en **shadPS4 v0.21.0** con soporte nativo de **OpenXR** y emulación 6DoF de mandos **PlayStation Move**.

---

## 1. Reglas Inviolables

1. **Idioma**: Todas las respuestas e interacciones con el usuario deben ser en **español**.
2. **Identidad Git (CRÍTICO)**:
   Siempre configurar y usar la dirección oficial noreply para evitar el rechazo `GH007`:
   ```powershell
   git config user.name "stefan0704"
   git config user.email "26739250+stefan0704@users.noreply.github.com"
   ```
3. **Control de Ramas**:
   - **Rama activa actual de trabajo**: `optimizacion-rendimiento-v0.21.0` (optimización de FPS, tiempos de carga y calidad visual).
   - **Rama dorada de respaldo (NO TOCAR JAMÁS)**: `actualizacion-v0.21.0` (commit `1d2f3936`, contiene el estado 100% funcional con todas las texturas, caras, flores y uvas arregladas).
   - **NUNCA tocar ni hacer push a `optimizacion-graficos`**.
4. **Mapeo de Mandos**:
   - Mantener siempre `swap_hands=1` en `pc-vr/settings.txt` (y recordar que el botón `L3` en VR permite intercambiar las manos en vivo).
5. **Documentos de Referencia Obligatoria**:
   - **[HANDOFF_V0.21.0.md](file:///e:/DeracinePCVR/HANDOFF_V0.21.0.md)**: **LECTURA OBLIGATORIA AL INICIAR**. Contiene las zonas intocables que arreglaron los morph targets/texturas y la **Hoja de Ruta Paso a Paso (Sección 4)** con el código exacto para seguir optimizando el rendimiento con un modelo ligero.
   - [DEVELOPMENT.md](file:///e:/DeracinePCVR/DEVELOPMENT.md): Historia completa del proyecto, arquitectura OpenXR, parches de sonido y calibración 6DoF de Move.

---

## 2. Estructura Rápida de Archivos

- `pc-vr/`: Carpeta de ejecución en Windows.
  - `shadps4.exe`: Binario activo (v0.21.0).
  - `launch.ps1`: Script de inicio que configura variables y OpenXR.
  - `settings.txt`: Configuración de usuario (`swap_hands=1`, `readbacks=relaxed`, `resolution=2160`, `msaa=4`, etc.).
- `Play Deracine VR.bat`: Acceso directo para que el usuario inicie el juego en VR.
- `src/`: Código C++ de shadPS4 con las extensiones VR:
  - `src/src/core/vr/`: Módulos de OpenXR y distorsión (`hmd.cpp`, `hmd_reprojection.cpp`, `openxr_host.cpp`, etc.).
  - `src/src/core/libraries/move/`: Emulación de mandos PlayStation Move (`move.cpp`).
  - `src/src/video_core/buffer_cache/`: Caché de buffers y barreras Vulkan (`buffer.h`, `buffer_cache.cpp`).
  - `src/src/video_core/renderer_vulkan/`: Rasterizador, Presenter y exportador de texturas VR (`vk_rasterizer.cpp`, `vk_presenter.cpp`, `vk_vr_exporter.cpp`).
- `tools/download-artifact.py`: Herramienta para descargar el artefacto compilado por GitHub Actions sin errores de autenticación 401.
- `.github/workflows/build-windows.yml`: Flujo de CI que compila Windows x86_64.

---

## 3. Flujo de Trabajo para Nuevas Optimizaciones

1. Leer la **Sección 4 de [HANDOFF_V0.21.0.md](file:///e:/DeracinePCVR/HANDOFF_V0.21.0.md)** y aplicar un paso de optimización en `src/`.
2. Commit y push a `optimizacion-rendimiento-v0.21.0`.
3. Esperar que GitHub Actions termine el workflow `build-windows.yml` usando `schedule`.
4. Ejecutar `python tools/download-artifact.py` para descargar y extraer el ejecutable a `pc-vr/shadps4.exe`.
5. Probar con el usuario a través de `Play Deracine VR.bat`.
