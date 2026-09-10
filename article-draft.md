# Der vollautomatische Digifant-2-Prüfstand: Diagnose, Simulation und Live-Tuning

Wer sich mit dem VW Digifant-Steuergerät beschäftigt — zu finden unter anderem im Golf 1 Cabrio (Digifant 2) sowie im Golf/Corrado G60 und Polo G40 (Digifant 1, verwandt, aber mit realen Unterschieden — z. B. belegt Pin 5 beim 2H die Klopfsensor-Masse, bei G40/G60 dagegen ein CO-Potentiometer, und die Zündungs-Endstufe sitzt beim G60 bereits im Steuergerät selbst mit invertiertem Signal) — kennt das Problem: Die Geräte sind mittlerweile über 35 Jahre alt, Lötstellen werden brüchig, Elkos altern, und die Fehlerbilder sind oft diffus. Ich repariere diese Steuergeräte schon länger von Hand und habe dazu bereits einige Artikel hier im Blog veröffentlicht. Jetzt möchte ich den nächsten Schritt gehen: die Diagnose automatisieren — und gleich noch einen draufsetzen, mit Live-Tuning per EPROM-Emulator und einem KI-gestützten, iterativen Regelkreis.

## Der Ausgangspunkt: Hardware-in-the-Loop von Hand

Der erste Prototyp existiert schon: ein handgebauter Hardware-in-the-Loop-Adapter, der das Kurbelwellensignal simuliert und über LEDs das Zünd- sowie Einspritzverhalten visuell darstellt. Das funktioniert, ist aber auf das menschliche Auge angewiesen — und genau da setzt die Automatisierung an.

Ob dieser Aufbau 1:1 auch für G40/G60 (Digifant 1) funktioniert, ist offen — angesichts der abweichenden Pin-5-Belegung und des invertierten Zündsignals beim G60 wird zumindest eine angepasste Verkabelung nötig sein, vermutlich sogar ein eigener Adapterstecker statt einer reinen Software-Umschaltung.

Nebenbei habe ich mir im Januar 2026 auch ein gebrauchtes, kommerzielles **Digifant I/II-Prüfgerät von KEN** über eBay geholt — aber eher als historisches Kuriosum: Es ist defekt, rein manuell bedient (keine Automatisierung, keine Elektronik-Diagnose im modernen Sinn) und ich habe bisher keine Ahnung, wie man es überhaupt bedient. Als Referenz für den DIY-Prüfstand taugt es also nicht, aber vielleicht wird das Zerlegen und Verstehen irgendwann ein eigener kleiner Blogbeitrag.

## Die Idee: Ein vollautomatischer Prüfstand

Das Ziel ist ein System, das alle relevanten Sensoren des Digifant 2 simulieren kann:

- **Kurbelwellensignal** mit variabler, sauber einstellbarer Drehzahl
- **NTC-Simulation** für Ansauglufttemperatur und Kühlwassertemperatur
- **Lambdasonden-Signal** der Sprungsonde (typisch ca. 100–900 mV, träge im Vergleich zu Zündung/Einspritzung) — am eigenen HiL-Aufbau liegt der Eingang auf Pin 2: ein fixer 40k-Widerstand mit einem 10k-Poti gegen die 5V-Versorgung von Pin 17 bildet die 0–1V-Spreizung der Sprungsonde nach (provisorisch, noch nicht endgültig verifiziert)
- **Klopfsensor-Signal** (siehe unten)
- optional das **Luftmengenmesser-Signal** — am eigenen HiL-Aufbau via 1k-Poti zwischen Pin 17 und Pin 6 an Pin 21, gemessen ca. 1V im Leerlauf und ca. 1,7V bei 3000 U/min

Gleichzeitig soll das System die Reaktion des Steuergeräts erfassen: Zünd- und Einspritzzeitpunkte im Mikrosekundenbereich, relativ zum Kurbelwellensignal, sowie den **Gesamtstromverbrauch** des Steuergeräts. Ein erhöhter oder unruhiger Stromverbrauch ist ein guter Frühindikator für alternde Endstufen oder Elkos, oft bevor sich das überhaupt im Zünd- oder Einspritzverhalten zeigt — dafür bietet sich ein I2C-Stromsensor wie der INA226 an. Das ist kein theoretisches Risiko: Sowohl bei einem G40 als auch bei einem G60, die mir zur Reparatur vorlagen, war ein 100µF-Elektrolytkondensator in der Hallgeber-Spannungsversorgung mit stark erhöhtem Innenwiderstand (statt niederohmig ca. 1Ω) die Ursache dafür, dass der davorliegende Widerstand R22 heiß wurde — in einem Fall brannte dabei sogar die Induktivität DR1 durch. Genau so ein schleichend steigender ESR würde sich frühzeitig als unruhiger Stromverbrauch zeigen, bevor es zum Totalausfall kommt.

