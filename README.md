# foo_dsp_manualstart

DSP per foobar2000 2.26 che ospita Affix silence (1000 ms / 0 ms / vuoto)
solo per la prima traccia avviata manualmente. Sostituisce lo script js_panel
"Auto DSP changer" e i preset Pregap_Manual / Pregap_Auto.

## Uso

1. Catena DSP: `... -> Pregap DSP -> Silenzio avvio manuale (host Affix silence)`
2. Non servono piu' i preset Manual/Auto ne' lo script.
3. Manuale = primo play dopo l'apertura di foobar2000 o dopo uno Stop utente.
   Sequenziale, Next e doppio clic durante la riproduzione = nessun silenzio.

## Installazione della DLL

- foobar2000 a 64 bit: copia `foo_dsp_manualstart.dll` (artifact x64) in
  `<profilo>\user-components-x64\foo_dsp_manualstart\`
- foobar2000 a 32 bit: copia la DLL (artifact Win32) in
  `<profilo>\user-components\foo_dsp_manualstart\`

Il profilo si apre da File > Open profile folder (o e' la cartella
di foobar2000 in modalita' portable). Riavvia foobar2000.
