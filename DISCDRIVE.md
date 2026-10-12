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
lesbarer Key ist nicht konfiguriert. Die Datei wird beim Lesen tolerant gegen Leerzeilen und
Kommentare sein.

Der Einstellungs-Punkt bleibt auch ohne Key sichtbar, damit der Status auffindbar ist. Beim
Anwählen ohne Key erklärt ein kurzer Dialog den Dateipfad und das benötigte Format, statt einen
Netzwerkaufruf zu starten.

Wenn ein Key vorhanden ist, öffnet **TheGamesDB** einen Dialog mit **Check API key**. Der Check
sendet einen HTTPS-GET an:

```text
https://api.thegamesdb.net/v1/API/Limit?apikey=<URL-encoded-key>
```

Die API-Dokumentation nennt diesen Endpoint „Check API key allowance“ und verlangt `apikey` als
Query-Parameter. Eine erfolgreiche Antwort enthält unter anderem `code`, `status`,
`remaining_monthly_allowance` und `extra_allowance`. `403` kann laut API sowohl einen ungültigen
Key als auch ein erreichtes Nutzungslimit bedeuten; die UI darf diesen Status daher nicht
ausschließlich als „invalid key“ darstellen. Netzwerkfehler, HTTP-Fehler und ungültige JSON-
Antworten erhalten verständliche Meldungen. Der Key wird nie ausgegeben.

Der bestehende `Downloader` kann HTTPS-Dateien abrufen, bietet aber keinen strukturierten
HTTP-Status/JSON-Response. Für den Check braucht es deshalb entweder eine eng begrenzte
API-Client-Erweiterung oder einen kleinen Request-Pfad, der HTTP-Status und Antwortdaten sicher
auswertet. API-Key-Werte müssen als URL-Komponente kodiert werden. UI-Thread und Render-Loop
dürfen nicht auf den Netzwerkzugriff warten; der Dialog zeigt einen laufenden Zustand und kann
geschlossen bzw. abgebrochen werden.

TheGamesDB dokumentiert außerdem Spielsuche nach Name (`/v1.1/Games/ByGameName`), optional mit
`filter[platform]`, und Cover-Abruf über `/v1/Games/Images`. Das kann Disc-Metadaten und Artwork
ergänzen, ersetzt aber nicht das lokale Lesen der Disc. Die API-Plattform-IDs für PS1 und Saturn
müssen einmal explizit geprüft und festgelegt werden. Treffer werden anhand des Disc-Titels,
der erkannten Plattform und — wenn vorhanden — Disc-ID/Serial plausibilisiert; bei mehreren
Treffern wird nicht stillschweigend ein beliebiges Spiel ausgewählt.

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

Für TheGamesDB wird zunächst per Titel und erkannter Plattform gesucht; eine PS1-Serial oder
Saturn-Kennung dient, sofern extrahierbar, zur zusätzlichen Plausibilisierung. Mehrdeutige
Treffer werden verworfen oder erfordern eine explizite Auswahl. Für den Treffer werden Boxart
sowie verfügbare Bilder vom Typ `screenshot` abgefragt. Screenshots sind die bevorzugte Quelle
für Ingame-Bilder; andere Bildtypen werden nur dann als Ersatz verwendet, wenn ihr Inhalt zur
Vorschau passt. Boxart und höchstens vier Ingame-Bilder werden heruntergeladen. Die GUI lädt
nicht bei jedem Tab-Wechsel erneut herunter.

Artwork, kleine Galerie-Vorschaubilder und der zugehörige strukturierte Metadatensatz werden in
einem persistenten Disc-Cache unter dem MiSTer-Pat-Datenverzeichnis gespeichert, nicht auf der
Disc. Cache-Schlüssel kombinieren Plattform mit stabiler Disc-Kennung; wenn nur ein Titel
verfügbar ist, wird dieser normalisiert und mit Plattform sowie Anbieter-ID gespeichert, damit
gleichnamige Titel nicht kollidieren. Temporäre Downloads werden erst nach erfolgreicher
Validierung atomar übernommen. Anbieter, Abrufzeit und gefundene Bildtypen werden mitgespeichert,
damit Fehlerzustände und erneute Versuche kontrolliert behandelt werden können. Cache-Größe,
Verdrängung und Aktualisierungsdauer sind vor Implementierung festzulegen.

TheGamesDB-Limits sollen nicht bei jedem Wechsel in die Disc-Ansicht vorsorglich abgefragt
werden. Der Key-Check bleibt eine explizite Aktion in Settings; normale Anfragen werten deren
HTTP- und API-Fehler aus und wechseln bei Fehlern direkt zum Libretro-Fallback. API-Key-URLs
dürfen weder geloggt noch in Fehlermeldungen angezeigt werden.

## Settings → System → Disc drive

Der Punkt ist sichtbar, aber **disabled**, wenn kein unterstütztes optisches Laufwerk erkannt
wird. Ist ein Laufwerk vorhanden, öffnet sich eine Status-/Verwaltungsansicht mit getrennten
Zeilen für:

- Laufwerk: erkannt/nicht verfügbar, Gerätepfad und Lesestatus.
- Physical-Disc-Main: installiert, fehlt oder Version unbekannt.
- PlayStation Disc Core/MGL und Saturn Disc Core/MGL: installiert, fehlt oder inkompatibel.
- **Install disc drive cores** bei fehlenden Dateien.
- **Update disc drive cores** bei installierten, aber älteren Dateien.

„Cores“ meint hier das zusammengehörige Softwarepaket aus Physical-Disc-Main-Unterstützung,
MGLs und den benötigten Core-Dateien. Ein einzelner vorhandener PSX-/Saturn-RBF genügt nicht als
Kompatibilitätsnachweis. Installation und Update brauchen eine festgelegte Quelle, Versions-
und Kompatibilitätsregeln, Integritätsprüfung, Download-Fortschritt sowie einen sicheren
Abbruch-/Rollback-Pfad. Dateien werden zunächst in temporäre Pfade geladen und erst nach
erfolgreicher Prüfung atomar installiert. Nutzerdateien und BIOS-Dateien werden weder geladen
noch überschrieben.

### Laufwerkserkennung

Die Erkennung läuft außerhalb des Render-Loops und wird zeitlich begrenzt wiederholt. Auf Linux
sollten mindestens `/dev/sr*`-Geräte und die zugehörigen `/sys/block/sr*`-Informationen geprüft
werden; konkrete Erkennung und Disc-Status müssen auf dem Dock verifiziert werden. `physcd_probe`
aus dem untersuchten Loader-Projekt ist ein nützliches manuelles Diagnosewerkzeug, aber die GUI
sollte nicht von einem optionalen Diagnoseprogramm als einziger Erkennungsmethode abhängen.

Laufwerkserkennung, eingelegte Disc und Lesbarkeit sind verschiedene Zustände. Ein nicht
beantwortender Gerätetreiber darf keine blockierende Schleife in Navigation oder Darstellung
auslösen. Laufwerksabfragen und Disc-Reads laufen in einem Worker mit Timeout; UI-Updates werden
als Zustandswechsel an den Hauptthread übergeben.

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

Der Optical-Drive-Backend ist die Voraussetzung, dass der Core Sektoren von einer Disc lesen
kann. Für GUI-Metadaten wird zusätzlich eine begrenzte lokale Abfrage benötigt. Der aktuelle POC
belegt noch keine konkrete, auf dem SuperStation Dock getestete Methode dafür. Zu untersuchen
sind:

1. Laufwerk und Disc-Status über Linux-Geräte-/CD-ROM-Schnittstellen erkennen.
2. TOC und verfügbare Disc-Kennungen lesen, ohne bei fehlender Disc zu blockieren.
3. Bei PlayStation 1 die Disc-ID aus lesbaren Disc-Strukturen, typischerweise über die
   `SYSTEM.CNF`-Boot-Zeile, gewinnen.
4. Bei Saturn Systemkennung und Titel aus den Disc-Header-/IP.BIN-Daten ermitteln.
5. Bei nicht lesbaren oder abweichenden Discs mit Status und unbekannten Feldern sauber
   weiterarbeiten.

Das konkrete Read-Verfahren, Sektorzugriff und Kompatibilität mit der SuperStation-Hardware
müssen praktisch validiert werden. TheGamesDB kann danach anhand des extrahierten Namens bzw.
der Kennung einen Titel, eine Plattform und Artwork ergänzen. Der lokale Parser bleibt die
Quelle für den tatsächlichen Gerät-/Disc-Status.

## Startpfad

MiSTer Pat startet normale ROMs über ein selbst erzeugtes MGL und `load_core` auf
`/dev/MiSTer_cmd`. Dieses MGL kann eine physische Disc nicht selbst einlegen oder mounten. Der
Disc-Tab muss daher das zur Disc passende Physical-Disc-MGL direkt starten. PlayStation nutzt im
normalen CD-Core den Slot `s`, Index `1`; Saturn den Slot `s`, Index `0`. Die Physical-Disc-MGLs
können einen anderen Startpfad bzw. Main-Build benötigen; deren bereitgestellte Definition ist
maßgeblich und darf nicht aus den normalen Image-MGLs abgeleitet werden.

Die Untersuchung in [`docs/POC.md`](docs/POC.md) nennt zwei voneinander unabhängige Ansätze:

- [Main_MiSTer_Physical_Disc](https://github.com/Anime0t4ku/Main_MiSTer_Physical_Disc)
- [MiSTer USB CDROM Loader](https://github.com/theshaneobrien/mister-disc-drive-support)

Beide Projekte haben eigene Main-Binaries, INI-Abschnitte und MGL-Layouts. Vor Implementierung
muss eines davon als Quelle gewählt werden; Dateien und Konfigurationsschema der Projekte dürfen
nicht vermischt werden. Die Main-Auswahl bleibt auf die Physical-Disc-Cores begrenzt. Insbesondere
darf die GUI nicht `[MiSTer] main=` global ändern, da dies MiSTer Main für alle Cores ersetzen
würde. Bestehende Core-spezifische `main=`-Abschnitte sind nur nach Prüfung der gewählten Quelle
zu verwenden.

Startfehler (Disc entfernt, Lesefehler, Core nicht mehr vorhanden, FIFO-Fehler) werden angezeigt;
ein erfolgreich gestarteter Core beendet wie bei anderen Spielen die GUI-Eingabeübernahme.

## Vorschlag für Aufbau und Performance

- `DiscDriveService`: Laufwerk-/Disc-Status und Hardwareabfrage im Worker; keine Abfrage pro Frame.
- `DiscMetadataReader`: begrenztes Lesen von TOC, PS1-Kennung und Saturn-Headerdaten.
- `TheGamesDBClient`: Key-Konfiguration, API-Key-Test, Suche und optionale Image-Metadaten.
- `ArtworkProvider`/`DiscArtworkService`: provider-neutrale Disc-Suche, lokaler Cache und
  Quellen-Priorität TheGamesDB → bestehende Libretro.com-Variante. Die Disc-Ansicht kennt keine
  TheGamesDB-Endpunkte direkt.
- `DiscCoreManager`: Erkennen, Installieren und Aktualisieren eines festgelegten, versionierten
  Physical-Disc-Pakets mit Integritätsprüfung.
- `DiscScreen`: reine Darstellung und Eingabe; konsumiert asynchrone Status-/Metadaten-Snapshots.
- `TopBar`/`TabIcons`: Disc-Tab ganz links, nur sichtbar, solange das Laufwerk verfügbar ist.

Polling, Netzwerkzugriffe, JSON-Auswertung, Disc-Reads und Core-Installationen laufen nicht im
Render-Loop. Ereignisse werden gedrosselt; Disc-Metadaten werden nur bei erkanntem Wechsel oder
beim Öffnen des Tabs erneut gelesen. Artwork- und Metadaten-Lookups laufen asynchron, Ergebnisse
werden zwischengespeichert und nur bei Disc-Wechsel oder expliziter Aktualisierung neu angefordert.
Bildsuche greift nicht auf den ROM-Scan zu. Das Einlesen der vorhandenen Spiele-Datenbank wird
für den Disc-Tab nicht neu gestartet.

Der `DiscArtworkService` soll einen gemeinsamen Provider-Vertrag verwenden. In 0.8.0 genügen
TheGamesDB für Disc-Metadaten und -Bilder sowie ein Adapter auf die vorhandene Libretro.com-
Suche als Fallback. Für eine spätere Version kann derselbe TheGamesDB-Client als Quelle eines
allgemeinen Bibliotheks-Scrapers erweitert werden, der ROMs und Metadaten der Spielebibliothek
verarbeitet und Libretro.com ergänzen oder ersetzen kann. Bulk-Scraping der Bibliothek gehört
nicht zum Disc-Drive-Umfang von 0.8.0.

## Abhängigkeiten und offene Entscheidungen

- Auf dem konkreten SuperStation Dock Laufwerkserkennung, Stromversorgung und Disc-Reads testen.
- Einen der zwei Physical-Disc-Ansätze auswählen und Version/Kompatibilität zu PSX- und
  Saturn-Cores verbindlich festlegen.
- Installationsquelle, Manifest, Prüfsummen und Update-/Rollback-Strategie festlegen.
- Verifizieren, ob die benötigten MGL-/Core-Dateien legal und technisch direkt durch die GUI
  installiert bzw. aktualisiert werden können; BIOS bleibt Nutzerinstallation.
- API-Plattform-IDs, Image-Nutzung, API-Antworten, Allowance-Verhalten und Fehlertexte mit einem
  echten TheGamesDB-Key prüfen. Der API-Key wird als Query-Parameter übertragen; URLs dürfen
  daher nicht geloggt werden.
- TheGamesDB-Bildtypen und Bildgrößen für Boxart und Screenshots sowie Metadatenfelder für PS1
  und Saturn verifizieren; Cache-Verfalls- und Größenlimit festlegen.
- Tastensteuerung und Fokus für **Start game**, Netzwerkdialog und Disc-Core-Verwaltung
  definieren.
- Verhalten festlegen, wenn die Disc während Abfrage oder Start entfernt wird.

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
- Auf echtem SuperStation Dock wurden Laufwerk, mindestens eine PS1-Disc und eine Saturn-Disc
  samt Start über die installierten MGLs überprüft.

## Referenzen

- [TheGamesDB API Swagger UI](https://api.thegamesdb.net/)
- [TheGamesDB API OpenAPI-Spezifikation](https://github.com/TheGamesDB/TheGamesDBv2/blob/master/API/spec.yaml)
- [Bootstrap Icons: Disc](https://icons.getbootstrap.com/icons/disc/)
- [`docs/POC.md` — Physical PlayStation and Saturn discs](docs/POC.md)