## Zwei Zeitwelten

Technisch zerfällt das Projekt in zwei sehr unterschiedliche Timing-Anforderungen:

- **Hart-Echtzeit** (Mikrosekundenbereich): Kurbelwellensignal, Zündzeitpunkt-Erfassung, Einspritzzeit-Erfassung — hier braucht es Hardware-Timer bzw. Input-Capture, um Jitter zu vermeiden.
- **Gemütlich-Echtzeit** (Millisekunden bis Sekunden): NTC-Werte und Lambdasonden-Spannung, gut per DAC oder PWM mit Tiefpassfilter simulierbar.

Diese Anforderung spricht gegen den ESP32 als Herzstück — dessen ADCs sind notorisch unlinear und die IO-Zahl wird bei so vielen gleichzeitigen Kanälen schnell knapp. Besser geeignet erscheinen ein STM32 mit sauberen Timern und DMA, oder ein Raspberry Pi Pico 2 mit seinen PIO-Einheiten, die präzise Timing-Ketten ganz ohne Software-Jitter erlauben. Für dieses Projekt fällt die Wahl auf den **Raspberry Pi Pico 2** (RP2350): Er bietet mit drei PIO-Blöcken und insgesamt 12 State Machines mehr Parallelität als der ursprüngliche Pico (RP2040, 8 State Machines), dazu 150 statt 133 MHz Takt und mit 520 KB fast doppelt so viel RAM – bei nur minimal höherem Preis. Das gibt genug Spielraum, um Kurbelwellensignal, EPROM-Bus-Emulation und Adress-Tracing parallel auf getrennten PIO-Einheiten laufen zu lassen, ohne um Ressourcen konkurrieren zu müssen.

## Welcher Chip steckt eigentlich im Digifant 2?

Der Hauptprozessor ist aus der eigenen Reparaturarbeit bereits bekannt: ein **TP8039AHL** (Intel-MCS-48-Familie, Toshiba-Fertigung) — siehe dazu den [Digifant-Reparatur-Artikel](de/mods/liste/208-digifant-motorsteuergeraet-reparatur), wo das defekte Original abgebildet ist. Das Programm wird aus einem externen EPROM geladen, ähnliche 8039-Typen eignen sich als CPU-Ersatz.

Recherche in einschlägigen Foren zeigt zusätzlich: Je nach Digifant-Variante kommen unterschiedliche EPROM-Typen für die Kennfelder zum Einsatz — beim (eng verwandten) Digifant 1 im G40/G60 etwa ein 27128 oder 27256, beim Digifant 2 auch ein 27C64. Zusätzlich gibt es einen separaten Zündprozessor mit eigenem, meist nicht ohne Weiteres änderbarem internem EPROM. Zu beachten: Der native Adressraum der MCS-48-Familie ist mit 4 KB recht klein, größere EPROMs wie ein 27256 wären also entweder über Bank-Switching angebunden oder nur teilweise genutzt — das sollte vor dem Bau des Emulators direkt am eigenen Chip verifiziert werden, statt sich allein auf Forenwissen zu verlassen. Für den Prüfstand heißt das: Der EPROM-Emulator sollte mindestens die Typen 27C64 bis 27C256 abdecken können, um sowohl Digifant 1 als auch Digifant 2 zu unterstützen.

## Der Clou: EPROM-Emulation mit Live-Tracing

Statt das Original-EPROM zu verwenden, soll ein EPROM-Emulator zum Einsatz kommen. Das erlaubt es, Kennfelder live zu verändern und sofort zu beobachten, wie sich Zündwinkel oder Einspritzzeit dadurch ändern — praktisch ein Reverse-Engineering des Codes in Echtzeit am Prüfstand.

Bestehende Open-Source-Lösungen wie der EPROM-EMU-NG von Kris Sekula (Arduino-Nano-basiert, unterstützt 27C64 bis 27C512) sind ein guter Ausgangspunkt fürs reine Emulieren. Für mein Vorhaben reicht das aber nicht ganz: Im Emulationsmodus übernimmt das Zielsystem den Adressbus direkt über Schieberegister, der Arduino Nano "sieht" die laufenden Zugriffe dabei gar nicht mit. Ein Live-Tracing, welches Kennfeld-Byte gerade gelesen wird, lässt sich damit nicht ohne Weiteres nachrüsten.

