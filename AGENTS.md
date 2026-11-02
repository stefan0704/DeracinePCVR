# AGENTS.md — Instrucciones Críticas para Agentes de IA

Bienvenido a **DeracinePCVR**. Este repositorio es un port para PCVR del juego exclusivo de PlayStation VR *Déraciné* (CUSA11919), basado en **shadPS4** con soporte nativo de **OpenXR** y emulación 6DoF de mandos **PlayStation Move**.

---

## 1. Reglas Inviolables

1. **Idioma**: Todas las respuestas e interacciones con el usuario deben ser en **español**.
2. **Identidad Git (CRÍTICO)**:
   Siempre configurar y usar la dirección oficial noreply para evitar el rechazo `GH007`:
   ```bash
   git config user.name "stefan0704"
   git config user.email "26739250+stefan0704@users.noreply.github.com"
   ```
3. **Control de Ramas**:
   - Rama activa actual: `actualizacion-v0.21.0` (actualización a shadPS4 v0.21.0 "Until Mavethee Dawn").
   - **NUNCA tocar ni hacer push a `optimizacion-graficos`**.
4. **Mapeo de Mandos**:
   - Mantener siempre `swap_hands=1` en `pc-vr/settings.txt`.
5. **Documentos de Referencia Obligatoria**:
   - [HANDOFF_V0.21.0.md](file:///e:/DeracinePCVR/HANDOFF_V0.21.0.md): Detalles técnicos de la actualización v0.21.0, errores resueltos y guía de solución.
   - [DEVELOPMENT.md](file:///e:/DeracinePCVR/DEVELOPMENT.md): Historia completa del proyecto, arquitectura OpenXR, parches de sonido y calibración 6DoF de Move.

---

## 2. Estructura Rápida de Archivos

- `pc-vr/`: Carpeta de ejecución en Windows.
  - `shadps4.exe`: Binario activo (v0.21.0).
  - `launch.ps1`: Script de inicio que configura variables y OpenXR.
  - `settings.txt`: Configuración de usuario (`swap_hands=1`, `readbacks=relaxed`, etc.).
- `Play Deracine VR.bat`: Acceso directo para que el usuario inicie el juego en VR.
- `src/`: Código C++ de shadPS4 con las extensiones VR:
  - `src/core/vr/`: Módulos de OpenXR y distorsión (`hmd.cpp`, `hmd_reprojection.cpp`, `openxr_host.cpp`, etc.).
  - `src/core/libraries/move/`: Emulación de mandos PlayStation Move (`move.cpp`).
  - `src/core/libraries/vr_tracker/`: Emulación de tracking VR (`vr_tracker.cpp`).
  - `src/video_core/renderer_vulkan/`: Presenter y exportador de texturas VR a OpenXR (`vk_presenter.cpp`, `vk_vr_exporter.cpp`).
- `tools/download-artifact.py`: Herramienta para descargar el artefacto compilado por GitHub Actions sin errores de autenticación 401.
- `.github/workflows/build-windows.yml`: Flujo de CI que compila Windows x86_64 con LLVM/Clang o MSVC.

---

## 3. Flujo de Trabajo para Nuevas Correcciones

1. Editar el código mínimo necesario en `src/`.
2. Commit y push a `actualizacion-v0.21.0`.
3. Esperar que GitHub Actions termine el workflow `build-windows.yml`.
4. Ejecutar `python tools/download-artifact.py` para descargar y extraer el ejecutable a `pc-vr/shadps4.exe`.
5. Probar con el usuario a través de `Play Deracine VR.bat`.
