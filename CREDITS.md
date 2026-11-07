# Créditos y licencias

Opal es GPL-3.0-only (texto completo en [LICENSE](LICENSE)).

## Código de terceros que usa Opal

| Qué | Autoría | Licencia |
| --- | --- | --- |
| Felucca: motores de síntesis, voces, efectos y tablas | Leo Kuroshita (@kurogedelic), Hügelton Instruments | GPL-3.0-only |
| ChoralRoot FM-1, fork de Felucca: VA, QUAD, GRAIN y otras partes | ChoralRoot FM-1 contributors (Quixotic7) | GPL-3.0-only |
| Motor FM6 y CZ (vía Melodee) | Kerem Kilic (Ellic Studio); restatement de MSFA (Google, Apache-2.0) y Dexed (Pascal Gauthier, GPL-3.0-or-later) | GPL-3.0-only / Apache-2.0 |
| Motor PHYS: modelos de DaisySP (Electrosmith, Emilie Gillet) y cuerdas simpáticas de Rings (Emilie Gillet) | Vía Felucca, portados a punto fijo | MIT (`vendor/felucca/LICENSES/`) |
| API de módulos de Schwung (`src/host/`) | Charles Vestal | MIT |

El código de Felucca/ChoralRoot va en `vendor/felucca/` sin cambios. Las tablas (`gen/felucca_tables.h`) y los
patches FM6 de Felucca (`gen/felucca_fm6.h`, voces propias de Felucca, sin datos de ROM de ningún instrumento) están
generados con las herramientas de Felucca que van en `vendor/felucca/tools/`, con su commit de origen anotado en
`vendor/*/UPSTREAM_COMMIT` y las cabeceras SPDX y de copyright originales intactas.

## Lo que Opal no incluye

- Los presets de fábrica del Casio CZ-1 ni los programas del Sequential Prophet-5: sus autores no han dado licencia.
- Firmware, samples, presets o gráficos de Teenage Engineering u otros fabricantes.
- Muestras sin licencia libre. Las muestras propias del usuario se cargan en el Move y no van en este repo.

Las pantallas de Opal son dibujos propios, hechos a partir de cómo suena cada motor y efecto.
