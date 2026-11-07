# Opal para Ableton Move (Schwung)

Una estación de síntesis de bolsillo para el Ableton Move, hecha como módulos de
[Schwung](https://github.com/charlesvestal/schwung). El sonido sale de los motores de
[Felucca](https://github.com/hugelton/Felucca), el firmware libre del M-VAVE FM-1, a través de su fork
[ChoralRoot FM-1](https://github.com/Quixotic7/ChoralRootFM1). Las pantallas son propias, dibujadas para la
OLED de 128×64 del Move.

> Proyecto independiente. No está afiliado con Ableton, M-VAVE, Hügelton Instruments, Teenage Engineering ni el
> proyecto Schwung. No incluye firmware, sonidos, presets ni gráficos de ningún producto comercial.

## Opal Synth v0.1

Un `sound_generator` de Schwung con **11 motores de Felucca**: ANALOG, FM6, PHASE, LOFI, VOICE, TRIO, WHEEL,
GRAIN, PHYS, NOISE y DRUM, cada uno con sus presets de fábrica (los de Felucca, GPL).

| Página | Knobs |
| --- | --- |
| Motor (con el navegador de presets) | Los 8 valores EDIT del motor, con sus nombres (por ejemplo WAVE, DTN, MIX, NOIS, CUT, RES, DRV, KTR en ANALOG) |
| Envelope | Motor, Attack, Decay, Sustain, Release, Level |

- Las páginas cambian de nombres y rangos al cambiar de motor.
- Guarda y recupera el sonido completo (motor, preset, valores y envolvente) con los presets de módulo de Schwung.
- Pitch bend de ±2 semitonos.

**Límite de esta versión:** el motor de Felucca es uno solo por proceso, así que Opal Synth suena en **una pista a
la vez**. Una segunda instancia muestra un error y queda en silencio. Las pantallas propias (las de la maqueta)
llegan en la siguiente versión; de momento usa las páginas de parámetros de Schwung.

## Compilar, probar e instalar

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
