# SuperStation Dock Disc Drive

## Ziel und Umfang für 0.8.0

Die GUI soll ein optisches Laufwerk im SuperStation Dock erkennen und physische Sega-Saturn-
und PlayStation-Discs (PS1) starten. Das Feature hat drei getrennte Voraussetzungen:

1. Linux erkennt ein optisches Laufwerk.
2. Eine Disc ist eingelegt und lesbar.
3. Der passende Physical-Disc-Main und das passende Disc-Core/MGL-Paket sind installiert und
   miteinander kompatibel.

Ein Laufwerk allein bedeutet nicht, dass physische Discs startbar sind. Der normale MiSTer Main
hat laut Voruntersuchung kein generisches USB-CD-ROM-Backend. Es braucht einen Main-Build mit
Optical-Drive-Unterstützung sowie dazu passende Physical-Disc-MGLs und FPGA-Cores. Die GUI darf
für dieses Feature die globale `[MiSTer] main=`-Einstellung nicht umstellen.

Der erste Umfang ist manuell: Disc einlegen, GUI erkennt sie, Nutzer wählt **Start game**.
Automatischer Start beim Einlegen, andere CD-Systeme, Disc-Auswurf und laufendes Wechseln von
Discs sind zunächst ausgeschlossen.

## Settings → Library → TheGamesDB

### Konfigurationsdatei

Die GUI legt beim Öffnen der Einstellung die Datei
`/media/fat/mister-pat/thegamesdb-settings.ini` an, falls sie fehlt. Der initiale Inhalt ist:

```ini
apikey=""
```

Die Konfiguration bleibt vorerst manuell. Die GUI zeigt den Status **Not configured** oder
**Configured** an, ohne den API-Key anzuzeigen oder in Logs auszugeben. Ein leerer oder nicht
lesbarer Key ist nicht konfiguriert. Die Datei wird tolerant gegen Leerzeilen und Kommentare
gelesen.

Der Einstellungs-Punkt bleibt auch ohne Key sichtbar, damit der Status auffindbar ist. Beim
Anwählen ohne Key zeigt eine Meldung den Dateipfad und das benötigte Format, statt einen
Netzwerkaufruf zu starten.

Wenn ein Key vorhanden ist, startet die Auswahl von **TheGamesDB** einen API-Key-Check im Worker.
Der Check sendet einen HTTPS-GET an:

```text
https://api.thegamesdb.net/v1/API/Limit?apikey=<URL-encoded-key>
```

Die API-Dokumentation nennt diesen Endpoint „Check API key allowance“ und verlangt `apikey` als
Query-Parameter. Eine erfolgreiche Antwort enthält unter anderem `code`, `status`,
`remaining_monthly_allowance` und `extra_allowance`. `403` kann laut API sowohl einen ungültigen
Key als auch ein erreichtes Nutzungslimit bedeuten; die UI darf diesen Status daher nicht
ausschließlich als „invalid key“ darstellen. Netzwerkfehler, HTTP-Fehler und ungültige JSON-
Antworten erhalten verständliche Meldungen. Der Key wird nie ausgegeben.

Der HTTP-Status und die API-Antwort werden ausgewertet; API-Key-Werte sind URL-kodiert. UI-Thread
und Render-Loop warten nicht auf den Netzwerkzugriff. Der Key selbst wird weder angezeigt noch
protokolliert.

TheGamesDB dokumentiert außerdem Spielsuche nach Name (`/v1.1/Games/ByGameName`), optional mit
`filter[platform]`, und Cover-Abruf über `/v1/Games/Images`. Das kann Disc-Metadaten und Artwork
ergänzen, ersetzt aber nicht das lokale Lesen der Disc. Die API-Plattform-IDs für PS1 und Saturn
müssen einmal explizit geprüft und festgelegt werden. Treffer werden anhand des Disc-Titels,
der erkannten Plattform plausibilisiert. Mehrdeutige Titeltreffer werden verworfen. Die
Plattform-IDs sind PS1 `10` und Saturn `17`.

### Artwork-Auflösung und Fallback

