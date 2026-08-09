---
tags:
  - band-ritual
  - gui
  - brief
  - grafica
progetto: Band Ritual Final 1
data: '2026-06-03'
scopo: Reference da inviare a un grafico per ridisegnare la GUI
---
# Band Ritual Final 1 — Brief GUI per Grafico

> Pacchetto di riferimento da inviare a un grafico. Mostra **quello che ho ora** per il plugin **Band Ritual Final 1** (un vocoder VST). Obiettivo: far ridisegnare/rifinire la **GUI** a un grafico professionista, partendo da questo stato attuale.

## Cosa voglio
Vorrei che un grafico ridisegnasse la GUI del plugin partendo dal layout e dalle funzioni attuali. **Le funzioni e la disposizione dei controlli devono restare** — quello che cerco è la resa visiva (chassis, knob, font, colori, finiture). Non rimuovere controlli.

## Specifiche tecniche di base
- **Canvas nativo dell'editor: 1580 x 1000 px** (scalato alla finestra).
- Plugin = vocoder. Tutta la UI (knob, pulsanti, testi, grafici) è disegnata via codice; in Blender è renderizzata **solo la plate dello chassis** (lo sfondo).
- Colore accento attuale: coral **#D9554E**. Font: Montserrat (header in Bold).
- Griglia 4px. Knob stile Serum (valore mostrato solo durante il drag).

## GUI attuale (stato live nello standalone)
La plate dello chassis di Blender con sopra tutti gli elementi interattivi (knob, menu a tendina, grafici, tastiera).

![[BR_final1_GUI_current.png]]

## Chassis esportato da Blender (la "plate" di sfondo)
Questo PNG è esattamente lo sfondo usato dentro la GUI.

![[BR_final1_chassis_export.png]]

## Progetto Blender dello chassis (sorgente)
File `.blend` completo e autosufficiente (texture incluse). Apribile in Blender 3.0+.

![[BR_final1_chassis.blend]]

## Sezioni / controlli presenti nella GUI (da mantenere)
- **BAND RITUAL** (in alto a sx): menu modalità vocoder (Self / External / Noise / Saw / Square / Pitch Track), knob grande **MIX**, due slider, dropdown **Key / Scale / Chord-degree / Variation** + toggle M, **mini-tastiera** 1 ottava.
- **BANDS** (centro/dx): display spettrale con tabs **DIST / OFFSET / BANDS**, numero bande grande, fila di knob: **width, attack, release, gate, depth, formant, tone**.
- **COMPLEMENT** (basso/dx): grafico EQ complementare con punti trascinabili.
- **NOISE** (basso/sx): dropdown Auto, knob **amount / color / stereo**, punto **HP** trascinabile, **CARVE**, **DELAY**.
- **GATE**: display inviluppo con linea soglia trascinabile.

## File originali del pacchetto
Cartella consegna: `~/Desktop/BandRitual_Final1_Handoff_2026-06-03/`
- `band_ritual_final1_chassis.blend` (progetto Blender, texture incluse)
- `chassis_export_from_blender.png` (render chassis)
- `band_ritual_final1_GUI_standalone.png` (screenshot GUI live)
- `README.txt` (note in italiano)

Codice/asset sorgente: `~/BandRitualV3_work/` (plate usata: `Source/Assets/chassis_plate.png`).

## Logo Ritual Audio Labs (marchio da usare sulla GUI)
Monogramma "RA". Versione alta risoluzione (su fondo scuro) + versione chiara/piccola.

![[BR_ritual_audio_labs_logo.png]]
![[BR_ritual_audio_labs_logo_light.png]]