Auch bei den bereits existierenden Pico-basierten ROM-Emulator-Projekten wie **PicoROM** von wickerwaka oder dem schlanken **rp2040-eeprom-emulator** von xrip ist ein echtes Adress-Zugriffs-Tracing bislang nicht vorgesehen — beide sind primär auf schnelles Hochladen und zuverlässiges Emulieren optimiert, nicht auf das Mitloggen von Zugriffen. Das ist also eine Lücke, die es in der Community offenbar noch nicht gibt.

Die sauberere Lösung: ein **eigener EPROM-Emulator auf Basis eines Raspberry Pi Pico 2**, aufbauend auf einem dieser bestehenden Projekte als Basis. Eine PIO-Einheit bedient den Bus im Nanosekundenbereich, eine zweite PIO-Einheit loggt parallel jeden Adresszugriff und streamt ihn per USB an den PC.

**Pegel-Hinweis:** Das Digifant arbeitet mit 5V-Logik, der Pico 2 intern mit 3,3V. Die Adressleitungen (Eingänge in den Pico) sind meist unproblematisch, da die meisten Pico-2-Pins 5V-tolerant sind. Die Datenleitungen sind aber bidirektional, und als Ausgang treibt der Pico 2 nur 3,3V — das liegt an der Erkennungsschwelle vieler TTL-/HCMOS-Eingänge. Ein Pegelwandler wie ein 74HCT245-Bustreiber auf den Datenleitungen ist daher empfehlenswert. Auch wickerwaka hat bei einer späteren PicoROM-Revision aus genau diesem Grund Level-Shifter ergänzt.

## Analogsignale sauber simulieren: externe DACs statt PWM

Für die "gemütlichen" Analogsignale — NTC-Ersatzspannungen, Lambdasonde, Luftmengenmesser — lohnt sich ein externer DAC-Chip statt PWM-plus-Filter oder dem (beim Pico 2 gar nicht vorhandenen) internen DAC. Ein Mehrkanal-Baustein wie der **MCP4728** (4 Kanäle, I2C) deckt das sauber ab. Für die NTC-Simulation im Speziellen wäre sogar ein digitales Potentiometer wie der **MCP4131** eine Überlegung wert, weil das Digifant intern einen Spannungsteiler mit festem Widerstand nutzt — ein simulierter Widerstand käme dem Original näher als eine reine Ersatzspannung.

## Klopfsensor-Simulation

Das Digifant 2 verfügt über eine Klopfregelung, die ebenfalls am Prüfstand nachgebildet werden soll. Ein echter Klopfsensor ist piezoelektrisch und wandelt hochfrequente mechanische Vibrationen (etwa 5–20 kHz) in eine kleine Wechselspannung um. Zur Simulation braucht es keine Mechanik, sondern einen kleinen DDS-Chip wie den **AD9833**, per SPI ansteuerbar, der ein definiertes, mit dem Kurbelwellensignal synchronisiertes Burst-Signal in genau diesem Frequenz- und Amplitudenbereich erzeugt — so lassen sich gezielt simulierte Klopfereignisse an bestimmten Kurbelwellenpositionen auslösen.

## Der Leerlaufsteller: reale Last statt Simulation

Der Digifant regelt die Leerlaufdrehzahl über ein Leerlaufregelventil an, das per PWM angesteuert wird, aber stromgeregelt ist — eine reine Spannungssimulation würde hier zu kurz greifen. Am eigenen Testaufbau wurde das erst mit einem einfachen 10Ω-Lastwiderstand an Pin 22/23 simuliert, der dabei aber mindestens 75W Verlustleistung abkönnen muss (bei nominell 20W Dauerlast) — ein klares Zeichen, dass eine simple resistive Last der Sache nicht gerecht wird. Pragmatischer Ansatz: einen gebrauchten, echten Leerlaufsteller als reale Last anschließen. Der bringt automatisch die richtige Induktivität und das richtige thermische Verhalten mit, das man sonst nur mühsam nachbilden müsste. Der Spulenstrom wird dann schlicht mit einem weiteren INA226 mitgemessen, um zu prüfen, ob Tastverhältnis und resultierender Ventilstrom korrekt auf Last, Drehzahl und Temperatur reagieren.

## Stand-alone-Bedienung: Display und Taster