Beim Öffnen der Disc-Ansicht wird zuerst der lokale Disc-Snapshot ermittelt und anhand seiner
stabilen Kennung nach bereits gespeicherten Metadaten und Bildern gesucht. Ein vorhandener Cache
wird sofort angezeigt; Netzwerkzugriffe laufen parallel im Worker. TheGamesDB wird nur dann
abgefragt, wenn ein Key konfiguriert ist und für diese Disc kein brauchbarer Cache vorliegt.
Ungültiger Key, erreichtes Limit, Netzwerk-/API-Fehler, kein sicherer Spieletreffer oder fehlendes
Cover führen zum Fallback auf die bereits verwendete Libretro.com-Artwork-Suche. Diese nutzt die
vorhandene `LibretroIndex`-Zuordnung und `MediaScraper`-Download-/Bildverarbeitung. Wenn auch dort
kein Bild gefunden wird, bleibt das große Disc-Icon als Platzhalter stehen. Fehler bei TheGamesDB
dürfen die Disc-Ansicht und den Spielstart nicht blockieren.

Für TheGamesDB wird per erkanntem Titel und Plattform gesucht. Nur Kandidaten mit exakt
passendem normalisiertem Titel und Plattform werden akzeptiert; mehrere unterschiedliche IDs
werden als mehrdeutig verworfen. Die Disc-Kennung dient dem lokalen Titelindex und dem stabilen
Cache-Schlüssel, da TheGamesDB dafür keinen verlässlichen Disc-Serial-Abgleich bereitstellt. Für
einen eindeutigen Treffer werden Boxart sowie verfügbare Bilder vom Typ `screenshot` abgefragt.
Screenshots sind die bevorzugte Quelle
für Ingame-Bilder; andere Bildtypen werden nur dann als Ersatz verwendet, wenn ihr Inhalt zur
Vorschau passt. Boxart und höchstens vier Ingame-Bilder werden heruntergeladen. Die GUI lädt
nicht bei jedem Tab-Wechsel erneut herunter.

Eindeutige Nulltreffer (kein passendes Spiel oder keine passenden Bildtypen) werden im Metadaten-
Cache 30 Tage lang vermerkt. Netzwerk- und HTTP-Fehler gelten nicht als Nulltreffer und bleiben
erneut versuchbar.

Artwork, kleine Galerie-Vorschaubilder und der zugehörige strukturierte Metadatensatz werden in
einem persistenten Disc-Cache unter dem MiSTer-Pat-Datenverzeichnis gespeichert, nicht auf der
Disc. Cache-Schlüssel kombinieren Plattform mit stabiler Disc-Kennung; wenn nur ein Titel
verfügbar ist, wird dieser normalisiert und mit Plattform sowie Anbieter-ID gespeichert, damit
gleichnamige Titel nicht kollidieren. Temporäre Downloads werden erst nach erfolgreicher
Validierung atomar übernommen. Anbieter, Spiel-ID, Disc-Kennung und gefundene Bildtypen werden
mitgespeichert. Der Cache wird bei 128 MiB oder 80 Disc-Verzeichnissen anhand der ältesten
Verzeichnisse verkleinert; das aktuelle Spiel bleibt dabei erhalten.

TheGamesDB-Limits sollen nicht bei jedem Wechsel in die Disc-Ansicht vorsorglich abgefragt
werden. Der Key-Check bleibt eine explizite Aktion in Settings; normale Anfragen werten deren
HTTP- und API-Fehler aus und wechseln bei Fehlern direkt zum Libretro-Fallback. API-Key-URLs
dürfen weder geloggt noch in Fehlermeldungen angezeigt werden.

## Settings → System → Disc drive

Der Punkt ist sichtbar, aber **disabled**, wenn kein optisches Laufwerk erkannt wird. Ist ein
Laufwerk vorhanden, öffnet sich eine Statusansicht mit getrennten Zeilen für:

