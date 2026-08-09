---
tags:
  - band-ritual
  - gui
  - moodboard
  - handoff
progetto: Band Ritual Final 1
data: '2026-06-03'
scopo: Moodboard + pacchetto di consegna dello chassis/GUI, con descrizione di ogni file
---
# Band Ritual Final 1 — GUI Moodboard & Handoff (2026-06-03)

> Moodboard e **punto raggiunto** sulla GUI di **Band Ritual Final 1** (vocoder VST).
> Pacchetto originale: `~/Desktop/BandRitual_Final1_Handoff_2026-06-03/`.
> Asset duplicati nel vault in `BandRitual_Final1_GUI_Brief_Assets/`.

## Punto raggiunto
- **Chassis** disegnato e renderizzato in **Blender** (progetto autosufficiente, texture *packed*).
- La **plate** dello chassis viene esportata in PNG e usata come **sfondo** della GUI del plugin.
- Tutta la UI interattiva (knob MIX, menu a tendina, grafici **BANDS** / **COMPLEMENT**, sezioni **NOISE + GATE**, riga di knob width/attack/release/gate/depth/formant/tone, tastiera) è **disegnata via codice C++ (JUCE)** sopra la plate. In Blender NON si costruisce la UI: solo la plate.
- Brand **RITUAL AUDIO LABS** definito (monogramma "RA").
- Canvas nativo dell'editor: **1580 × 1000 px** (scalato alla finestra). Accento coral **#D9554E**, font Montserrat, griglia 4px, knob stile Serum (valore solo durante il drag).
- Prossimo passo: rifinitura grafica (eventuale passaggio a un grafico) partendo da questo stato — **funzioni e layout restano**, si lavora sulla resa visiva. Vedi anche [[Band Ritual Final 1 - Brief GUI per Grafico]] e [[Band Ritual - Final 1 COMPLETE Checkpoint]].

## Cosa c'è in ogni file (pacchetto di consegna)

| File | Cos'è |
|---|---|
| `band_ritual_final1_chassis.blend` | Progetto Blender completo dello chassis. Texture/dati **packed** dentro al .blend → autosufficiente (Blender ≥ 3.0). Per ri-estrarre le texture: *File > External Data > Unpack All Into Files*. Origine: `band_ritual_v3_keyboard_restored.blend`. |
| `chassis_export_from_blender.png` | Render **piatto** dello chassis esportato da Blender = il PNG usato come **sfondo/plate** dentro la GUI (`chassis_plate.png`). |
| `band_ritual_final1_GUI_standalone.png` | **Screenshot dello standalone** così com'è ora: la plate + tutti gli elementi interattivi disegnati via codice. Riferimento di come la UI si appoggia sopra lo chassis. |
| `ritual_audio_labs_logo.png` | Logo **RITUAL AUDIO LABS** (monogramma "RA") in alta risoluzione su fondo scuro. Marchio per GUI/materiali. |
| `ritual_audio_labs_logo_light.png` | Versione chiara/piccola dello stesso logo (mark su fondo trasparente). |
| `mood board BR.pptx` | La **moodboard** vera e propria (PowerPoint, ~42 MB) — riferimenti visivi/estetici raccolti per la direzione artistica. |
| `Band ritual - Blender + Photoshop (& some Chat gpt ...) - ATTEMPT.pptx` | Tentativo/lavorazione dello chassis tra **Blender + Photoshop** (+ alcuni asset trasparenti generati). Storia del processo. |
| `README.txt` | Note di consegna originali (in italiano) — descrizione cartella + come aprire il Blender. |

## GUI attuale (standalone)
La plate Blender con sopra tutti i controlli disegnati in JUCE (MIX, dropdown chiave/scala/triade, grafici BANDS e COMPLEMENT, NOISE + GATE, riga knob, tastiera).
![[BR_final1_GUI_current.png]]

## Chassis esportato da Blender (la "plate")
Esattamente lo sfondo usato dentro la GUI.
![[BR_final1_chassis_export.png]]

## Logo — RITUAL AUDIO LABS
Monogramma "RA". A sinistra full su fondo scuro, a destra la versione light/trasparente.
![[BR_ritual_audio_labs_logo.png]]
![[BR_ritual_audio_labs_logo_light.png]]

## Sorgenti / allegati nel vault
- Progetto Blender: [[BR_final1_chassis.blend]]
- Moodboard PPTX: [[BR_moodboard.pptx]]
- Attempt Blender+Photoshop PPTX: [[BR_chassis_attempt_blender_photoshop.pptx]]
- README consegna: [[BR_handoff_README.txt]]