Damit der Prüfstand nicht zwingend an einen PC gekoppelt sein muss, ist ein kleines OLED-Display (z. B. SSD1306 über I2C) sinnvoll, das live Drehzahl, aktuelle Sensorwerte und die letzten erfassten Zünd-/Einspritzwerte anzeigt. Ein paar Taster erlauben es, die Drehzahl manuell zu verstellen oder zwischen vordefinierten Testprofilen zu wechseln, ohne jedes Mal ein Skript am PC starten zu müssen — praktisch für Vorführungen oder den schnellen Check auf der Werkbank.

Sollte der Prüfstand später auch Fehlercodes auslesen, kommt eine weitere Digifant-1/2-Inkompatibilität ins Spiel: Der G40 spricht dafür K-Line, der G60 dagegen klassische Blinkcodes über das Kraftstoffpumpenrelais — zwei komplett unterschiedliche Protokolle, die eigene Auslese-Logik bräuchten statt einer gemeinsamen Lösung.

## Vorbild aus der MegaSquirt-Welt: der Stim

In der MegaSquirt-Szene gibt es mit dem **Stim** (bzw. dem erweiterten **JimStim**) bereits ein sehr ähnliches Konzept: eine Simulatorbox, die CLT-, MAT-, TPS- und Lambdasonden-Signale sowie diverse Kurbelwellen-Trigger-Muster erzeugt und über LEDs die Zünd- und Einspritzausgänge anzeigt. Das ist im Grunde die Blaupause für genau dieses Vorhaben — nur eben ohne Automatisierung und ohne EPROM-Emulator. Einige Schaltungsideen, insbesondere zur Kurbelwellensignal-Erzeugung mit verschiedenen Radmustern, lassen sich davon direkt übernehmen.

Das eröffnet auch die Möglichkeit, etwas an die Open-Source-Community zurückzugeben: Das Adress-Tracing ließe sich als Pull-Request bei einem der bestehenden Pico-ROM-Projekte einbringen, und im JimStim-Umfeld, das bislang eher auf klassische Potentiometer setzt, wäre ein digitaler Ableger auf DAC-Basis mit automatisierter Ansteuerung ein spannender Beitrag.

## Der geschlossene Regelkreis mit Claude Code

Der letzte Baustein: Alle Baugruppen — Signal-Simulator, Erfassungseinheit und EPROM-Emulator — werden mit einem PC verbunden, auf dem Claude Code läuft. Damit entsteht ein autonomer Regelkreis: Kennfeld-Änderung vorschlagen, am Prüfstand testen, Reaktion messen und auswerten, nächste Änderung ableiten — iterativ und ohne manuellen Eingriff. Ein autonomer Tuning- und Analyse-Agent für ein 35 Jahre altes Steuergerät.

## Die Baugruppen im Überblick

1. **Signal-Simulator** — Kurbelwellensignal (variable Drehzahl), NTC-Simulation über DAC (Luft-/Kühlwassertemperatur), Lambdasonden-Signal, Klopfsensor-Signal (DDS), optional Luftmengenmesser
2. **Erfassungseinheit** — Zünd-/Einspritzzeitpunkt-Messung im µs-Bereich, Stromverbrauchsmessung (INA226) für Steuergerät und Leerlaufsteller
3. **Eigenbau-EPROM-Emulator (Raspberry Pi Pico 2)** — Live-Kennfeld-Emulation plus Adress-Tracing über eine zweite PIO-Einheit, aufbauend auf bestehenden Open-Source-Projekten
4. **Bedienoberfläche** — OLED-Display, Taster für Stand-alone-Betrieb
5. **Reale Last** — gebrauchter Leerlaufsteller statt Simulation

## Grober Aufwand

Nach Erfahrungswerten aus ähnlichen Embedded-Projekten, als grobe Richtschnur:

| Baugruppe | Geschätzter Aufwand |
|---|---|
| Signal-Simulator (Kurbelwelle, NTC, Lambda, LMM) | 2–3 Wochenenden |
| Erfassungseinheit (Zünd-/Einspritzzeit, Stromsensoren) | 2 Wochenenden |
| EPROM-Emulator mit Adress-Tracing | 3–4 Wochenenden |
| Claude-Code-Regelkreis | 1–2 Wochenenden |
| **Gesamt** | **ca. 8–11 Wochenenden** |

Da bereits Routine mit PIO-Programmierung, I2C-Sensoren und Claude Code aus früheren Projekten besteht, dürfte der reale Aufwand eher am unteren Ende dieser Spanne liegen.

## Projektplan mit Meilensteinen

**Phase 1 — Hardware-Grundlagen**
- Bauteilliste finalisieren (Pico 2(s), Pegelwandler, MCP4728, INA226, AD9833, OLED, Taster)
- Schaltplan für Signal-Simulator und Erfassungseinheit
- *Meilenstein: Schaltplan-Review abgeschlossen*