- Laufwerk: erkannt/nicht verfügbar, Gerätepfad und Lesestatus.
- Physical-Disc-Main: installiert, fehlt oder Version unbekannt.
- PlayStation Disc MGL und zugehöriger PlayStation-RBF-Core sowie Saturn Disc MGL und
  zugehöriger Saturn-RBF-Core: installiert, fehlt oder inkompatibel. MGL-RBF-Pfad und
  CD-Setname werden geprüft; die passenden RBFs werden nur erkannt, nicht durch den
  Disc-Paketinstaller verändert.
- `Install/update Disc cores`: lädt das gepinnte v0.7.0-Paket, prüft beide SHA-256-Prüfsummen,
  installiert nur `MiSTer-disc` und die PlayStation-/Saturn-MGLs, legt Backups an und schreibt
  ausschließlich `[CD-*] main=MiSTer-disc` in `MiSTer.ini`. Download und Prüfschritt lassen sich
  abbrechen; während der Installation wird ein begonnener Austausch sicher beendet.

„Disc-Paket“ meint hier Physical-Disc-Main-Unterstützung und MGLs; installierte FPGA-Cores und
BIOS-Dateien werden nicht verändert. Der Start wird nur freigegeben, wenn auch der passende
PlayStation- beziehungsweise Saturn-RBF-Core vorhanden ist. Nutzerdateien werden nur an den drei
ausdrücklich verwalteten Zielpfaden ersetzt; davor werden sie gesichert.

### Laufwerkserkennung

Die Implementierung sucht alle vier Sekunden nach `/dev/sr*`-Blockgeräten und liest das Modell aus
`/sys/block/sr*/device/model`. Laufwerkserkennung, eingelegte Disc und Lesbarkeit bleiben getrennte
Zustände. Die Hardwareabfrage läuft in einem separaten Loader-Unterprozess. Nach sieben Sekunden
ohne Ergebnis beendet der GUI-Prozess die Probe, verwirft den alten Titel und sperrt **Start game**.
Falls ein Treiber den Prozess in einem nicht unterbrechbaren Kernel-Wartezustand hält, wird kein
weiterer Probeprozess gestartet, bis der alte Prozess beendet ist.

## Disc-Tab und Darstellung

Wenn ein optisches Laufwerk und mindestens ein kompatibles Physical-Disc-Core-Paket erkannt
sind, wird ganz links in der Tab-Navigation **Disc** mit dem Bootstrap-Icon `disc` angezeigt.
Das Icon ist lokal als Asset zu bundlen; zur Laufzeit wird kein Web-Font benötigt. Bei fehlendem
Laufwerk oder fehlenden kompatiblen Disc-Cores bleibt der Tab verborgen. Die Verwaltung und
Installation der Cores bleibt über Settings → System erreichbar, sobald ein Laufwerk erkannt
wurde. Der Tab kann sichtbar sein, auch wenn keine Disc eingelegt ist.

### Zustands-Mockups

Alle Zustände verwenden dieselbe Zweispaltenansicht. Die rechte Spalte reserviert unter dem
Start-Button die Breite für vier Vorschau-Bilder; der Button ist exakt so breit wie diese Reihe.
Die Unterkante der Vorschau-Reihe fluchtet mit der Unterkante des Boxarts. Netzwerk- und
Metadatenzustände dürfen weder Render-Loop noch Navigation blockieren.

#### Keine Disc eingelegt

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│  Disc   Arcade   Console   Handheld                              Settings    │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                                                                              │
│                            Please insert disc                                │
│                                                                              │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

