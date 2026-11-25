# AC Songlist · Instructivo de uso

Guía paso a paso para usar el plugin en un directo. Si aún no lo instalaste, mira la sección *Instalación* del [README](README.md).

## En resumen

1. Armas tu **biblioteca** de canciones dentro de OBS.
2. Tu público escribe **`!lista`** y **`!sr`** en el chat para pedir.
3. Tú pulsas **Tocar ahora** y la canción aparece en el stream.

No hace falta iniciar sesión en Twitch ni ningún token: solo el nombre de tu canal.

---

## 1. Abrir el panel

En OBS, menú superior **Paneles → AC Songlist**. Aparece una ventana flotante; arrástrala a un lado de OBS para dejarla acoplada.

Arriba del panel verás siempre dos cosas:

- **El estado del chat** (punto de color y texto).
- **El botón Pedidos abiertos / Pedidos cerrados**, para aceptar o no pedidos del chat.

## 2. Conectar tu canal

1. Pestaña **Ajustes**.
2. En **Canal de Twitch** escribe tu canal (vale `tu_canal` o el enlace completo).
3. Pulsa **Guardar y conectar**.

| Punto | Significado |
|---|---|
| 🟢 Verde, «conectado, leyendo pedidos» | Todo bien |
| 🟡 Amarillo, «Conectando…» | Espera unos segundos |
| 🔴 Rojo, «Sin conexión, reintentando…» | Revisa tu internet o el nombre del canal; reintenta solo |
| ⚪ Gris, «Chat sin configurar» | Falta escribir el canal |

## 3. Armar la biblioteca

Pestaña **Canciones**.

**Agregar una a una (lo más rápido):**
1. Escribe el título y pulsa **Enter**.
2. Escribe el artista y pulsa **Enter**.

La canción queda guardada con un **número fijo**. Ese número es el que usará tu público para pedirla.

**Importar muchas de golpe:** botón **CSV → Importar CSV…**. Acepta:
- Un CSV con columnas `título` y `artista` (y opcionalmente `categorías`).
- Un archivo de texto con una canción por línea: `Título - Artista`.
- El archivo que exporta StreamerSonglist.

Las repetidas no se duplican. Si borras **toda** la biblioteca y vuelves a importar, la numeración empieza otra vez en 1.

**Otras acciones:**
- **Buscar:** escribe en el buscador título, artista o número. No importan acentos ni mayúsculas.
- **Filtrar por categoría:** desplegable a la derecha del buscador.
- **Editar:** doble clic sobre la canción. Ahí puedes poner categorías (separadas por comas), una nota solo para ti, o **desactivarla** para que el público no pueda pedirla sin tener que borrarla.
- **Eliminar:** selecciona una o varias y pulsa **Eliminar**.
- **Copia de seguridad:** **CSV → Exportar CSV…**

## 4. Poner el overlay en tu escena

En tu escena: **Fuentes → + → AC Songlist**. Al crearla se abren sus propiedades; en **Qué mostrar** elige una de las tres:

| Modo | Qué muestra |
|---|---|
| **Sonando ahora** | Tarjeta con la canción que estás tocando y quién la pidió. Si no suena nada, no se ve. |
| **Cola de pedidos** | Los próximos pedidos, en orden, con quién los hizo. |
| **Lista de canciones** | Tu biblioteca numerada, por páginas. |

Puedes agregar la fuente **varias veces**, una por cada modo, y colocar cada una donde quieras.

**Recomendado para la lista:** activa **«Mostrar solo cuando alguien escribe !lista»**. Así no ocupa pantalla todo el tiempo y aparece cuando la piden.

**Skins.** En las propiedades de la fuente, **Skin** permite elegir entre *Minimalista* y tres retro: *casete*, *equipo de sonido* y *vinilo*. Los retro tienen su propia paleta y solo toman tu **color de acento**; si no quieres movimiento, desmarca **Animar**.

**Cambiar el aspecto** (skin minimalista, en las propiedades de cada fuente): tipografía y tamaño, color del texto, color de acento, color y opacidad del fondo (0 = sin fondo), esquinas redondeadas, alineación, filas y columnas, y qué datos mostrar.

## 5. En directo

**Lo que escribe tu público:**

| Comando | Qué hace |
|---|---|
| `!lista` | Muestra la lista de canciones en pantalla |
| `!sr 12` | Pide la canción número 12 |
| `!sr dancing queen` | Pide por nombre (sirve título, artista o ambos) |
| `!quitar` | Retira su último pedido |

También funcionan `!pedir` y `!request` en lugar de `!sr`, y `!canciones` en lugar de `!lista`.

**Lo que haces tú**, en la pestaña **Cola**:

