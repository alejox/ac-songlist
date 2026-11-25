# AC Songlist · pedidos de canciones para OBS Studio

Plugin nativo de OBS (C++/Qt) para streamers que tocan en directo. Sin servicios externos:

1. **Tú armas la biblioteca** dentro de OBS, canción a canción o importando un CSV.
2. **Tu público elige**: escribe `!lista` para ver las canciones en pantalla y `!sr 12` o `!sr nombre` para pedir una.
3. **Cuando la tocas, se muestra** en el stream con una tarjeta minimalista de «Sonando ahora».

![Tarjeta Sonando ahora](docs/sonando-ahora.png)

![El plugin dentro de OBS](docs/obs.png)

## Qué incluye

- **Panel acoplable** (menú *Paneles → AC Songlist*) con cuatro pestañas:
  - **Cola**: pedidos pendientes, quién los hizo, reordenar, quitar, *Tocar ahora*.
  - **Canciones**: alta rápida (título, Enter, artista, Enter), buscador sin acentos ni mayúsculas, categorías, editar, desactivar, importar y exportar CSV.
  - **Historial**: qué se tocó, cuándo y para quién.
  - **Ajustes**: canal de Twitch, límites de pedidos y respuestas en el chat.
- **Fuente de vídeo «AC Songlist»** para tus escenas, con tres modos:
  - *Sonando ahora*: la tarjeta de la canción actual, con fundido al cambiar.
  - *Cola de pedidos*: los siguientes pedidos y quién los pidió.
  - *Lista de canciones*: la biblioteca numerada, por páginas. Puede estar siempre visible o aparecer solo cuando alguien escribe `!lista`.
- **Skins retro** además del minimalista: casete (con carretes que giran), equipo de sonido (display de puntos con ecualizador y marquesina) y vinilo (disco girando). Sirven para la tarjeta, la cola y la lista.
- **Estilo neutro y configurable** desde las propiedades de la fuente: tipografía, tamaño, colores de texto, acento y fondo, opacidad (0 = sin fondo), esquinas, márgenes, alineación, encabezado, filas y columnas.
- **Chat de Twitch** sin iniciar sesión ni tokens: solo escribes el nombre de tu canal. La conexión va cifrada y es de solo lectura.
- **Atajos de teclado** en *Ajustes → Atajos*: siguiente canción, terminar la actual, abrir/cerrar pedidos, mostrar la lista.

![Cola y lista](docs/cola-y-lista.png)

![Skins retro](docs/skins.png)

## Instalación

Requiere **OBS Studio 30.2 o superior** (64 bits).

