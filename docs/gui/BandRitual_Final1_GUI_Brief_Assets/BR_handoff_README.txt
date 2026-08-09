BAND RITUAL FINAL 1 — CONSEGNA CHASSIS GUI
Preparato il 2026-06-03

COSA C'E' IN QUESTA CARTELLA
---------------------------
1. band_ritual_final1_chassis.blend
   Il progetto Blender completo dello chassis della GUI. Tutte le texture /
   i dati esterni sono stati INCLUSI (packed) dentro al file .blend, quindi
   e' autosufficiente: basta aprirlo in Blender (versione 3.0 o successiva).
   Non serve copiare nient'altro.
   (Origine: band_ritual_v3_keyboard_restored.blend, l'ultima versione dello
   chassis.)

2. chassis_export_from_blender.png
   Il render piatto dello chassis esportato da Blender. E' esattamente il PNG
   usato come sfondo (la "plate") dentro alla GUI del plugin (chassis_plate.png).

3. band_ritual_final1_GUI_standalone.png
   Screenshot dello standalone "Band Ritual Final 1" cosi' come appare ora:
   la plate dello chassis di Blender con sopra tutti gli elementi interattivi
   disegnati via codice (knob MIX, menu a tendina, grafici BANDS / COMPLEMENT,
   sezioni NOISE + GATE, tastiera, fila di knob, ecc.). E' il riferimento di
   come la UI finale si appoggia sopra lo chassis.

4. ritual_audio_labs_logo.png
   Logo di RITUAL AUDIO LABS (monogramma "RA") in alta risoluzione, su fondo
   scuro. Da usare come marchio sulla GUI / materiali.

5. ritual_audio_labs_logo_light.png
   Versione chiara/piccola dello stesso logo (mark su fondo trasparente).

COME APRIRE IL PROGETTO BLENDER
-------------------------------
- Apri band_ritual_final1_chassis.blend in Blender.
- Le texture sono incorporate; se in futuro le vuoi di nuovo come file separati
  usa File > External Data > Unpack All Into Files.

NOTE
----
- La GUI del plugin NON e' costruita in Blender. In Blender viene renderizzata
  solo la plate dello chassis; tutti i knob/pulsanti/testi sono disegnati nel
  codice C++ (JUCE) del plugin.
- Il canvas dell'editor e' nativamente 1580 x 1000 px, scalato alla finestra.