**Phase 2 — EPROM-Emulator**
- Bestehendes Pico-ROM-Projekt (PicoROM oder xrip's rp2040-eeprom-emulator) als Basis evaluieren und auf den Pico 2 / RP2350 portieren (dabei auf mögliche PIO-Verhaltensunterschiede zwischen RP2040 und RP2350 achten)
- Pegelwandler auf Datenleitungen integrieren, mit Original-EPROM-Inhalt gegentesten
- Zweite PIO-Einheit für Adress-Tracing entwickeln und per USB streamen
- *Meilenstein: Zuverlässige Emulation mit sichtbarem Live-Tracing am realen Digifant*

**Phase 3 — Signal-Simulator**
- Kurbelwellensignal per PIO mit variabler Drehzahl
- NTC-, Lambda- und Klopfsensor-Simulation über DAC/DDS
- *Meilenstein: Digifant läuft am Prüfstand im simulierten Leerlauf hoch*

**Phase 4 — Erfassungseinheit**
- Zünd-/Einspritzzeit-Erfassung synchron zur Kurbelwellenposition
- Stromsensoren für Steuergerät und Leerlaufsteller (mit realer Last)
- *Meilenstein: Vollständiger Datensatz aus einem Testlauf plausibel auswertbar*

**Phase 5 — Bedienoberfläche & Stand-alone-Betrieb**
- OLED-Display und Taster-Menü
- Vordefinierte Testprofile
- *Meilenstein: Prüfstand ohne PC bedienbar*

**Phase 6 — Claude-Code-Regelkreis**
- Skript zur automatisierten Kennfeld-Anpassung und Auswertung
- Iterationslogik (Änderung → Test → Messung → nächste Änderung)
- *Meilenstein: Erster vollautomatischer Tuning-Durchlauf ohne manuellen Eingriff*

**Phase 7 — Dokumentation & Open Source**
- Blog-Artikel-Serie, GitHub-Repo veröffentlichen
- Pull-Request für Adress-Tracing bei bestehendem Pico-ROM-Projekt
- Prüfung eines digitalen JimStim-Ablegers als eigenständiger Community-Beitrag
- *Meilenstein: Projekt öffentlich reproduzierbar*

## Ausblick: Von Digifant zu Motronic

Der logische nächste Schritt nach dem Digifant-Prüfstand ist die **Motronic**, wie sie im VR6-Motor zum Einsatz kam. Sie ist deutlich umfangreicher und komplexer als die Digifant — mehr Sensorik, mehr Kennfelder, aufwendigere Lambdaregelung und, wie beim Digifant 2, ebenfalls eine **Klopfregelung**. Wird der Digifant-Prüfstand von Anfang an modular genug aufgebaut, lässt er sich mit mehr Kanälen und angepasster Firmware grundsätzlich auch für die Motronic erweitern — ein spannender zweiter Teil dieser Prüfstand-Reihe.

## Bild-Ideen für diesen Artikel (Prompts für Gemini)

Um den Artikel visuell aufzulockern, hier ein paar Prompt-Vorschläge für KI-generierte Bilder:

1. **Übersichtsbild:** "Eine technische Illustration eines Werkstatt-Prüfstands für ein Volkswagen-Digifant-Steuergerät, mit einem Raspberry Pi Pico 2, Platinen, Kabeln und einem Oszilloskop im Hintergrund, im Stil einer sauberen Explosionszeichnung, blaue und orange Akzentfarben, hoher Detailgrad, fotorealistisch."

2. **EPROM-Emulator im Detail:** "Nahaufnahme einer kleinen grünen Leiterplatte mit einem Raspberry Pi Pico 2, die in einen alten DIP-28-Chip-Sockel gesteckt ist, Retro-Elektronik-Ästhetik trifft auf moderne Platine, Makro-Fotografie-Stil, weicher Hintergrund-Unschärfe."

3. **Regelkreis-Konzept:** "Eine konzeptionelle Grafik, die einen Kreislauf zeigt zwischen einem Computerbildschirm mit Code, einem Mikrocontroller-Board, und einem alten Volkswagen-Steuergerät, verbunden durch leuchtende Datenlinien, futuristisch aber technisch nachvollziehbar, dunkler Hintergrund."

4. **Historischer Bezug:** "Ein Golf 2 GTI oder G60 Motorraum aus den späten Achtzigern, mit Fokus auf dem Digifant-Steuergerät, warmes nostalgisches Licht, Dokumentarfotografie-Stil."

*Wird fortgesetzt.*