#### Disc ungültig oder nicht lesbar

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│  Disc   Arcade   Console   Handheld                              Settings    │
├──────────────────────────────────────────────────────────────────────────────┤
│  ┌────────────────────────────────┐   Disc could not be read                │
│  │                                │                                          │
│  │       [großes Disc-Icon]       │   Check that the disc is clean and      │
│  │                                │   inserted correctly.                   │
│  │                                │                                          │
│  └────────────────────────────────┘   ┌──────────────────────────────────┐   │
│                                       │           Start game             │   │
│                                       └──────────────────────────────────┘   │
│                                         (deaktiviert)                        │
└──────────────────────────────────────────────────────────────────────────────┘
```

#### Kein unterstütztes Spiel erkannt

Zum Beispiel bei einer Film-DVD oder einer unbekannten Daten-Disc. Die GUI zeigt keinen
erfundenen Titel oder Systemnamen; **Start game** bleibt deaktiviert, solange kein passender
Physical-Disc-Core zugeordnet werden kann.

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│  Disc   Arcade   Console   Handheld                              Settings    │
├──────────────────────────────────────────────────────────────────────────────┤
│  ┌────────────────────────────────┐   No supported game detected            │
│  │                                │                                          │
│  │       [großes Disc-Icon]       │   This disc may be a movie or use an    │
│  │                                │   unsupported format.                   │
│  │                                │                                          │
│  └────────────────────────────────┘   ┌──────────────────────────────────┐   │
│                                       │           Start game             │   │
│                                       └──────────────────────────────────┘   │
│                                         (deaktiviert)                        │
└──────────────────────────────────────────────────────────────────────────────┘
```

#### Spiel erkannt, Boxart vorhanden

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│  Disc   Arcade   Console   Handheld                              Settings    │
├──────────────────────────────────────────────────────────────────────────────┤
│  ┌────────────────────────────────┐   19XX: The War Against Destiny         │
│  │                                │                                          │
│  │                                │   System       Sony PlayStation          │
│  │         ┌────────────┐         │   Region       USA                       │
│  │         │            │         │   Disc ID      SLUS-xxxxx                │
│  │         │  BOXART    │         │   Publisher    Capcom                    │
│  │         │            │         │   Year         1996                      │
│  │         └────────────┘         │                                          │
│  │                                │   ┌──────────────────────────────────┐   │
│  │                                │   │            Start game            │   │
│  │                                │   └──────────────────────────────────┘   │
│  │                                │   ┌───────┐ ┌───────┐ ┌───────┐ ┌─────┐ │
│  │                                │   │ Shot 1│ │ Shot 2│ │ Shot 3│ │Shot4│ │
│  └────────────────────────────────┘   └───────┘ └───────┘ └───────┘ └─────┘ │
└──────────────────────────────────────────────────────────────────────────────┘
```

#### Kein Boxart; Informationen werden geladen

Der erkannte und startfähige Datenträger bleibt sofort startbar. Das Icon bleibt links als
Platzhalter; Metadaten und Bilder dürfen nachladen, ohne **Start game** zu sperren.

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│  Disc   Arcade   Console   Handheld                              Settings    │
├──────────────────────────────────────────────────────────────────────────────┤
│  ┌────────────────────────────────┐   PlayStation disc                     │
│  │                                │                                          │
│  │       [großes Disc-Icon]       │   Loading game information…             │
│  │                                │                                          │
│  │                                │   ┌──────────────────────────────────┐   │
│  └────────────────────────────────┘   │            Start game            │   │
│                                       └──────────────────────────────────┘   │
│                                         Loading artwork…                     │
└──────────────────────────────────────────────────────────────────────────────┘
```

Sobald die Suche endet, werden verfügbare Metadaten und Bilder ergänzt. Bei fehlendem Key,
API-Fehler oder fehlendem TheGamesDB-Cover wird die bestehende Libretro.com-Artwork-Suche
versucht. Fehlschlagen beide Quellen, bleibt das Platzhalter-Icon stehen; der Spielstart bleibt
verfügbar, sofern Disc, System und kompatibler Core dafür bereit sind.

#### Ingame-Bild vergrößert

