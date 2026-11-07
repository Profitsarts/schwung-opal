# Opal para Ableton Move (Schwung)

Una estación de síntesis de bolsillo para el Ableton Move, hecha como módulos de
[Schwung](https://github.com/charlesvestal/schwung). El sonido sale de los motores de
[Felucca](https://github.com/hugelton/Felucca), el firmware libre del M-VAVE FM-1, a través de su fork
[ChoralRoot FM-1](https://github.com/Quixotic7/ChoralRootFM1). Las pantallas son propias, dibujadas para la
OLED de 128×64 del Move.

> Proyecto independiente. No está afiliado con Ableton, M-VAVE, Hügelton Instruments, Teenage Engineering ni el
> proyecto Schwung. No incluye firmware, sonidos, presets ni gráficos de ningún producto comercial.

## Opal Synth v0.2

Un `sound_generator` de Schwung con **11 motores de Felucca**: ANALOG, FM6, PHASE, LOFI, VOICE, TRIO, WHEEL,
GRAIN, PHYS, NOISE y DRUM, cada uno con sus presets de fábrica (los de Felucca, GPL).

| Página | Knobs |
| --- | --- |
| Motor (con el navegador de presets) | Los 8 valores EDIT del motor, con sus nombres (por ejemplo WAVE, DTN, MIX, NOIS, CUT, RES, DRV, KTR en ANALOG) |
| Envelope | Motor, Attack, Decay, Sustain, Release, Level |
| FX | Drive, Chorus, Delay y Reverb (envíos de la pista), tiempo y feedback del delay, tamaño y amortiguación de la reverb |
| FX+ | Color y mezcla del delay, tipo de reverb (Room o Spring), velocidad y profundidad del chorus |

- Las páginas cambian de nombres y rangos al cambiar de motor.
- Guarda y recupera el sonido completo (motor, preset, valores y envolvente) con los presets de módulo de Schwung.
- Pitch bend de ±2 semitonos.

**Pantallas animadas** (`src/canvas.js`, páginas canvas de Schwung). La primera página de cada motor es un dibujo
que se mueve con los knobs y con lo que suena, y es también el navegador de presets (gira el jog para cambiar de
preset). La página FX empieza con los ecos del delay y la nube de la reverb.

| Motor | Dibujo |
| --- | --- |
| ANALOG | Tres ondas apiladas: WAVE da la forma, DTN las separa, CUT redondea las esquinas y RES las hace rebotar |
| FM6 | Un planeta con lunas en órbita y, a la derecha, las bandas laterales de la FM |
| PHASE | Un coseno que se dobla hacia diente de sierra o pulso con DCW |
| LOFI | Escalones de píxeles gordos: DUTY el pulso, CRSH el tamaño del píxel, ARP salta de nota |
| VOICE | Una boca que cambia de vocal y el pulso de la garganta |
| TRIO | Las tres voces, cada una en su intervalo |
| WHEEL | Una rueda de tonos que gira con el rotor, con las bocinas a los lados |
| GRAIN | Una nube de granos que sale de la línea de lectura |
| PHYS | Una cuerda entre dos clavijas que vibra al tocar |
| NOISE | Un campo de puntos que se concentra en una banda con FREQ y RES |
| DRUM | Un parche visto desde arriba que lanza anillos con cada golpe |

Los dibujos salen de cómo suena cada motor; no copian gráficos de ningún otro instrumento. `scripts/test.sh` los
dibuja todos en el ordenador y deja una hoja de muestra en `build/opal_screens.png`.

**Hasta 4 pistas a la vez**, una por pista del Move, cada una con su motor, su sonido y sus efectos. Cada
instancia es una copia completa del motor de Felucca: el código de Felucca guarda su estado en variables globales,
así que se compila cuatro veces (`src/dsp/opal_core0.c` .. `opal_core3.c`) sin tocarlo. Una quinta instancia
muestra un error y queda en silencio.

## Instalar

Desde el **Schwung Manager**: busca Opal Synth en la lista de módulos e instálalo. También puedes descargar
`opal-synth-module.tar.gz` de [Releases](https://github.com/Profitsarts/schwung-opal/releases) y descomprimirlo en
`/data/UserData/schwung/modules/sound_generators/` del Move.

## Publicar una versión

1. Sube `version` en `src/module.json` (por ejemplo `0.1.0`) y fusiona el PR.
2. En GitHub, **Releases → Draft a new release**, escribe la etiqueta `v0.1.0` y elige **Create new tag on publish**.
3. Al publicar, la acción `release` compila el módulo, adjunta `opal-synth-module.tar.gz` a la release y
   actualiza `release.json` en `main`, que es lo que lee el Schwung Manager.

La etiqueta tiene que coincidir con la versión de `src/module.json`; si no, la acción falla sin publicar nada.

## Compilar, probar e instalar a mano

```sh
./scripts/test.sh     # prueba en el ordenador: los 11 motores suenan; escribe build/opal_analog.wav
./scripts/build.sh    # dsp.so aarch64 + dist/opal-synth-module.tar.gz (gcc cruzado, cc nativo en arm64 o Docker)
./scripts/install.sh  # copia el módulo al Move por SSH (move.local)
```

A mano: copia `dist/opal-synth/` a `/data/UserData/schwung/modules/sound_generators/opal-synth/`.

## Plan

Plan por fases:

| Fase | Módulo | Contenido |
| --- | --- | --- |
| 1 | Opal Synth | Motores analógico, VA, FM6 y distorsión de fase, con pantallas Motor y Envolvente |
| 2 | Opal FX, Opal Synth | Efectos de Felucca; motores chip, grain, wheel y trio |
| 3 | Opal Drum | Kit de 8 carriles, sample player, slicer y carga de muestras del usuario |
| 4 | Opal Station (opcional) | Secuenciador, mezclador y proyectos a pantalla completa |

## Licencia

GPL-3.0-only, igual que Felucca. Si distribuyes Opal o algo derivado, tienes que dar también su código
completo con la misma licencia. Autoría y licencias de terceros en [CREDITS.md](CREDITS.md).
