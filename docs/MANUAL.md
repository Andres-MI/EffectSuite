# EffectSuite: manual de usuario

EffectSuite es un simulador de pedales y amplificadores para guitarra y bajo. Funciona como aplicación independiente (la abres y tocas) y como plugin VST3 (Windows y macOS) o AU (macOS) dentro de un DAW.

## Índice

1. [Qué es y qué necesitas](#1-qué-es-y-qué-necesitas)
2. [Instalación](#2-instalación)
3. [Primer arranque y ajustes de audio](#3-primer-arranque-y-ajustes-de-audio)
4. [La interfaz](#4-la-interfaz)
5. [Presets](#5-presets)
6. [Referencia de pedales](#6-referencia-de-pedales)
7. [Amplificadores y pantallas](#7-amplificadores-y-pantallas)
8. [Capturas neuronales (.nam)](#8-capturas-neuronales-nam)
9. [Afinador](#9-afinador)
10. [Rendimiento y latencia](#10-rendimiento-y-latencia)
11. [MIDI](#11-midi)
12. [Uso como plugin en un DAW](#12-uso-como-plugin-en-un-daw)
13. [Solución de problemas](#13-solución-de-problemas)
14. [Limitaciones conocidas](#14-limitaciones-conocidas)
15. [Licencia](#15-licencia)

---

## 1. Qué es y qué necesitas

EffectSuite reproduce en tiempo real una cadena de señal completa:

```
pedales de antes del ampli -> amplificador -> pantalla -> pedales de después del ampli
```

Incluye 14 pedales, 9 amplificadores (6 de guitarra y 3 de bajo), 5 pantallas, carga de respuestas a impulso propias, carga de capturas neuronales `.nam`, un afinador y 15 presets de fábrica.

### Qué necesitas

- Un ordenador con **Windows 10/11** o **macOS 10.15 o posterior** (Intel y Apple Silicon).
- Una **interfaz de audio con entrada de instrumento** (Hi-Z). Enchufar la guitarra directamente a la entrada de línea o de micrófono de un ordenador suele dar una señal débil y con ruido.
- **Auriculares o monitores** de estudio. Con altavoces abiertos y un micrófono cerca puedes provocar acoples (ver [sección 13](#13-solución-de-problemas)).
- Un cable de instrumento y tu guitarra o bajo.

Para usarlo como plugin necesitas además un DAW compatible con VST3 o, en macOS, con AU.

---

## 2. Instalación

Los instaladores no están firmados con certificado de desarrollador, así que tu sistema te avisará la primera vez. Es lo esperado; los pasos para continuar están más abajo.

### Dónde descargar

Cada versión se compila automáticamente. En la página del repositorio entra en la pestaña **Actions**, abre la **última ejecución correcta** y descarga el instalador de tu sistema:

| Sistema | Archivo | Qué instala |
|---|---|---|
| Windows | `EffectSuite-Setup.exe` | La aplicación y el plugin VST3 |
| macOS | `EffectSuite.pkg` | La aplicación, el plugin VST3 y el plugin AU |

### Windows

1. Descarga `EffectSuite-Setup.exe` y ábrelo.
2. Windows SmartScreen mostrará un aviso de que protegió tu equipo, porque el programa no está firmado. Pulsa **Más información** y después **Ejecutar de todas formas**.
3. Sigue los pasos del instalador.
4. Abre EffectSuite para usar la aplicación independiente. Si usas un DAW, vuelve a escanear los plugins para que aparezca el VST3.

### macOS

1. Descarga `EffectSuite.pkg`.
2. No lo abras con doble clic. Haz **clic derecho** (o Control + clic) sobre el archivo y elige **Abrir**. Confirma en el cuadro que aparece.
3. Sigue los pasos del instalador.
4. Abre EffectSuite desde la carpeta Aplicaciones. Si usas un DAW, vuelve a escanear los plugins.

### Dónde quedan los plugins

Los plugins quedan en las carpetas estándar de cada sistema. Si algún día tienes que buscarlos o copiarlos a mano:

| Formato | Ruta |
|---|---|
| VST3, Windows | `C:\Program Files\Common Files\VST3` |
| VST3, macOS | `~/Library/Audio/Plug-Ins/VST3` |
| AU, macOS | `~/Library/Audio/Plug-Ins/Components` |

---

## 3. Primer arranque y ajustes de audio

Esta sección es para la aplicación independiente. Si usas el plugin en un DAW, los ajustes de audio los controla el DAW; salta a la [sección 4](#4-la-interfaz) y a la [sección 12](#12-uso-como-plugin-en-un-daw).

La ventana se ajusta sola al tamaño de tu pantalla al abrirse. Puedes cambiar su tamaño arrastrando la esquina inferior derecha, y el programa recuerda el tamaño que dejes.

### 3.1 Elegir driver y dispositivo

1. Conecta tu interfaz de audio y enchufa la guitarra a su entrada de instrumento.
2. En la esquina superior izquierda pulsa **Options** y elige **Audio/MIDI Settings**. También puedes pulsar el botón **Settings...** del aviso amarillo.
3. Elige el **driver** (tipo de dispositivo de audio):
   - En Windows, usa **ASIO** si tu interfaz tiene driver ASIO. Es la opción de menor latencia.
   - Si no hay ASIO, usa **Windows Audio (Exclusive Mode)**. Como alternativa, **Low Latency Mode**.
   - En macOS, usa el driver del sistema (Core Audio) y elige tu interfaz.
4. Elige tu interfaz como dispositivo de entrada y de salida.

### 3.2 Tamaño de búfer

El tamaño de búfer es el trozo de audio que el ordenador procesa de una vez. Cuanto más pequeño, menos retardo (latencia), pero más carga para el equipo.

1. Baja el tamaño de búfer a **64 o 128 muestras**.
2. Toca un rato. Si oyes chasquidos o cortes, sube un paso (por ejemplo, de 64 a 128).

### 3.3 Quitar el silencio de la entrada

Al arrancar, la entrada de audio está **silenciada** para evitar un acople si los altavoces suenan cerca del micrófono. Lo verás en una franja amarilla bajo la barra del título: *Audio input is muted to avoid feedback loop*.

1. Desactiva el silencio de la entrada desde ese aviso (botón **Settings...**) o en **Options → Audio/MIDI Settings**.
2. Comprueba que al tocar se mueve el medidor de entrada (ver más abajo).

Hazlo cuando ya tengas auriculares puestos o los monitores a un volumen razonable.

### 3.4 Elegir el canal de entrada

En la cabecera, el selector **Canal** indica de dónde toma la señal EffectSuite:

| Opción | Qué hace |
|---|---|
| Left (1) | Usa la entrada 1 (izquierda). Es la opción por defecto. |
| Right (2) | Usa la entrada 2 (derecha). |
| Left + Right | Suma las dos entradas. |

Elige el canal donde esté enchufada tu guitarra. Si la guitarra está en la entrada 1 de tu interfaz, deja *Left (1)*.

### 3.5 Ajustar la ganancia de entrada

El mando **Entrada** de la cabecera sube o baja el nivel de lo que llega de tu interfaz (de -24 a +24 dB). A su lado hay un medidor.

1. Toca con la fuerza más alta que vayas a usar (acordes fuertes).
2. Mira el medidor de entrada. Sube **Entrada** hasta que los picos suban bien por el medidor sin llegar al tope.
3. Si el medidor llega al máximo, baja **Entrada**. Si no ves apenas movimiento, sube la ganancia de la propia interfaz antes de subir **Entrada** al máximo.

El mando **Salida** (de -24 a +12 dB) regula el volumen final. Su medidor es el de la derecha. Un limitador suave evita que la salida llegue a 0 dBFS, pero no sustituye a un buen ajuste de volumen.

---

## 4. La interfaz

La ventana tiene cuatro zonas, de arriba abajo: la cabecera, la barra de presets, la tira de señal y las tres filas de equipo.

### 4.1 Cabecera

De izquierda a derecha:

| Elemento | Función |
|---|---|
| **EffectSuite** | Nombre del programa. |
| **Entrada** (mando) y su medidor | Nivel de entrada, de -24 a +24 dB. |
| **Canal** | Entrada de la interfaz que se usa (ver 3.4). |
| Afinador | Pantalla con la nota, una escala de cents y la frecuencia en Hz (ver [sección 9](#9-afinador)). |
| **Silenciar** | Interruptor que silencia la salida mientras afinas. |
| **Calidad** | Sobremuestreo: Mínimo (1x), Eco (2x), Normal (4x) o Alta (8x). Ver [sección 10](#10-rendimiento-y-latencia). |
| **Salida** (mando) y su medidor | Nivel de salida, de -24 a +12 dB. |

Los mandos se mueven arrastrando con el ratón.

En la aplicación independiente, encima de la cabecera hay una barra con el botón **Options** (a la izquierda) y el título de la ventana. Bajo ella puede verse la franja amarilla del aviso de entrada silenciada.

### 4.2 Barra de presets

Contiene la etiqueta **Preset**, un desplegable (muestra *Sin preset* hasta que eliges uno), los botones **Guardar** y **Borrar**, y a la derecha el indicador **CPU**. Todo sobre presets está en la [sección 5](#5-presets) y sobre el indicador de CPU, en la [sección 10](#10-rendimiento-y-latencia).

### 4.3 Tira de señal

Es una fila de etiquetas con el orden real de la señal, de **IN** a **OUT**:

```
IN -> pedales de antes del ampli -> AMP -> CAB -> pedales de después del ampli -> OUT
```

Cada etiqueta representa un bloque (por ejemplo PUERTA, COMP, OD, WAH, DIST, PHASER, FUZZ, AMP, CAB, CHORUS, DELAY, REVERB) con el color de su pedal. La etiqueta está **iluminada cuando el bloque está encendido**. Si pulsas una etiqueta, enciendes o apagas ese bloque. La tira refleja siempre el orden actual de tus pedales.

### 4.4 Las tres filas

El equipo está dividido en tres filas, cada una con su título:

1. **ANTES DEL AMPLI**: los pedales que van entre tu guitarra y el ampli. Pueden estar aquí: Puerta, Wah, Compresor, Octavador, Phaser, Overdrive, Distorsión y Fuzz.
2. **AMPLI**: a la izquierda el **AMPLIFICADOR** (cabezal) y a la derecha la **PANTALLA**.
3. **DESPUÉS DEL AMPLI**: los pedales que van tras el ampli y la pantalla. Pueden estar aquí: Ecualizador, Flanger, Chorus, Tremolo, Delay y Reverb.

Cada tipo de pedal existe **una sola vez**: no puedes tener dos overdrives, por ejemplo.

### 4.5 Los pedales

Cada pedal tiene una carcasa de color, sus mandos con el nombre encima, el nombre del pedal, un **piloto (LED)** y un **interruptor de pie** redondo.

- **Encender y apagar**: pulsa el interruptor de pie. El LED se ilumina en rojo cuando el pedal está activo. El cambio se hace con un fundido corto, sin chasquido.
- **Mover un mando**: arrástralo con el ratón.
- **Elegir modelo**: los pedales con modelos (Wah, Overdrive, Distorsión y Fuzz) tienen un desplegable en la parte superior.
- **Quitar un pedal de la pedalera**: pulsa la pequeña **x** de la esquina superior derecha del pedal, o haz clic derecho sobre la carcasa y elige **Quitar de la pedalera**. Al quitarlo, el pedal se apaga.
- **Añadir un pedal**: pulsa el recuadro de líneas discontinuas **Añadir pedal** al final de la fila. Se abre un menú con los pedales de esa fila que no están en la pedalera. Al elegir uno, se añade al final de la fila y **se enciende**.
- **Reordenar**: agarra el pedal por la carcasa (no por un mando ni por el interruptor) y arrástralo a otra posición **dentro de su misma fila**. Una marca indica dónde caerá. El orden en la fila es el orden real de la señal, y la tira de señal lo refleja. Un pedal no puede pasar de la fila de antes del ampli a la de después, ni al revés.

El orden importa. Por ejemplo, un overdrive delante de una distorsión suena distinto que al revés.

La pedalera inicial trae: Puerta, Compresor y Overdrive antes del ampli; Chorus, Delay y Reverb después. De ellos, solo la Puerta empieza encendida (el ampli y la pantalla también están encendidos).

### 4.6 El amplificador y la pantalla

En la fila central:

- **AMPLIFICADOR**: un interruptor **POWER** a la izquierda, seis mandos (**Gain, Bass, Mid, Treble, Presence, Master**), el desplegable **Modelo** y los botones **Cargar captura...** y **Quitar** con el nombre de la captura (*Sin captura* si no hay ninguna). Ver secciones 7 y 8.
- **PANTALLA**: un interruptor de encendido, el desplegable **Integrada** con las 5 pantallas, y los botones **Cargar IR...** y **Quitar** con el nombre del archivo (*Sin IR* si no hay ninguno). Ver sección 7.

Si apagas el ampli o la pantalla con su interruptor, ese bloque se salta.

### 4.7 Tamaño de la ventana

La ventana mantiene siempre la misma proporción. Se adapta a tu pantalla al abrirse, se redimensiona desde la esquina inferior derecha y recuerda su tamaño para la próxima vez.

---

## 5. Presets

Un preset es una foto de tu sonido: qué pedales hay, en qué orden, cómo están ajustados, qué ampli y pantalla usas.

### 5.1 Cargar un preset

1. Abre el desplegable de la barra de presets.
2. Verás dos secciones: **De fábrica** y **Mis presets**. Si aún no has guardado ninguno, *Mis presets* muestra *(ninguno)*.
3. Elige uno. Se carga al instante.

Si el preset no se puede cargar, aparece el aviso *No se pudo cargar el preset*.

### 5.2 Guardar tu propio preset

1. Deja el sonido como quieres.
2. Pulsa **Guardar**.
3. Escribe el nombre en el cuadro **Nombre del preset** y pulsa **Guardar** (o Intro). **Cancelar** cierra el cuadro sin guardar.

Si el preset que tienes cargado ya es uno tuyo, el cuadro te propone su nombre. Si guardas con un nombre que ya existe, ese archivo se sustituye. Los caracteres que no sean válidos en un nombre de archivo se cambian.

Si falla, aparece *No se pudo guardar el preset*.

### 5.3 Borrar un preset

1. Elige en el desplegable un preset de *Mis presets*.
2. Pulsa **Borrar** y confirma en el cuadro *Borrar preset*.

El botón **Borrar** solo está activo cuando hay seleccionado un preset tuyo. Los de fábrica no se pueden borrar.

### 5.4 Dónde se guardan los archivos

Tus presets son archivos `.espreset` en la carpeta:

```
Documentos/EffectSuite/Presets
```

(la carpeta *Documentos* de tu usuario, en Windows y en macOS). Puedes hacer copia de seguridad de esos archivos o pasarlos a otro ordenador copiándolos a la misma carpeta. Aparecerán en *Mis presets*, ordenados por nombre.

### 5.5 Qué guarda un preset y qué no

| Un preset **guarda** | Un preset **no guarda** (son ajustes de tu equipo) |
|---|---|
| Qué pedales hay en la pedalera y su orden | Ganancia de **Entrada** |
| Si cada pedal está encendido o apagado | El **Canal** de entrada |
| Los mandos y modelos de cada pedal | El nivel de **Salida** |
| El ampli: encendido, modelo y mandos | La **Calidad** |
| La pantalla: encendida y modelo integrado | El interruptor **Silenciar** |
| La respuesta a impulso (IR) cargada | |
| La captura neuronal (.nam) cargada | |

Así, cambiar de preset no te cambia el volumen de entrada ni la configuración de calidad.

Un detalle importante: el preset guarda la **ruta** del archivo de IR o de captura, no el archivo. Si mueves, renombras o borras ese archivo, o abres el preset en otro ordenador donde el archivo no está en la misma ruta, el preset se carga sin la IR o sin la captura. Mantén tus IR y capturas en una carpeta fija.

Los presets de fábrica no llevan IR ni captura.

### 5.6 Presets de fábrica

Hay 15: 10 de guitarra y 5 de bajo. Las descripciones se derivan de sus ajustes. Los pedales que se indican son los que el preset deja encendidos.

| Preset | Qué lleva |
|---|---|
| Guitarra: Limpio cristalino | Ampli Clean American con la pantalla Guitar 2x12 Open, compresor suave, chorus y algo de reverb. Para sonidos limpios y abiertos. |
| Guitarra: Blues con cuerpo | Ampli Tweed con la pantalla Guitar 1x12 y un overdrive Blues con ganancia media, más un toque de reverb. |
| Guitarra: Brillo clase A | Ampli Class A Chime con Guitar 2x12 Open, tremolo suave y reverb. |
| Guitarra: Crunch británico | Ampli British Crunch con Guitar 4x12 y la puerta de ruido. Sin más pedales: el crunch lo da el ampli. |
| Guitarra: Rock clásico | British Crunch con Guitar 4x12, puerta, overdrive Mid Boost con poca ganancia y un delay de 340 ms. |
| Guitarra: Metal moderno | Ampli High Gain con Guitar 4x12, medios recortados, puerta con umbral más alto (-55 dB) y overdrive Mid Boost con la ganancia al mínimo como refuerzo previo. |
| Guitarra: Solista | Ampli Modern Lead con Guitar 4x12, medios subidos, puerta, delay de 420 ms y reverb. |
| Guitarra: Fuzz psicodélico | Fuzz Vintage delante de un ampli Clean American con poca ganancia, más phaser lento y reverb. |
| Guitarra: Funk auto-wah | Compresor con mucho sustain y wah en modo Auto, delante del Clean American con Guitar 2x12 Open. |
| Guitarra: Ambiente | Clean American con Guitar 2x12 Open, compresor, chorus, delay largo (500 ms) con repeticiones, y reverb grande. |
| Bajo: Redondo a válvulas | Ampli Bass Tube con la pantalla Bass 8x10 y un compresor. |
| Bajo: Limpio moderno | Ampli Bass Clean con Bass 1x15, compresor y ecualizador con graves y agudos subidos y medios graves recortados. |
| Bajo: Rock | Ampli Bass Rock con Bass 8x10, compresor y overdrive Bass Drive. |
| Bajo: Fuzz | Ampli Bass Tube con Bass 8x10 y el fuzz Bass Fuzz. |
| Bajo: Sub octava | Ampli Bass Clean con Bass 8x10, compresor, octavador (octava inferior) y chorus suave. |

---

## 6. Referencia de pedales

Todos los mandos van de 0 a 1 (en la pantalla verás el valor), salvo los que se indican en dB o ms.

Los pedales de antes del ampli trabajan sobre tu señal sin amplificar; los de después, sobre el sonido ya amplificado y en estéreo.

### 6.1 Antes del ampli

#### Puerta (noise gate)

Corta el ruido cuando no tocas. Abre al detectar señal y cierra cuando la señal cae por debajo de la mitad del umbral, con una pequeña retención para no cortar el final de las notas.

| Control | Qué hace |
|---|---|
| **Threshold** | Nivel (de -90 a -20 dB) por encima del cual se abre la puerta. |

Consejo: sube **Threshold** poco a poco hasta que el ruido de fondo desaparezca al parar; si se come el final de las notas, bájalo. Con ganancias altas hará falta un umbral más alto.

#### Wah

Filtro de paso de banda resonante que barre de 350 Hz a 2,2 kHz.

| Control | Qué hace |
|---|---|
| Selector **Manual / Auto** | En Manual, **Position** mueve el filtro a mano o por MIDI. En Auto, el filtro lo mueve la fuerza con que tocas. |
| **Position** | En Manual: posición del pedal (talón a punta). En Auto: sensibilidad al ataque. |
| **Resonance** | Cuánto se estrecha y destaca el pico del filtro. |

Consejo: para usarlo como un pedal real, ponlo en Manual y controla **Position** con un pedal de expresión MIDI (ver [sección 11](#11-midi)). En Auto, toca con dinámica: las notas fuertes abren más el filtro.

#### Compresor

Compresor de pedal con detector de picos y relación 4:1 con codo suave. Compensa el volumen automáticamente.

| Control | Qué hace |
|---|---|
| **Sustain** | Cuánta compresión se aplica (más Sustain = umbral más bajo). |
| **Attack** | Tiempo de ataque, de 1 a 30 ms aproximadamente. |
| **Level** | Volumen de salida. |

Consejo: con **Attack** alto se deja pasar el golpe inicial de la nota; con **Attack** bajo se aplana antes. Pónlo el primero de la cadena para uniformar el nivel que reciben los demás pedales.

#### Octavador

Añade una octava por debajo de la nota que tocas.

| Control | Qué hace |
|---|---|
| **Sub** | Volumen de la octava inferior. |
| **Dry** | Volumen de la señal original. |

Consejo: toca notas sueltas y limpias, sobre todo en las cuerdas graves. Es **monofónico**: con acordes o dos notas a la vez no funciona bien (ver [sección 14](#14-limitaciones-conocidas)).

#### Phaser

Cuatro etapas de desfase con realimentación. Barre entre unos 300 Hz y 3 kHz.

| Control | Qué hace |
|---|---|
| **Rate** | Velocidad del barrido (de 0,1 a 8 Hz). |
| **Depth** | Amplitud del barrido. |
| **Feedback** | Realimentación: más resonancia y carácter. |

Consejo: velocidades bajas dan un efecto envolvente; con **Feedback** alto el efecto se hace más marcado.

#### Overdrive

Saturación suave a base de diodos. Tiene cuatro modelos. Los nombres son genéricos y no corresponden a marcas concretas.

| Control | Qué hace |
|---|---|
| Selector de modelo | Elige uno de los cuatro modelos. |
| **Drive** | Cantidad de saturación. |
| **Tone** | Brillo del sonido saturado. |
| **Level** | Volumen de salida. |

| Modelo | Carácter, según cómo está construido |
|---|---|
| **Mid Boost** | Recorta más los graves antes de saturar, así que el sonido queda centrado en los medios. Saturación simétrica. Es el modelo por defecto. |
| **Blues** | Saturación asimétrica y más graves entrando al circuito. Algo más bajo de volumen. |
| **Transparent** | Mezcla señal limpia con la saturada, por lo que conserva más la dinámica y el carácter de tu guitarra. |
| **Bass Drive** | **Pensado para bajo.** Mantiene limpios los graves (por debajo de unos 150 Hz) y satura solo el resto. |

Consejo: úsalo con **Level** alto y **Drive** bajo delante del ampli para empujarlo; o con **Drive** alto como saturación propia.

#### Distorsión

Recorte duro: mucha ganancia seguida de diodos. Cuatro modelos.

| Control | Qué hace |
|---|---|
| Selector de modelo | Elige uno de los cuatro modelos. |
| **Gain** | Cantidad de distorsión. |
| **Tone** | Brillo. |
| **Level** | Volumen de salida. |

| Modelo | Carácter, según cómo está construido |
|---|---|
| **Classic** | Distorsión de uso general. Es el modelo por defecto. |
| **Rodent** | Tiene la mayor ganancia posible de los cuatro y un tono de partida más oscuro. |
| **Metal** | Muy alta ganancia, con los medios vaciados (una bajada alrededor de 700 Hz) y más volumen de salida. |
| **Bass Distortion** | **Pensado para bajo.** Mantiene limpios los graves (por debajo de unos 120 Hz) y distorsiona el resto. |

Consejo: con distorsiones fuertes pon delante la Puerta y, si te sobran recursos, sube **Calidad** para reducir el aliasing (ver [sección 10](#10-rendimiento-y-latencia)).

#### Fuzz

Dos etapas de saturación muy asimétrica, de recorte casi cuadrado. Es un **modelo de comportamiento**, no una réplica de un circuito concreto.

| Control | Qué hace |
|---|---|
| Selector de modelo | Elige uno de los tres modelos. |
| **Fuzz** | Cantidad de fuzz. |
| **Tone** | Brillo. |
| **Level** | Volumen de salida. |

| Modelo | Carácter |
|---|---|
| **Vintage** | Fuzz de dos etapas, el básico. Es el modelo por defecto. |
| **Octave** | Rectifica la señal antes de saturarla, lo que añade una octava por encima. |
| **Bass Fuzz** | **Pensado para bajo.** Mantiene limpios los graves (por debajo de unos 120 Hz) y deja entrar el fuzz en el resto. |

Consejo: el fuzz responde al volumen de la guitarra y a lo que haya antes; ponlo el primero en la cadena (antes de compresor o wah) y prueba a bajar el volumen de la guitarra para limpiar el sonido.

### 6.2 Después del ampli

#### Ecualizador

Cuatro bandas de ±12 dB, con deslizadores verticales.

| Control | Qué hace |
|---|---|
| **100** | Graves (100 Hz). |
| **400** | Medios graves (400 Hz). |
| **1.6k** | Medios agudos (1,6 kHz). |
| **5k** | Agudos (5 kHz). |

Consejo: se aplica tras la pantalla, así que sirve para ajustar el sonido final (por ejemplo, recortar medios graves en un bajo).

#### Flanger

Retardo muy corto (de 1 a 6 ms) modulado, con realimentación.

| Control | Qué hace |
|---|---|
| **Rate** | Velocidad (de 0,05 a 3 Hz). |
| **Depth** | Amplitud del barrido. |
| **Feedback** | Realimentación: da un sonido más metálico. |

#### Chorus

Estéreo, con un retardo modulado que se lee de forma distinta en cada lado.

| Control | Qué hace |
|---|---|
| **Rate** | Velocidad (de 0,1 a 5 Hz). |
| **Depth** | Cuánto varía el retardo (hasta ±4 ms). |
| **Mix** | Cantidad de efecto mezclada con la señal original. |

Consejo: con **Mix** moderado ensancha el sonido sin que se note el vaivén.

#### Tremolo

Modulación del volumen.

| Control | Qué hace |
|---|---|
| **Rate** | Velocidad (de 1 a 12 Hz). |
| **Depth** | Cuánto baja el volumen en cada ciclo. |

#### Delay

Retardo de hasta 1,5 s con realimentación. Las repeticiones se oscurecen poco a poco.

| Control | Qué hace |
|---|---|
| **Time** | Tiempo entre repeticiones, de 20 ms a 1500 ms. |
| **Feedback** | Cuántas repeticiones se oyen. |
| **Mix** | Volumen del retardo respecto a la señal directa. |

Consejo: para un eco discreto, **Feedback** bajo y **Mix** bajo; para un ambiente largo, **Time** alto y **Feedback** medio.

#### Reverb

Reverb estéreo.

| Control | Qué hace |
|---|---|
| **Size** | Tamaño de la sala. |
| **Mix** | Cantidad de reverb. |

Consejo: ponla la última de la cadena y con **Mix** bajo; la reverb come definición.

### 6.3 Resumen: modelos pensados para bajo

| Pedal o bloque | Modelos para bajo |
|---|---|
| Overdrive | Bass Drive |
| Distorsión | Bass Distortion |
| Fuzz | Bass Fuzz |
| Amplificador | Bass Tube, Bass Clean, Bass Rock |
| Pantalla | Bass 8x10, Bass 1x15 |

Los tres pedales "Bass" conservan los graves limpios y saturan solo el resto, para que el bajo no pierda cuerpo.

---

## 7. Amplificadores y pantallas

### 7.1 El amplificador

El ampli es un modelo con varias etapas de válvula, un control de tonos calculado desde un circuito y una etapa de potencia que "se hunde" un poco cuando le pides mucho (sag). El desplegable **Modelo** elige entre nueve. Los nombres son genéricos y no corresponden a marcas.

| Modelo | Para | Carácter, según sus ajustes internos |
|---|---|---|
| **Clean American** | Guitarra | Dos etapas con poca ganancia, mucho margen limpio y control de tonos de tipo americano. Es el modelo por defecto. |
| **British Crunch** | Guitarra | Tres etapas, ganancia media-alta y control de tonos de tipo británico. Pasa de limpio a crunch al subir **Gain**. |
| **High Gain** | Guitarra | Cuatro etapas y mucha ganancia, con control de tonos de tipo moderno. |
| **Tweed** | Guitarra | Tres etapas, mucho sag: se comprime y se ensucia con facilidad. Bueno para blues y rock suave. |
| **Class A Chime** | Guitarra | Dos etapas con respuesta en agudos más abierta y mucho sag. Sonido brillante. |
| **Modern Lead** | Guitarra | Cuatro etapas, la mayor ganancia de la lista, con la parte alta más recortada para solos. |
| **Bass Tube** | Bajo | Tres etapas de válvula con respuesta extendida en graves. |
| **Bass Clean** | Bajo | Dos etapas, sin sag: es el más limpio. |
| **Bass Rock** | Bajo | Tres etapas con más ganancia, para un bajo rugoso. |

### 7.2 Controles del ampli

| Control | Qué hace |
|---|---|
| **Gain** | Ganancia del previo: cuánto satura. |
| **Bass** | Graves. |
| **Mid** | Medios. |
| **Treble** | Agudos. |
| **Presence** | Brillo en la etapa de potencia (agudos altos). |
| **Master** | Volumen de la etapa de potencia; al subirlo también satura y comprime más. |
| Interruptor **POWER** | Enciende o apaga el ampli. |

Consejo: para un sonido limpio, sube **Master** y baja **Gain**; para un sonido saturado, sube **Gain** y ajusta el volumen con **Master** o con **Salida**.

### 7.3 Pantallas integradas

La pantalla simula el altavoz y la caja, que dan buena parte del carácter final. Sin pantalla, la salida del ampli modelado queda sin filtrar por el altavoz.

El desplegable **Integrada** de la sección PANTALLA elige entre cinco:

| Pantalla | Descripción, según su filtrado |
|---|---|
| **Guitar 4x12** | Guitarra. Graves con cuerpo, medios algo recortados, presencia marcada en torno a 2,6 kHz y corte de agudos a partir de unos 5 kHz. Es la pantalla por defecto. |
| **Guitar 2x12 Open** | Guitarra. Con menos graves y los medios agudos más adelantados. |
| **Guitar 1x12** | Guitarra. Sonido más compacto, con menos graves que la 4x12 y un realce en medios. |
| **Bass 8x10** | Bajo. Llega a graves muy bajos y corta agudos en torno a 4 a 5 kHz. |
| **Bass 1x15** | Bajo. Llega aún más abajo en graves y es la más oscura de agudos. |

### 7.4 Cargar tu propia respuesta a impulso (IR)

Una respuesta a impulso (IR) es un archivo que contiene el sonido de una pantalla y un micrófono reales. Puedes cargar la tuya en lugar de las integradas.

1. En la sección PANTALLA pulsa **Cargar IR...**.
2. En el cuadro *Seleccionar respuesta a impulso* elige un archivo **WAV o AIFF** (`.wav`, `.aif`, `.aiff`).
3. El nombre del archivo aparece bajo los botones. A partir de ese momento se usa la IR y el selector **Integrada** queda deshabilitado.
4. Para volver a una pantalla integrada, pulsa **Quitar**. El texto vuelve a *Sin IR*.

Notas:

- La IR se procesa sin añadir latencia.
- Se usa una sola respuesta, en mono.
- El volumen de la IR se normaliza al cargarla.
- Si el nombre del archivo no aparece tras elegirlo, no se ha cargado: prueba con otro archivo (ver [sección 13](#13-solución-de-problemas)).
- La IR se guarda en los presets por su ruta (ver 5.5).

---

## 8. Capturas neuronales (.nam)

### 8.1 Qué son

Una captura neuronal es un archivo `.nam` (formato Neural Amp Modeler) que contiene una red neuronal entrenada para imitar un ampli o un pedal real. EffectSuite puede reproducirlas. Una captura **sustituye al ampli modelado** de EffectSuite mientras esté cargada.

### 8.2 Dónde conseguirlas

En la web **TONE3000**, que reúne capturas `.nam` de amplis y pedales. Descarga el archivo `.nam` en una carpeta fija de tu disco.

### 8.3 Cargar una captura

1. En el cabezal AMPLIFICADOR pulsa **Cargar captura...**.
2. En el cuadro *Seleccionar captura Neural Amp Modeler* elige un archivo `.nam`.
3. El nombre de la captura aparece en el cabezal (antes ponía *Sin captura*).
4. Para volver al ampli modelado, pulsa **Quitar**.

Si el archivo no se puede cargar, aparece el aviso *No se pudo cargar la captura* con el motivo.

Una captura funciona igual de bien a cualquier frecuencia de muestreo de tu interfaz: EffectSuite la adapta internamente, así que suena igual con 44,1, 48 o 96 kHz.

### 8.4 Qué cambia en el cabezal con una captura cargada

- El desplegable **Modelo** queda deshabilitado (la captura sustituye al modelo).
- **Bass, Mid, Treble y Presence** quedan deshabilitados: la captura ya lleva su propio carácter y el control de tonos no interviene.
- El mando **Gain** pasa a llamarse **Entrada** y ajusta el nivel con que la señal entra a la captura (de -20 a +20 dB; el centro es 0 dB).
- El mando **Master** pasa a llamarse **Salida** y ajusta el nivel de la salida de la captura (de -20 a +20 dB; el centro es 0 dB).

Usa **Entrada** para mover cuánto satura la captura (si es de un ampli o de un pedal con saturación) y **Salida** para igualar el volumen.

Los pedales de antes del ampli, la pantalla y los de después siguen funcionando con la captura. El interruptor **POWER** del ampli también la enciende y apaga.

### 8.5 Cuándo apagar la pantalla

Hay capturas de solo el ampli y otras de "equipo completo", que ya incluyen el sonido del altavoz y del micrófono. Si la captura ya incluye la pantalla, **apaga el interruptor de la PANTALLA** o el sonido pasará por dos pantallas seguidas. Si es de un cabezal sin altavoz, déjala encendida o carga una IR.

### 8.6 Coste de CPU

Las capturas son lo que más CPU consume de EffectSuite. En equipos justos:

- Prefiere los amplis modelados de EffectSuite, o
- busca capturas ligeras (las marcadas como "nano" o "feather" en TONE3000).

Vigila el indicador **CPU** (ver [sección 10](#10-rendimiento-y-latencia)).

La captura cargada se guarda en los presets por su ruta (ver 5.5).

---

## 9. Afinador

El afinador está en la cabecera y funciona siempre, sin que tengas que activarlo.

### Cómo se lee

- A la izquierda se muestra la **nota** más cercana (por ejemplo, `E2`). Si no hay nota, se muestra `--`.
- La **escala** del centro va de -50 a +50 cents. La barra indica cuánto te desvías: a la izquierda estás bajo, a la derecha, alto.
- A la derecha se muestra la **frecuencia** en Hz (o `-- Hz`).
- Cuando la desviación es de 3 cents o menos, la nota se pone en **verde**.

El afinador es cromático y puede detectar notas graves, hasta el Si grave de un bajo de 5 cuerdas.

### Afinar sin que suene

1. Activa el interruptor **Silenciar**. Se silencia la salida, con un pequeño fundido.
2. Toca la cuerda al aire y ajusta la clavija hasta que la nota se ponga en verde.
3. Desactiva **Silenciar** al terminar.

Silenciar no corta el afinador, solo lo que oyes.

Para afinar bien, toca una sola cuerda cada vez y deja que suene. El afinador lee la señal de tu entrada, así que no depende de los pedales que tengas puestos. Si ves `--`, sube el volumen de **Entrada** o toca más fuerte.

El interruptor **Silenciar** es un ajuste de tu equipo: los presets no lo cambian.

---

## 10. Rendimiento y latencia

### 10.1 El selector Calidad

Los pedales de antes del ampli y el amplificador trabajan **sobremuestreados**: el audio se procesa a una frecuencia mayor para que las distorsiones fuertes no generen un tipo de ruido llamado *aliasing*. El selector **Calidad** de la cabecera fija ese factor:

| Calidad | Factor | Uso |
|---|---|---|
| **Mínimo (1x)** | Sin sobremuestreo | Mínima CPU; no añade latencia. Más aliasing en distorsiones fuertes. |
| **Eco (2x)** | 2 veces | Mitad de coste que Normal. |
| **Normal (4x)** | 4 veces | Valor por defecto. |
| **Alta (8x)** | 8 veces | Menos aliasing, mucha más CPU. |

Cada paso hacia abajo consume aproximadamente la mitad de CPU, a cambio de más aliasing en distorsiones fuertes. Cambiar de calidad silencia la salida durante un instante.

A frecuencias de muestreo muy altas el programa reduce por su cuenta el factor, porque ya no hace falta tanto.

La **Calidad** es un ajuste de tu equipo: los presets no la cambian.

### 10.2 El indicador CPU y los cortes

A la derecha de la barra de presets verás `CPU nn %`, la carga del procesado de audio, y un contador de **cortes** (`n cortes`) cuando el audio se ha interrumpido porque el equipo no llegó a tiempo.

| Color | Significado |
|---|---|
| Gris | Carga baja. |
| Ámbar | Carga de 60 % o más. |
| Rojo | Carga de 85 % o más: estás cerca de tener cortes. |

Si el indicador se acerca al 100 %, baja la calidad, quita pedales o sube el tamaño de búfer.

### 10.3 Consejos para equipos poco potentes

1. Baja **Calidad** a Eco o Mínimo.
2. Si usas una captura `.nam`, prueba con una más ligera o con un ampli modelado.
3. Quita de la pedalera los pedales que no uses.
4. Sube un paso el tamaño de búfer de la interfaz (de 64 a 128, por ejemplo).
5. Cierra otros programas, sobre todo navegadores y programas de vídeo.
6. Usa un driver de baja latencia (ASIO en Windows).

### 10.4 Qué determina la latencia

- **Driver y tamaño de búfer** de tu interfaz: es la parte mayor. A 48 kHz, un búfer de 64 muestras son unos 1,3 ms y uno de 128 muestras, unos 2,7 ms (por cada sentido, entrada y salida).
- **Calidad**: el procesado de EffectSuite añade solo unas pocas muestras (del orden de 0,1 ms). **Mínimo (1x)** no añade ninguna.
- Las respuestas a impulso (IR) no añaden latencia.

Si notas retardo, lo primero que debes tocar es el búfer y el driver.

---

## 11. MIDI

EffectSuite responde a dos tipos de mensajes MIDI.

| Mensaje | Qué hace |
|---|---|
| **Control Change 11** (expresión) o **Control Change 4** (pedal de pie) | Mueve la posición del wah (**Position**). Usa el wah en modo **Manual**. |
| **Program Change** | Carga un preset por número. El 0 es el primer preset de fábrica y sigue por la lista de fábrica en orden; después vienen tus presets, por orden alfabético. |

Con los 15 presets de fábrica, los números **0 a 14** cargan los de fábrica y el **15** y siguientes cargan tus presets de usuario.

### Conectar un pedal o controlador MIDI

En la aplicación independiente:

1. Conecta tu pedal de expresión o controlador MIDI al ordenador.
2. Abre **Options** y elige **Audio/MIDI Settings**.
3. Elige tu dispositivo como entrada MIDI.
4. En el pedal Wah, elige el modo **Manual** y enciende el pedal.
5. Mueve el pedal de expresión: **Position** se mueve con él.

Si usas el plugin en un DAW, el DAW debe enviarle los mensajes MIDI. El mando **Position** del wah también se puede automatizar desde el DAW.

---

## 12. Uso como plugin en un DAW

1. Instala el plugin con el instalador (ver [sección 2](#2-instalación)).
2. Abre tu DAW y vuelve a escanear los plugins si no aparece EffectSuite.
3. Inserta EffectSuite como efecto en una pista con la guitarra o el bajo grabados o en monitorización.

Cosas a tener en cuenta:

- Los ajustes de audio (driver, búfer, canales) son los del DAW. El menú *Audio/MIDI Settings* solo existe en la aplicación independiente.
- Los ajustes del plugin se guardan con el proyecto del DAW.
- El plugin informa a tu DAW de su latencia, para que pueda compensarla. Si cambias la **Calidad**, la latencia puede cambiar.
- Presets, IR y capturas funcionan igual que en la aplicación.
- Las entradas y salidas del plugin pueden ser mono o estéreo. EffectSuite procesa tu señal en mono y entrega la salida en estéreo (los efectos de después del ampli son estéreo).

---

## 13. Solución de problemas

### No suena nada

1. ¿Está **quitado el silencio de la entrada**? Si ves la franja amarilla *Audio input is muted to avoid feedback loop*, desactívalo (ver 3.3).
2. ¿Está activado **Silenciar** en la cabecera? Desactívalo.
3. ¿Se mueve el **medidor de entrada** al tocar? Si no, revisa el **Canal** (ver 3.4), el cable y que la guitarra esté en la entrada de instrumento. Mira también que la ganancia de la interfaz no esté a cero.
4. ¿Se mueve el medidor de salida pero no oyes nada? Revisa el dispositivo de salida en *Audio/MIDI Settings* y el volumen de tus auriculares o monitores.
5. ¿Está encendido el interruptor **POWER** del ampli? Y si usas una captura, ¿has subido **Salida**?
6. Comprueba que el nivel de **Salida** de la cabecera no esté en el mínimo.

### Suena con retardo

1. Usa un driver de baja latencia: **ASIO** en Windows.
2. Baja el tamaño de búfer a 64 o 128 muestras (ver 3.2).
3. Baja la **Calidad** (ver 10.1).
4. Si estás en un DAW, comprueba su configuración de búfer y evita el monitorizado a través de pistas con otros plugins pesados.

### Chasquidos o cortes

1. Mira el indicador **CPU**: si está en ámbar o rojo, o cuenta cortes, el equipo va justo.
2. Sube un paso el tamaño de búfer.
3. Baja **Calidad**, quita pedales o cambia una captura por un ampli modelado (ver 10.3).
4. Cierra otros programas.

### Acoples (pitidos)

1. Usa auriculares, o aleja el micrófono de los altavoces.
2. Baja **Salida** y la ganancia del ampli.
3. Activa la **Puerta** para cortar el ruido cuando no tocas.
4. Mantén la entrada silenciada hasta tener todo conectado (ver 3.3).

### Windows bloquea el instalador

Aparece el aviso de SmartScreen porque el instalador no está firmado. Pulsa **Más información** y después **Ejecutar de todas formas**.

### macOS bloquea el instalador

Si macOS dice que no puede abrir el paquete, no lo abras con doble clic: haz **clic derecho** (o Control + clic) sobre `EffectSuite.pkg` y elige **Abrir**.

### Una captura (.nam) no carga

1. Comprueba que el archivo tiene extensión `.nam` y que se ha descargado entero.
2. Lee el motivo en el aviso *No se pudo cargar la captura*.
3. Prueba con otra captura para descartar que el archivo esté dañado.
4. Si un preset no carga su captura, puede que hayas movido o renombrado el archivo (ver 5.5).

### Una IR no carga

1. Comprueba que es un archivo **WAV o AIFF** (`.wav`, `.aif` o `.aiff`).
2. Si tras elegirla bajo los botones sigue el texto *Sin IR*, el archivo no se ha cargado: prueba con otro archivo o conviértelo a WAV.
3. Si un preset no carga su IR, puede que hayas movido o renombrado el archivo (ver 5.5).

### El wah no responde a mi pedal

Comprueba que el wah está **encendido**, en modo **Manual**, y que tu dispositivo MIDI está elegido en *Options → Audio/MIDI Settings*. El pedal debe enviar Control Change 11 o 4 (ver [sección 11](#11-midi)).

---

## 14. Limitaciones conocidas

- **El octavador es monofónico.** Solo funciona bien con notas sueltas. Con acordes o varias cuerdas a la vez, el resultado no es fiable.
- **Los pedales no pueden cruzar al otro lado del ampli.** Los ocho de antes (Puerta, Wah, Compresor, Octavador, Phaser, Overdrive, Distorsión, Fuzz) siempre van antes del ampli, y los seis de después (Ecualizador, Flanger, Chorus, Tremolo, Delay, Reverb) siempre después. Solo puedes reordenarlos dentro de su fila. Por la misma razón, no hay lazo de efectos con el ampli ni se puede poner, por ejemplo, un chorus antes del ampli.
- **Cada tipo de pedal solo existe una vez** en la pedalera.
- **El fuzz es un modelo de comportamiento**, no una simulación de un circuito concreto, a diferencia del overdrive y la distorsión.
- **Los instaladores no están firmados**, por eso Windows y macOS muestran avisos la primera vez.
- **Los nombres de los modelos son genéricos** (por ejemplo, "Clean American", "British Crunch", "Rodent") y no corresponden a marcas ni a equipos concretos. Descríbelos por lo que hacen, no por lo que recuerdan.
- Con una captura neuronal el control de tonos del ampli no interviene.
- Los presets guardan la ruta de las IR y capturas, no los archivos.

---

## 15. Licencia

EffectSuite se distribuye bajo la licencia **AGPL-3.0**.

EffectSuite usa JUCE (AGPLv3 o licencia comercial), Neural Amp Modeler Core (MIT) para reproducir las capturas `.nam` y Eigen (MPL 2.0).
