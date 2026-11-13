# EffectSuite

Simulador de pedales y amplificadores de guitarra, pensado para tocar en tiempo real con la mínima latencia y para grabar dentro de un DAW.

Funciona como aplicación independiente y como plugin (VST3 en Windows y Mac, AU en Mac).

## Qué incluye ahora

| Bloque | Cómo está hecho |
|---|---|
| Afinador cromático | Detección de tono YIN, error medido < 0,2 cents |
| Puerta de ruido | Con histéresis y retención |
| Compresor | Detector de pico, relación 4:1 con codo suave |
| Overdrive | Circuito de diodos en la realimentación de un operacional, resuelto por Newton-Raphson en cada muestra |
| Distorsión | Circuito de recorte duro: operacional de alta ganancia y diodos a masa, resuelto igual |
| Fuzz | Dos etapas de saturación asimétrica (modelo de comportamiento, no de circuito) |
| Amplificador | Tres modelos: Clean American, British Crunch, High Gain. Previo de varias etapas de válvula, control de tonos calculado desde el circuito real y etapa de potencia con sag |
| Pantalla | Carga de respuestas a impulso (WAV/AIFF) sin latencia, más una pantalla integrada |
| Chorus | Estéreo, con retardo modulado en cuadratura |
| Delay | Hasta 1,5 s, con realimentación filtrada |
| Reverb | Estéreo, algoritmo Freeverb |

Orden de la señal: puerta → compresor → overdrive → distorsión → fuzz → amplificador → pantalla → chorus → delay → reverb.

El procesado añade unas 4 muestras de latencia (0,1 ms). El resto de la latencia depende del driver y del tamaño de búfer de la interfaz de audio.

## Descargar

Cada cambio se compila automáticamente. En la pestaña **Actions** del repositorio, abre la última ejecución correcta y descarga `EffectSuite-Windows` o `EffectSuite-macOS`.

### Windows

- `EffectSuite.exe` es la aplicación; no necesita instalación.
- Para usarlo en un DAW, copia la carpeta `EffectSuite.vst3` a `C:\Program Files\Common Files\VST3`.
- La primera vez Windows mostrará un aviso de SmartScreen porque el programa no está firmado: "Más información" → "Ejecutar de todas formas".

### macOS (10.15 o posterior, Intel y Apple Silicon)

- Copia `EffectSuite.app` a Aplicaciones.
- Para un DAW: `EffectSuite.vst3` va en `~/Library/Audio/Plug-Ins/VST3` y `EffectSuite.component` en `~/Library/Audio/Plug-Ins/Components`.
- Al no estar firmado con certificado de Apple, hay que quitar la marca de cuarentena una vez:

```bash
xattr -cr /Applications/EffectSuite.app
```

## Primer uso (aplicación independiente)

1. Abre **Options → Audio/MIDI Settings**.
2. Elige el driver y el dispositivo. En Windows, usa **ASIO** si la interfaz tiene driver ASIO; si no, **Windows Audio (Exclusive Mode)** o **Low Latency Mode**.
3. Baja el tamaño de búfer a 64 o 128 muestras. Si hay chasquidos, sube un paso.
4. La entrada arranca silenciada para evitar acoples: desactívalo en el aviso amarillo de la ventana o en esos mismos ajustes.
5. Elige en el panel **Entrada** el canal donde está enchufada la guitarra.

## Compilar

Hace falta CMake 3.22 o posterior y un compilador de C++17 (Visual Studio 2022, Xcode o GCC). JUCE se descarga solo.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build --config Release --parallel
```

Los resultados quedan en `build/EffectSuite_artefacts/Release/`.

## Pruebas

`EffectSuiteMeasure` comprueba el motor de audio sin tarjeta de sonido: respuesta del control de tonos frente al circuito analógico, distorsión, aliasing, latencia, coste de CPU, precisión del afinador, comportamiento de cada pedal y estabilidad.

```bash
./build/EffectSuiteMeasure_artefacts/Release/EffectSuiteMeasure
```

Con `--render <carpeta>` escribe además archivos WAV de demostración con una guitarra sintética.

## Estructura

- `src/dsp/`: motor de audio (pedales, amplificador, control de tonos, afinador y cadena completa).
- `src/PluginProcessor.*`: parámetros, estado y conexión con el host.
- `src/PluginEditor.*`: interfaz gráfica.
- `tests/Measure.cpp`: pruebas de medida.

## Licencia de dependencias

EffectSuite usa [JUCE](https://juce.com), con licencia AGPLv3 o comercial. Distribuir binarios sin licencia comercial de JUCE implica publicar el código bajo AGPLv3.