1. Cuando vayas a tocar el siguiente pedido, pulsa **▶ Siguiente**. La tarjeta aparece en el stream.
2. Para tocar otro pedido que no sea el primero, selecciónalo y pulsa **▶ Tocar ahora** (o doble clic).
3. Al terminar la canción:
   - **▶ Siguiente**: la da por tocada y empieza la próxima de la cola.
   - **✓ Terminar**: la da por tocada y oculta la tarjeta, sin empezar otra.
   - **Quitar**: la quita sin contarla como tocada.

**Tocar algo que nadie pidió:** pestaña **Canciones**, selecciona la canción y pulsa **▶ Tocar ahora**.

**Agregar un pedido tú mismo:** pestaña **Canciones**, selecciona y pulsa **+ A la cola**.

**Ordenar la cola:** selecciona un pedido y usa las flechas ▲ ▼, o **Quitar** para sacarlo.

**Pausar pedidos:** botón **Pedidos abiertos** arriba del panel. Al pulsarlo pasa a **Pedidos cerrados** y el chat no puede pedir hasta que lo vuelvas a abrir.

**Tus moderadores también pueden**, desde el chat: `!abrir`, `!cerrar`, `!tocada` (pasa a la siguiente) y `!saltar` (quita la actual).

> El plugin **no escribe en el chat**. El público ve el resultado de su pedido en el stream: en la cola y en la tarjeta.

## 6. Reglas de los pedidos

Pestaña **Ajustes → Reglas de los pedidos**:

- **Pedidos a la vez por persona:** cuántos puede tener en cola cada usuario (por defecto 2).
- **Máximo de canciones en cola:** tope total de la cola.
- **Segundos que se muestra la lista:** cuánto dura en pantalla tras un `!lista`.
- **Aceptar canciones que no están en la biblioteca:** si lo activas, un `!sr` con algo que no tienes entra igual a la cola con el texto que escribieron.

Si dos personas piden la misma canción, no se duplica: la segunda se suma al pedido y aparecen los dos nombres.

## 7. Página web pública

Si quieres que tu público vea la lista completa fuera del stream:

1. Pestaña **Ajustes → Página web pública**.
2. Marca **Publicar mi lista en la web** y pulsa **Guardar y conectar**.
3. Copia el enlace con **Copiar enlace** y compártelo (en el panel del canal, en un comando del chat, en redes).

La página tiene buscador y filtro por categoría, muestra la canción que suena y la cola, y **se actualiza sola**: cuando agregas una canción en OBS, aparece resaltada a los pocos segundos. Al tocar una canción se copia su comando (`!sr 12`).

- Solo se publican las canciones activas; tus notas privadas no salen del equipo.
- Para dejar de publicar, desmarca la opción y guarda: la lista se borra de la web.
- La página queda ligada a este equipo por una clave que el plugin guarda con tus datos. Si cambias de ordenador, lleva también el archivo de datos (ver *Preguntas frecuentes*).

## 8. Atajos de teclado

En OBS: **Ajustes → Atajos**, busca «AC Songlist». Puedes asignar teclas a:

- Siguiente canción
- Terminar canción actual
- Abrir/cerrar pedidos
- Mostrar la lista en pantalla

Útil para no soltar el instrumento: con un pedal o una tecla pasas a la siguiente.

## 9. Historial

Pestaña **Historial**: qué tocaste, cuándo y quién lo pidió. La biblioteca guarda además cuántas veces tocaste cada canción (columna **Veces**) y la última fecha (pasa el ratón por encima).

---

## Preguntas frecuentes

**No aparece el panel en el menú Paneles.**
El plugin no se cargó. En OBS: **Ayuda → Archivos de registro → Ver registro actual** y busca `ac-songlist`. En Mac, revisa que ejecutaste el segundo comando de `LEEME-MAC.txt` (el que quita el bloqueo de macOS).

**El punto está en rojo.**
Revisa el nombre del canal y tu conexión. El plugin reintenta solo cada pocos segundos.

**Alguien pidió y no entró a la cola.**
Comprueba que los pedidos estén abiertos, que esa persona no haya llegado a su límite, y que la canción exista y esté activa. Si escribió un nombre muy distinto al de tu biblioteca, no la encuentra.

**La tarjeta no sale en el stream.**
Solo se ve mientras hay una canción «sonando»: pulsa **Tocar ahora** o **Siguiente**. Revisa también que la fuente esté en la escena activa y visible (el ojo en la lista de fuentes).

**¿Dónde se guardan mis canciones?**
- Windows: `%APPDATA%\obs-studio\plugin_config\ac-songlist\songlist.json`
- macOS: `~/Library/Application Support/obs-studio/plugin_config/ac-songlist/songlist.json`
- Linux: `~/.config/obs-studio/plugin_config/ac-songlist/songlist.json`

Actualizar o reinstalar el plugin no las borra.

**¿Funciona con Kick o YouTube?**
No, por ahora solo lee el chat de Twitch.
