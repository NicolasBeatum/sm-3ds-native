# sm-3ds-native

[English](README.md) · [Descargas](https://github.com/NicolasBeatum/sm-3ds-native/releases/latest) · [Registro de cambios](CHANGELOG.md)

> [!IMPORTANT]
> **Port no oficial desarrollado con una participación amplia de IA.** NicolasBeatum dirige y prueba este proyecto; OpenAI Codex se ha usado para analizar código, implementar cambios, depurar, optimizar y escribir documentación. Es totalmente comprensible y respetable que alguien prefiera no jugar este port por haber utilizado IA. Este no es un lanzamiento oficial de Nintendo ni de los proyectos originales.

Port de Super Metroid para Nintendo 3DS, basado en [CharlesAverill/sm-3ds](https://github.com/CharlesAverill/sm-3ds) y en el [port de PC y decompilación de snesrev](https://github.com/snesrev/sm), con una pantalla inferior inspirada en MetroidArch.

## Estado actual

**Es jugable, pero sigue en desarrollo.** NicolasBeatum todavía no ha terminado una partida completa ni completado el juego al 100% con este port. No podemos confirmar que se pueda terminar de principio a fin: podrían quedar problemas en zonas avanzadas, jefes o el final. El rendimiento depende de la consola, la sala, los efectos y el modo panorámico; alcanzar 60 FPS es un objetivo, no una garantía.

Los mismos paquetes funcionan en Old y New 3DS. En New 3DS se activan automáticamente la mayor frecuencia de CPU y la caché L2. Las optimizaciones conservan la imagen, la paleta, el sonido estéreo y la jugabilidad del juego.

## Capturas

Tomadas en **Azahar**; estas imágenes no demuestran el rendimiento en una consola real. Incluyen distribuciones de las dos pantallas en vertical y una junto a la otra.

| Introducción | Zona de aterrizaje |
| --- | --- |
| ![Introducción en Azahar](screenshots/azahar/intro.png) | ![Zona de aterrizaje y mapa de área en Azahar](screenshots/azahar/landing-site.png) |
| Cuevas y mapa | Pantalla de equipamiento |
| ![Cuevas y mapa de área en Azahar](screenshots/azahar/caves-map.png) | ![Equipamiento en la pantalla inferior de Azahar](screenshots/azahar/items.png) |

![Crateria y mapa en la pantalla inferior de Azahar](screenshots/azahar/crateria-map.png)

## Funciones

- Lógica nativa del juego y sonido estéreo a 32 kHz.
- Renderizado mediante PICA200 en los cuadros compatibles con Mode 1, con una ruta exacta por CPU cuando el estado gráfico no es compatible o cambia dentro del cuadro.
- Modo panorámico opcional con escenario ampliado, sprites, iluminación HDMA y temblores de cámara.
- HUD inferior configurable: ONLY AMMO, AMMO + HOOK o ALL ITEMS, números verticales/horizontales, espacios reservados para objetos no adquiridos y botón de X-Ray opcional en el mapa. También muestra equipamiento, ilustración Redux, porcentaje de objetos y tiempo de partida.
- Mapas de área y del mundo con arrastre táctil, zoom, centrado y seguimiento de Samus, nombres de áreas y marcadores. Los botones flotantes se pueden ocultar desde SETUP.
- SETUP agrupado en HUD/MAP/VIDEO, interruptores de barra de estado por pestaña y modos FIT/STRETCHED/1:1 para la pantalla superior. Las preferencias se guardan en la microSD.
- Selector de ROM al iniciar, partidas guardadas independientes por ROM y comprobaciones de compatibilidad con la traducción española 1.0 de Klint/Pacochan. El selector todavía no aplica parches IPS.
- **SETUP → i** muestra el modelo detectado (Old/New 3DS), información de compilación y el botón **SAVE DUMP**. También se pueden generar dumps con **L + R + A**, organizados en carpetas con fecha e identificador.
- Dumps con estadísticas de toda la sesión y de los últimos cuadros, mediciones separadas del juego con y sin modo panorámico, WRAM/SRAM y capturas de las pantallas cuando están disponibles.
- Banner animado de Samus y su nave en el menú HOME del CIA, con tres segundos de la música de apertura. Se conserva el icono pequeño del menú HOME.

## Building / descarga

**Para jugar, descarga las compilaciones listas para instalar desde [Releases](https://github.com/NicolasBeatum/sm-3ds-native/releases/latest).** La versión [0.1.5](https://github.com/NicolasBeatum/sm-3ds-native/releases/tag/v0.1.5) incluye:

- `.3dsx` y `.smdh` para Homebrew Launcher.
- `.cia` para instalar con FBI.
- Código QR para **FBI → Remote Install → Scan QR Code**.

![Código QR de FBI para el CIA v0.1.5](docs/assets/fbi-v0.1.5.png)

Coloca tu propia ROM compatible `.smc` o `.sfc` en `sdmc:/3ds/sm3dsnative/` y selecciónala al iniciar. La carpeta se crea automáticamente. Dentro de ella, las partidas se guardan en `saves/`, la configuración en `settings.cfg` y los dumps en `dump/<fecha-id>/`. Ninguna compilación incluye una ROM del juego ni un parche de traducción. Las partidas y los ajustes guardados se mantienen al actualizar.

Las instrucciones para compilar, preparar la traducción y analizar dumps están en [Building y diagnósticos](docs/BUILDING.md), en inglés. La arquitectura y el historial de verificaciones están en [Notas del port](docs/PORTING_NOTES.md), también en inglés.

## Origen del código y declaración sobre IA

Este repositorio integra varios proyectos independientes; no se presenta como un trabajo enteramente original:

| Proyecto | Participación |
| --- | --- |
| [CharlesAverill/sm-3ds](https://github.com/CharlesAverill/sm-3ds) | Base del port para Nintendo 3DS. |
| [CharlesAverill/sm-3ds-lib](https://github.com/CharlesAverill/sm-3ds-lib) | Submódulo de la lógica nativa y decompilación usado por el port. |
| [snesrev/sm](https://github.com/snesrev/sm) | Decompilación original de Super Metroid y port de PC. |
| [libsdl-org/SDL](https://github.com/libsdl-org/SDL) | Capa de plataforma, entrada y sonido. |
| [Raekwon1603/RetroArch, rama `metroidarch-dual-screen`](https://github.com/Raekwon1603/RetroArch/tree/metroidarch-dual-screen) | Referencia visual de la pantalla inferior y origen de código de interfaz, decodificación de datos de la ROM e ilustración Redux. |
| [EstebanPdN/zelda-alttp-3ds](https://github.com/EstebanPdN/zelda-alttp-3ds) | Referencia para organizar dumps y estadísticas de rendimiento de toda la sesión. |

La implementación de la pantalla inferior y los datos de la ilustración Redux derivan de la rama MetroidArch bajo GPL-3.0. El port integrado se distribuye bajo GPL-3.0; se conserva el aviso MIT de CharlesAverill en `LICENSE.MIT`, y los submódulos mantienen sus licencias originales. Consulta [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) para conocer el origen del código y los recursos. Los bytes de la ilustración Redux fueron extraídos de una ROM modificada; la licencia GPL del código no establece la propiedad de esos gráficos. Super Metroid, su música y sus gráficos pertenecen a Nintendo y a sus respectivos titulares.

Los submódulos públicos son los forks [NicolasBeatum/sm-3ds-lib-native-fork](https://github.com/NicolasBeatum/sm-3ds-lib-native-fork) y [NicolasBeatum/SDL-3DS-native-fork](https://github.com/NicolasBeatum/SDL-3DS-native-fork). Sus relaciones de fork identifican los proyectos originales; las ramas de `.gitmodules` contienen los cambios específicos del port.

NicolasBeatum aporta la dirección del proyecto y las pruebas de aceptación. OpenAI Codex se ha usado ampliamente para borradores de implementación, comparación de código, análisis de rendimiento, integración de PICA200 y la interfaz inferior, diagnóstico de errores, pruebas automatizadas y documentación. Las decisiones de aceptar cambios y publicar corresponden al responsable humano del repositorio. El uso de IA no cambia las licencias, los derechos de autor ni las atribuciones obligatorias de los proyectos originales.

Si prefieres no jugar un port desarrollado con ayuda de IA, esa decisión se entiende y se respeta totalmente. Esta declaración permite decidir con esa información disponible.