Ein Vorschaubild lässt sich anwählen und in einem großen Overlay ansehen. D-pad links/rechts
wechselt zwischen den vorhandenen Bildern; **Abbrechen** schließt das Overlay und kehrt zur
Disc-Ansicht zurück.

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│                         ◀    [ großes Bild ]    ▶                           │
│                                                                              │
│                                  Bild 2 von 4                               │
│                                                                              │
│                         Abbrechen: schließen                               │
└──────────────────────────────────────────────────────────────────────────────┘
```

**Start game** ist nur deaktiviert, wenn Disc-Lesbarkeit, System/Core-Zuordnung oder kompatible
Physical-Disc-Dateien den Start verhindern. Boxart, Metadaten, Internetverbindung und laufende
Artwork-Suche sind keine Startvoraussetzungen.

## Disc-Informationen ermitteln

`DiscDriveService` sucht alle `/dev/sr*`-Blockgeräte und liest den Laufwerksstatus außerhalb des
Render-Loops. Saturn wird anhand des IP.BIN-Headers an LBA 0 erkannt. Für PS1 wird der
ISO-9660-Deskriptor an LBA 16 geprüft, anschließend `SYSTEM.CNF` aus dem Root-Verzeichnis gelesen
und die `BOOT`-Kennung normalisiert. Der mitgelieferte Redump/TOSEC-Index löst Kennungen offline
zu Titeln auf. Mehrdeutige Kennungen werden nicht willkürlich einem Titel zugeordnet; beim Saturn
bleibt dann der im IP.BIN gelesene Titel erhalten.

Auf dem Dock wurden `/dev/sr0` (`DVDRAM GUD1N`), Saturn `T-11302H` / `DISC WORLD` und PS1
`SLES-01507` aus `SYSTEM.CNF` gelesen. Das bestätigt die dort verwendeten Sektorzugriffe. Das
Timeout- und Abbruchverhalten des isolierten Prozesses muss noch mit einem nicht reagierenden
Laufwerk validiert werden.

## Startpfad

MiSTer Pat startet normale ROMs über ein selbst erzeugtes MGL und `load_core` auf
`/dev/MiSTer_cmd`. Dieses MGL kann eine physische Disc nicht selbst einlegen oder mounten. Der
Disc-Tab muss daher das zur Disc passende Physical-Disc-MGL direkt starten. PlayStation nutzt im
normalen CD-Core den Slot `s`, Index `1`; Saturn den Slot `s`, Index `0`. Die Physical-Disc-MGLs
können einen anderen Startpfad bzw. Main-Build benötigen; deren bereitgestellte Definition ist
maßgeblich und darf nicht aus den normalen Image-MGLs abgeleitet werden.

Die Implementierung verwendet ausschließlich den [MiSTer USB CDROM Loader](https://github.com/theshaneobrien/mister-disc-drive-support):
sein gepinntes `MiSTer-disc`-Binary, `_Disc_Cores/PlayStation.mgl`,
`_Disc_Cores/Saturn.mgl` und `[CD-*] main=MiSTer-disc`. Die GUI startet das zum erkannten System
gehörende MGL über `load_core`. `[MiSTer] main=` wird nicht geändert. Das alternative
Main-Binary gilt nur für die durch `[CD-*]` gerouteten Sessions.

Startfehler (Disc entfernt, Lesefehler, Core nicht mehr vorhanden, FIFO-Fehler) werden angezeigt;
ein erfolgreich gestarteter Core beendet wie bei anderen Spielen die GUI-Eingabeübernahme.

## Implementierte Komponenten

- `DiscDriveService`: Laufwerk-/Disc-Status und Hardwareabfrage alle vier Sekunden im Worker;
  keine Abfrage pro Frame.
- Lokaler Disc-Index: eingebetteter PS1-/Saturn-Titelbestand mit optionaler externer TSV-Datei.
- `TheGamesDBClient`: Key-Konfiguration, API-Key-Test, Suche und optionale Image-Metadaten.
- `ArtworkProvider`/`DiscArtworkService`: provider-neutrale Disc-Suche, lokaler Cache und
  Quellen-Priorität TheGamesDB → bestehende Libretro.com-Variante. Die Disc-Ansicht kennt keine
  TheGamesDB-Endpunkte direkt.
- `DiscCoreManager`: Erkennen, Installieren und Aktualisieren eines festgelegten, versionierten
  Physical-Disc-Pakets mit Integritätsprüfung.
- `DiscScreen`: reine Darstellung und Eingabe; konsumiert asynchrone Status-/Metadaten-Snapshots.
- `TopBar`/`TabIcons`: Disc-Tab ganz links, nur sichtbar, solange das Laufwerk verfügbar ist.

Polling, Netzwerkzugriffe, JSON-Auswertung, Disc-Reads und Core-Installationen laufen nicht im
Render-Loop. Disc-Metadaten werden nur bei erkanntem Wechsel erneut gelesen. Artwork- und
Metadaten-Lookups laufen asynchron, Ergebnisse werden zwischengespeichert und nur bei Disc-Wechsel
oder expliziter Aktualisierung neu angefordert.
Bildsuche greift nicht auf den ROM-Scan zu. Das Einlesen der vorhandenen Spiele-Datenbank wird
für den Disc-Tab nicht neu gestartet.

Der `DiscArtworkService` verwendet einen gemeinsamen Provider-Vertrag. TheGamesDB liefert optionale
Disc-Metadaten und -Bilder; die vorhandene Libretro.com-Suche dient als Fallback. Der Client kann
später als Quelle für einen allgemeinen Bibliotheks-Scraper erweitert werden. Bulk-Scraping der
ROM-Bibliothek gehört nicht zum Disc-Drive-Umfang von 0.8.0.

## Noch offene Abnahme

- PS1- und Saturn-MGL-Start sowie Disc-Wechsel auf echtem Dock abnehmen; bisher sind dort nur
  Laufwerk und lesbare Disc-Metadaten bestätigt.
- Einen nicht reagierenden Laufwerkstreiber prüfen und sicherstellen, dass das Zeitlimit keine
  Probeprozesse anhäuft und die UI weiterhin keinen alten Start anbietet.
- TheGamesDB mit einem echten Key und tatsächlichen Bildantworten prüfen; URLs mit Key dürfen
  dabei nicht protokolliert werden.

## Abnahmekriterien

- Eine fehlende Key-Datei wird mit exakt `apikey=""` angelegt; fehlender/leer gesetzter Key
  wird als **Not configured** angezeigt.
- Mit eingetragenem Key öffnet TheGamesDB einen Dialog, der Erfolg, ungültigen Key bzw. Limit,
  Netzwerkfehler und ungültige Antwort verständlich unterscheidet, soweit die API-Antwort das
  erlaubt; der Key wird nie angezeigt oder geloggt.
- Ohne optisches Laufwerk ist **Disc drive** in Settings deaktiviert und der Disc-Tab verborgen.
- Bei erkanntem Laufwerk ist der Tab links mit dem Disc-Icon verfügbar; ohne Disc steht zentral
  **Please insert disc**.
- Bei eingelegter Disc steht zunächst das große Disc-Icon links; erkannte Metadaten erscheinen
  rechts und **Start game** folgt darunter.
- Mit konfiguriertem TheGamesDB-Key erscheinen gecachte bzw. erfolgreich geladene Boxart und
  Metadaten; bis zu vier vorhandene Ingame-Bilder stehen unter **Start game** horizontal und
  ihre Unterkante fluchtet mit der Unterkante des Boxarts.
- Ohne Key oder bei TheGamesDB-Fehlern wird die bestehende Libretro.com-Artwork-Suche versucht;
  deren Ergebnis wird ebenfalls lokal gespeichert. Wenn beide Quellen kein Bild liefern, bleibt
  das Disc-Icon sichtbar.
- PS1 und Saturn werden nur bei ausreichenden lokalen Merkmalen erkannt; unbekannte Discs
  erhalten keine erfundene System-/Titelzuordnung.
- Start game verwendet das passende Physical-Disc-MGL. Die globale `[MiSTer] main=`-Einstellung
  bleibt unverändert.
- Kein Disc-, API- oder Installationszugriff blockiert Render-Loop oder Navigation.
- Auf echtem SuperStation Dock wurden Laufwerk und Disc-Metadaten für PS1 und Saturn gelesen.
  Der Start über die installierten MGLs sowie Disc-Wechsel sind noch nicht bestätigt.

## Referenzen

- [TheGamesDB API Swagger UI](https://api.thegamesdb.net/)
- [TheGamesDB API OpenAPI-Spezifikation](https://github.com/TheGamesDB/TheGamesDBv2/blob/master/API/spec.yaml)
- [Bootstrap Icons: Disc](https://icons.getbootstrap.com/icons/disc/)
- [`docs/POC.md` — Physical PlayStation and Saturn discs](docs/POC.md)