**Windows.** Descarga `AC-Songlist-Windows.zip` desde [Releases](https://github.com/alejox/ac-songlist/releases/latest), descomprime y ejecuta `instalar.bat` con OBS cerrado. A mano: copia la carpeta `ac-songlist` a `C:\ProgramData\obs-studio\plugins\`.

**macOS** (Apple Silicon e Intel). Descarga `AC-Songlist-macOS.tar.gz` desde el mismo sitio, descomprime y, con OBS cerrado, abre Terminal, escribe `bash ` y arrastra `instalar.command` a la ventana. El plugin no está firmado con una cuenta de desarrollador de Apple, por eso se instala desde Terminal; los detalles y la instalación a mano están en `LEEME-MAC.txt`.

**Linux.** Descarga `AC-Songlist-Linux.tar.gz` y copia la carpeta `ac-songlist` a `~/.config/obs-studio/plugins/`.

Después abre OBS y activa el panel en *Paneles → AC Songlist*.

## Uso

**Guía completa paso a paso: [INSTRUCTIVO.md](INSTRUCTIVO.md)** · [en PDF](docs/AC-Songlist-Instructivo.pdf). Lo esencial:

1. **Biblioteca.** Pestaña *Canciones*: escribe título y artista y pulsa Enter. Cada canción recibe un número fijo, que es el que usa el público para pedirla.
2. **Chat.** Pestaña *Ajustes*: escribe tu canal de Twitch y pulsa *Guardar y conectar*.
3. **Overlay.** En tu escena, *Fuentes → + → AC Songlist*, y elige qué mostrar. Agrega la fuente tantas veces como quieras (por ejemplo, una para *Sonando ahora* y otra para la *Lista*).
4. **En directo.** Cuando vayas a tocar un pedido, pulsa **Siguiente** (o **Tocar ahora** sobre uno concreto). También puedes tocar una canción de la biblioteca sin que nadie la pida. Al acabar, **Terminar** o **Siguiente**.

### Comandos del chat

| Comando | Alias | Qué hace |
|---|---|---|
| `!sr <número o nombre>` | `!pedir`, `!request` | Pide una canción |
| `!lista` | `!canciones`, `!list` | Muestra la lista en pantalla |
| `!quitar` | `!wrongsong`, `!ws` | Retira tu último pedido |
| `!abrir` / `!cerrar` | `!sropen` / `!srclose` | Abre o cierra pedidos (streamer y mods) |
| `!tocada` | `!srnext` | Termina la actual y pasa a la siguiente (streamer y mods) |
| `!saltar` | `!srskip` | Quita la actual sin contarla (streamer y mods) |

El plugin no escribe en el chat: el resultado de cada pedido se ve en el stream (en la cola y en la tarjeta).

La búsqueda por nombre tolera acentos, mayúsculas, palabras sueltas y erratas pequeñas (`!sr sientist coldplay` encuentra *The Scientist*). Si dos personas piden la misma canción, la segunda se suma al pedido en vez de duplicarlo.

### Importar canciones (CSV)

Botón **CSV → Importar**. Acepta:

- Un CSV con cabeceras `title`/`título`, `artist`/`artista` y, opcionalmente, `attributeNames`/`categorías`, `active`, `timesPlayed`, `lastPlayed`, `comment`. Es el formato que exporta **StreamerSonglist**, así que migrar desde ahí es exportar allí e importar aquí.
- Un CSV sin cabeceras: título, artista, categorías.
- Texto plano con una canción por línea: `Título - Artista`.

Las canciones repetidas (mismo título y artista) no se duplican.

## Dónde se guardan los datos

- Windows: `%APPDATA%\obs-studio\plugin_config\ac-songlist\songlist.json`
- macOS: `~/Library/Application Support/obs-studio/plugin_config/ac-songlist/songlist.json`
- Linux: `~/.config/obs-studio/plugin_config/ac-songlist/songlist.json`

Biblioteca, cola, historial y ajustes van en ese archivo. Reinstalar o actualizar el plugin no lo toca.

## Compilar

```bash
sudo apt-get install cmake ninja-build g++ libobs-dev qt6-base-dev
```

```bash
cmake -S . -B build -G Ninja -DACSL_BUILD_TESTS=ON
```

```bash
cmake --build build
```

```bash
QT_QPA_PLATFORM=offscreen ./build/acsl-tests
```

En Windows y macOS lo compila [el flujo de GitHub Actions](.github/workflows/build.yml), que arma un SDK mínimo de OBS (cabeceras de la versión 30.2.3 y, para enlazar, los binarios de la versión oficial) y usa Qt 6.6. El de macOS es universal (arm64 + x86_64).

### Estructura

| Carpeta | Contenido |
|---|---|
| `src/core/` | Biblioteca, cola, búsqueda, CSV y comandos de chat. Solo QtCore, con pruebas. |
| `src/chat/` | Cliente del chat de Twitch (IRC cifrado). En Windows y macOS usa la red del sistema (WinHTTP / Network.framework), porque el Qt que trae OBS ahí no incluye TLS. |
| `src/overlay/` | Dibujado del overlay y la fuente de vídeo de OBS. |
| `src/ui/` | Panel acoplable. |
| `tests/` | Pruebas del núcleo (con un servidor de chat simulado), prueba de conexión real con Twitch y generador de imágenes de muestra. |

## Límites conocidos

- Solo lee el chat de **Twitch** (no Kick ni YouTube), y no escribe en él.
- No tiene página web pública de la lista: el público la ve en el stream con `!lista`.
- Probado dentro de OBS 30 en Linux. Las versiones de Windows y macOS se compilan y pasan las pruebas del núcleo en GitHub Actions, pero no se han ejecutado dentro de OBS en esos sistemas antes de publicarlas aquí: si algo falla al cargarlo, el registro de OBS (*Ayuda → Archivos de registro*) lo dirá en las líneas `[ac-songlist]`.
