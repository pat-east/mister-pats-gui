# TheGamesDB-Integration

## Zweck und Ablauf

TheGamesDB ergänzt die Disc-Ansicht um Boxart, Ingame-Screenshots und Spieldaten. Die vorhandene Libretro-Suche bleibt der Fallback, wenn TheGamesDB keinen passenden Treffer oder kein verwendbares Bild liefert.

Bei einem regulären Treffer laufen pro erkannter Disc zwei TheGamesDB-API-Aufrufe. Nur wenn die breite Namenssuche keinen Treffer auf der Disc-Plattform enthält, kommt eine plattformgefilterte Nachsuche hinzu (in diesem Fall insgesamt höchstens drei):

1. `v1.1/Games/ByGameName` sucht plattformübergreifend nach dem Titel und liefert die zusätzlichen Spieldaten.
2. Bei einem Treffer auf der Disc-Plattform fragt `v1/Games/Images` die IDs aller exakten Titel-, Plattform- und Regionsvarianten gemeinsam ab. Nach einer erfolgreichen plattformgefilterten Nachsuche folgt derselbe gebündelte Bildabruf. Die Antwort enthält die Bildpfade für Boxart und Screenshots. Die Bilddateien selbst werden danach vom CDN geladen.

Der Regionsfilter wird nicht als Suchparameter übergeben. Stattdessen wertet die Anwendung die `region_id` der Spielvarianten aus und wählt nach tatsächlicher Bildverfügbarkeit. `Games/Images` liefert keine eigene Regionsangabe pro Bild; die Region ergibt sich aus der gewählten Spiel-ID.

## API-Aufrufe

Die verwendeten Plattform-IDs sind `10` für Sony PlayStation und `17` für Sega Saturn. Die Namenssuche fordert zusätzlich `players`, `publishers`, `genres`, `overview`, `rating`, `coop` und `alternates` an. Die Antwort enthält außerdem `region_id` und `country_id`, obwohl diese beiden Felder in der veröffentlichten Spezifikation nicht bei den optionalen `fields` aufgeführt sind.

Die Regionsstammdaten stammen vom API-Endpunkt `v1/Regions`:

| ID | TheGamesDB-Region |
| ---: | --- |
| 1 | NTSC |
| 2 | NTSC-U |
| 3 | NTSC-C |
| 4 | NTSC-J |
| 5 | NTSC-K |
| 6 | PAL |
| 7 | PAL-A |
| 8 | PAL-B |
| 9 | Other |

Eine Länder-ID allein ist kein guter Ersatz für die Regions-ID: Die Bildauswahl folgt dem Videosystem der Veröffentlichung. `country_id` wird zusammen mit der gewählten Spielvariante für Diagnose und spätere Detailansichten gespeichert.

## Auswahl und Fallback

Die Anwendung akzeptiert übereinstimmende normalisierte Haupt- oder Alternativtitel. Ein einzelnes vierstelliges Jahres-Suffix wird als Titelvariante toleriert, zum Beispiel `Sega Rally Championship 1995`; beliebige ähnliche Titel werden nicht als Treffer behandelt. Mindestens ein Treffer auf der Disc-Plattform muss vorhanden sein.

Für die Varianten der Disc-Plattform wird eine Regionsreihenfolge aus der Disc-Region gebildet. Boxart und Spieldaten kommen immer von der Disc-Plattform. Screenshots werden zuerst aus den regional passenden Varianten dieser Plattform gewählt; fehlen dort Screenshots, können exakte Haupt- oder Alternativtitel anderer Plattformen als Screenshot-Fallback dienen. Die Bildauswahl versucht bis zu zwei passend gereihte Varianten, lädt pro Variante höchstens zwölf Bilddateien und behält maximal vier erfolgreich heruntergeladene und dekodierte Screenshots. So kann ein fehlerhafter CDN-Pfad übersprungen und eine weitere regionale oder plattformübergreifende Variante versucht werden. Das nutzt dieselbe gebündelte Bildanfrage und erzeugt keinen zusätzlichen API-Aufruf. Sind keine passenden Bilder verfügbar, bleibt Libretro als Fallback aktiv.

| Disc-Region | Bevorzugte TheGamesDB-Regionen |
| --- | --- |
| Europe, Australia, Brazil | PAL (6/7/8) → NTSC-U (2) → NTSC (1) → NTSC-J (4) → NTSC-K (5) → NTSC-C (3) → Other (9) |
| USA, Canada, Mexico | NTSC-U (2) → NTSC (1) → NTSC-C (3) → PAL (6/7/8) → NTSC-J (4) → NTSC-K (5) → Other (9) |
| Japan, Taiwan, Hong Kong, China, Asia | NTSC-J (4) → NTSC-K (5) → NTSC (1) → NTSC-U (2) → PAL (6/7/8) → NTSC-C (3) → Other (9) |
| Korea | NTSC-K (5) → NTSC-J (4) → NTSC-U (2) → NTSC (1) → PAL (6/7/8) → NTSC-C (3) → Other (9) |

Die Reihenfolge enthält absichtliche Fallbacks. So bekommt eine europäische Disc zuerst PAL-Artwork und kann bei fehlendem Cover auf US-Artwork ausweichen. Taiwan fällt zunächst auf Japan zurück. Ein Datensatz ohne `region_id` wird gegenüber allen bekannten Regionsvarianten nachrangig behandelt.

Wenn die plattformübergreifende Namenssuche keinen exakten Treffer für die Disc-Plattform enthält, folgt eine gezielte Suche mit `filter[platform]` für PlayStation beziehungsweise Saturn. Diese zusätzliche Suche läuft nur für solche Fehltreffer; übliche erfolgreiche Suchen behalten zwei API-Aufrufe. Die ersten Suchtreffer anderer Plattformen bleiben erhalten, damit sie weiterhin als Screenshot-Fallback dienen können. Die TheGamesDB-API unterstützt den Plattformfilter für die Namenssuche.

## Lokal gespeicherte Daten

TheGamesDB-Daten und heruntergeladene Bilder liegen lokal unter `/media/fat/mister-pat/disc-cache/<disc-key>/`. Der Schlüssel enthält die erkannte Disc-Region, damit eine zuvor gespeicherte JP-Ausgabe nicht eine spätere PAL-Auswahl blockiert.

`thegamesdb-details.json` hält die gewählte Spiel-ID, Region, Land, Erscheinungsdatum, Spielerzahl, Beschreibung, Bewertung, Koop-Angabe, die separat gewählten Screenshot-Spiel-IDs samt Plattform und Region sowie das vollständige ausgewählte Spielobjekt aus der Suchantwort fest. `metadata.json` merkt sich unter anderem die ausgewählte TheGamesDB-ID, Bildtypen, Provider und den Zeitpunkt der letzten Prüfung. Boxart und bis zu vier Screenshots werden lokal zwischengespeichert. Ein frischer Treffer- oder Negativ-Cache verhindert unnötige Wiederholungen.

### Laufzeittest: Virtua Fighter 2 (Europa)

Am 12. Oktober 2026 zeigte der MiSTer-Log zunächst trotz erfolgreicher Titelsuche nur Libretro-Artwork. Die Bildmetadaten-Anfrage endete mit `HTTP=0`, bevor ein HTTP-Status vom Server vorlag. Ursache war die kommaseparierte ID-Liste im `games_id`-Parameter: rohe Kommas wurden vom URL-Sicherheitsfilter abgelehnt. Die gesamte Liste wird jetzt URL-kodiert; die Fehlermeldung protokolliert zusätzlich den Transportgrund.

Nach dem Deploy meldete der MiSTer für die europäische Disc `TheGamesDB selected game=112054` (PAL), wählte für Screenshots mangels PAL-Screenshots die US-Variante `5635` und lud erfolgreich das PAL-Boxart sowie vier Screenshots. Damit bleibt die regionale Coverauswahl erhalten, während fehlende Screenshots aus einer passenden Fallback-Region kommen. Der Libretro-Fallback bleibt für fehlende oder nicht ladbare TheGamesDB-Bilder verfügbar.

### Laufzeittest: Discworld (Saturn mit PC-Screenshot-Fallback)

Der Saturn-Datensatz `29884` hat bei TheGamesDB „Region Not Set“ und bietet ein Boxart, aber keine Screenshots. Die Disc selbst wurde als Europe erkannt. Die plattformübergreifende Titelsuche fand zusätzlich die PC-Variante `14112`; deren vier Screenshots wurden geladen, während Boxart und Metadaten von der Saturn-Variante stammen. Das Ergebnis wurde auf dem MiSTer im Log bestätigt. Die Suche und der gemeinsame Bildmetadatenabruf bleiben bei insgesamt zwei API-Aufrufen.

Der API-Key wird ausschließlich aus der lokalen Einstellungsdatei geladen und weder protokolliert noch in dieses Repository geschrieben.

## API-Test: drei Titel und drei Regionen

Am 12. Oktober 2026 wurden die Titel Virtua Fighter (Sega Saturn), Sega Rally Championship (Sega Saturn; die Eingabe „Sega Ralley“ wurde als Sega Rally interpretiert) und Metal Gear Solid (PlayStation) geprüft. Dafür gingen insgesamt **10 API-Aufrufe** an TheGamesDB: drei Namenssuchen, ein gemeinsamer Bildabruf, ein ID-Abgleich, zwei Schema-/Antwortprüfungen, je ein Abruf der Regions- und Länderstammdaten sowie ein einmalig fehlgeschlagener Bildabruf mit HTTP 403 wegen eines Tippfehlers im lokal eingegebenen Schlüssel. Der Schlüssel wurde dabei nicht ausgegeben. Damit blieb der Test klar unter dem genehmigten Maximum von 50 Anfragen.

`Boxart` benennt die Zahl der in der Bildantwort gefundenen Vorderseiten; Rückseiten sind separat angegeben. Die Screenshots sind die von der API gemeldeten Datensätze und wurden für diese Prüfung nicht heruntergeladen.

### Virtua Fighter — Sega Saturn

| Region | ID | Erscheinungsdatum laut TGDB | Boxart | Screenshots |
| --- | ---: | --- | --- | ---: |
| Europe / PAL (6) | 112055 | 1995-07-08 | 1 Vorderseite | 0 |
| US / NTSC-U (2) | 9269 | 1995-05-11 | Vorder- und Rückseite | 0 |
| Japan / NTSC-J (4) | 84989 | 1994-01-07 | 1 Vorderseite | 0 |

Die japanische Variante `84989` enthält ein auffälliges Veröffentlichungsdatum von 1994-01-07. Die Regions-ID und die TheGamesDB-Seite ordnen sie NTSC-J zu; das Datum sollte daher als fehlerhafte oder fragwürdige Quelldaten behandelt werden, nicht als Grund, die Region zu ändern. Der frühere Treffer `90094` betrifft **Virtua Fighter 2**, ist NTSC-J und hat eine Vorder- und Rückseite, aber keine Screenshots. Er darf nicht als Treffer für Virtua Fighter verwendet werden. Die US-Ausgabe von Virtua Fighter 2 ist ID `5635`; der Bildabruf meldet dort acht Screenshots sowie Vorder- und Rückseite. So erklärt sich, warum die Website Screenshots zeigt, obwohl JP-ID `90094` keine hat.

### Sega Rally Championship — Sega Saturn

| Region | ID | Erscheinungsdatum laut TGDB | Boxart | Screenshots |
| --- | ---: | --- | --- | ---: |
| Europe / PAL (6) | 112064 | 1996-01-24 | 1 Vorderseite | 0 |
| US / NTSC-U (2) | 11173 | 1995-11-18 | Vorder- und Rückseite | 9 |
| Japan / NTSC-J (4) | 124430 | 1995-12-29 | 1 Vorderseite | 0 |

Weitere Namensvarianten sind nicht automatisch die gesuchte Region: `119324` ist Korea / NTSC-K; `114137` ist Brasilien / NTSC. Die Japan-Variante heißt in der API `Sega Rally Championship 1995`; das Jahres-Suffix ist deshalb für den exakten Variantenabgleich erlaubt.

### Metal Gear Solid — PlayStation

| Region | ID | Erscheinungsdatum laut TGDB | Boxart | Screenshots |
| --- | ---: | --- | --- | ---: |
| Europe / PAL (6) | 120996 | 1999-02-26 | Vorder- und Rückseite | 0 |
| US / NTSC (1) | 196 | 1998-10-20 | Vorder- und Rückseite | 4 |
| Japan / NTSC-J (4) | 112397 | 1998-09-03 | Vorder- und Rückseite | 4 |

Damit gibt es für die europäische Ausgabe tatsächlich einen eigenen TheGamesDB-Treffer. Die bisherige Auswahl des Japan-Treffers `112397` entstand, weil mehrere exakte Titel ohne Regionswertung nach API-Reihenfolge behandelt wurden.

Die europäische MGS-Ausgabe `120996` hat laut Bildantwort kein Screenshot. Bei der unabhängigen Screenshot-Auswahl fällt die Integration daher auf die US-Ausgabe `196` zurück, die vier Screenshot-Datensätze anbietet; das PAL-Cover bleibt dabei erhalten.

## Referenzen

- [TheGamesDB API-Spezifikation](https://github.com/TheGamesDB/TheGamesDBv2/blob/master/API/spec.yaml)
- [Virtua Fighter (USA, ID 9269)](https://thegamesdb.net/game.php?id=9269)
- [Virtua Fighter (Japan, ID 84989)](https://thegamesdb.net/game.php?id=84989)
- [Virtua Fighter 2 (Europe, ID 112054)](https://thegamesdb.net/game.php?id=112054)
- [Sega Rally Championship (USA, ID 11173)](https://thegamesdb.net/game.php?id=11173)
- [Sega Rally Championship 1995 (Japan, ID 124430)](https://thegamesdb.net/game.php?id=124430)
- [Sega Rally Championship (Europe, ID 112064)](https://thegamesdb.net/game.php?id=112064)
- [Metal Gear Solid (Japan, ID 112397)](https://thegamesdb.net/game.php?id=112397)
- [Metal Gear Solid (USA, ID 196)](https://thegamesdb.net/game.php?id=196)

## Sega-Saturn-Stichprobe (40 Titel)

Am 12. Oktober 2026 wurden die folgenden **40 eindeutigen Titel** geprüft. „Sega Rally Championship“ stand doppelt in der Eingabe und wurde nur einmal abgefragt. Insgesamt gingen **81 API-Aufrufe** ein: 40 Namenssuchen, 40 gebündelte Bildmetadatenabfragen und ein Plattform-Stammdatenabruf. Das liegt unter dem freigegebenen Maximum von 200. Es wurden keine erneuten Versuche nach Fehlern ausgelöst.

Die Untersuchung erfasst, was die API-Suche und Bildmetadatenantwort für diese Namen zurückgaben. „Front“ und „Screenshots“ zählen von der API gemeldete Bilddatensätze; Bilddateien wurden in diesem Katalogtest nicht heruntergeladen oder dekodiert. Regionsnamen: R1 NTSC, R2 NTSC-U, R4 NTSC-J, R5 NTSC-K, R6 PAL, R9 Other; „ohne Region“ heißt, dass TheGamesDB am Spiel-Datensatz keine `region_id` lieferte. IDs sind TheGamesDB-Spiel-IDs.

### Saturn-Treffer pro Titel

| Spiel | Saturn-Varianten (ID, Region, Frontcover, Screenshots) |
| --- | --- |
| Alien Trilogy | `2421` (R2: 1 Front, 4 Screenshots); `103805` (R4: 1 Front, 0 Screenshots); `112053` (R6: 1 Front, 0 Screenshots) |
| Alone in the Dark: One-Eyed Jack's Revenge | `112370` (R6: 1 Front, 0 Screenshots); `9613` (ohne Region: 1 Front, 1 Screenshots) |
| Andretti Racing | `10492` (ohne Region: 1 Front, 0 Screenshots) |
| Batman Forever: The Arcade Game | `10565` (ohne Region: 1 Front, 0 Screenshots) |
| Command & Conquer | `10029` (ohne Region: 1 Front, 0 Screenshots) |
| Cyberia | `10584` (ohne Region: 1 Front, 0 Screenshots) |
| D | `10585` (ohne Region: 1 Front, 1 Screenshots) |
| Dark Savior | `11227` (R2: 1 Front, 0 Screenshots); `112040` (R6: 1 Front, 0 Screenshots); `112041` (R4: 1 Front, 0 Screenshots) |
| Deep Fear | `11230` (R6: 1 Front, 12 Screenshots); `91575` (R4: 1 Front, 9 Screenshots) |
| Defcon 5 | `11231` (ohne Region: 1 Front, 0 Screenshots) |
| Die Hard Trilogy | `15191` (ohne Region: 1 Front, 1 Screenshots) |
| Discworld | `29884` (ohne Region: 1 Front, 0 Screenshots) |
| Duke Nukem 3D | `125043` (R1: 1 Front, 1 Screenshots); `5688` (ohne Region: 1 Front, 0 Screenshots) |
| Fighting Vipers | `11172` (R2: 1 Front, 10 Screenshots); `79300` (R4: 1 Front, 0 Screenshots) |
| Gungriffon | `11272` (ohne Region: 1 Front, 0 Screenshots); `84274` (ohne Region: 1 Front, 0 Screenshots) |
| Hexen | Kein passender Saturn-Datensatz in den gelieferten Suchergebnissen |
| Magic Carpet | `11294` (ohne Region: 1 Front, 0 Screenshots) |
| Mystaria: The Realms of Lore | `36136` (ohne Region: 1 Front, 0 Screenshots); `7938` (ohne Region: 1 Front, 1 Screenshots) |
| NBA Jam Extreme | `2443` (ohne Region: 1 Front, 0 Screenshots) |
| Nights into Dreams | `113832` (R2: 1 Front, 0 Screenshots); `138965` (R6: 1 Front, 4 Screenshots); `48` (R2: 1 Front, 5 Screenshots) |
| Panzer Dragoon | `112311` (R6: 1 Front, 0 Screenshots); `9260` (R1: 1 Front, 8 Screenshots); `70855` (R9: 1 Front, 0 Screenshots) |
| Policenauts | `23765` (ohne Region: 1 Front, 0 Screenshots) |
| Pro Pinball: The Web | `11318` (ohne Region: 1 Front, 0 Screenshots) |
| Resurrection: Rise 2 | `10033` (ohne Region: 1 Front, 0 Screenshots) |
| Road Rash | `9205` (ohne Region: 1 Front, 0 Screenshots) |
| Sega Rally Championship | `11173` (R2: 1 Front, 9 Screenshots); `112064` (R6: 1 Front, 0 Screenshots); `114137` (R1: 1 Front, 0 Screenshots); `119324` (R5: 1 Front, 0 Screenshots); `124430` (R4: 1 Front, 0 Screenshots) |
| Sega Worldwide Soccer '97 | `10034` (ohne Region: 1 Front, 0 Screenshots) |
| Shining Wisdom | `11184` (R1: 1 Front, 8 Screenshots); `78469` (R4: 1 Front, 0 Screenshots) |
| SimCity 2000 | `11344` (ohne Region: 1 Front, 0 Screenshots) |
| Snatcher | `71838` (ohne Region: 1 Front, 0 Screenshots) |
| Sonic 3D: Flickies' Island | `24406` (R4: 1 Front, 7 Screenshots); `138961` (R6: 1 Front, 7 Screenshots); `11348` (R2: 1 Front, 7 Screenshots) |
| The Horde | `11277` (ohne Region: 1 Front, 0 Screenshots) |
| The Story of Thor 2 | `4106` (R2: 1 Front, 11 Screenshots); `79336` (R4: 1 Front, 0 Screenshots) |
| Tomb Raider | `2779` (R2: 1 Front, 0 Screenshots) |
| True Pinball | `11365` (ohne Region: 1 Front, 0 Screenshots) |
| Ultimate Mortal Kombat 3 | `11368` (ohne Region: 1 Front, 0 Screenshots) |
| Virtua Fighter 2 | `5635` (R1: 1 Front, 8 Screenshots); `90094` (R4: 1 Front, 0 Screenshots); `112054` (R6: 1 Front, 0 Screenshots); `114138` (R1: 1 Front, 0 Screenshots) |
| Virtua Fighter Kids | `2391` (R2: 1 Front, 0 Screenshots); `112056` (R6: 1 Front, 0 Screenshots) |
| Virtual On: Cyber Troopers | `11370` (ohne Region: 1 Front, 0 Screenshots) |
| Wing Arms | `2426` (ohne Region: 1 Front, 0 Screenshots) |

`The Story of Thor 2` ist ein konkreter Alternativtitel-Fall: TheGamesDB führt den US-Saturn-Datensatz `4106` als `The Legend of Oasis` und listet elf Screenshots; der japanische Datensatz `79336` heißt `Thor: Seireioukiden` und hat keine Screenshots. Der Titelabgleich berücksichtigt die von TheGamesDB gelieferten Alternativtitel. Die API-Suche verwendet den vollständigen Disc-Titel einschließlich führender Artikel wie „The“, weil dieser Wortlaut selbst der hinterlegte Alternativtitel sein kann; erst der Vergleich der Suchtreffer normalisiert Artikel und Präfixe. Die Screenshot-Auswahl kann dadurch die elf Bilder der US-Variante finden, wenn eine Disc-Variante keine Screenshots enthält. Ein eigener PAL-Datensatz war in der abgefragten Stichprobe nicht enthalten.

### Hinweise für Weiterentwicklung

- **Boxart-Abdeckung ist hoch:** 39 der 40 Suchanfragen lieferten mindestens einen passenden Saturn-Datensatz, und jeder dieser Datensätze hatte mindestens ein Frontcover. `Hexen` hatte in der gelieferten Titelsuche keinen Saturn-Treffer; das ist ein Such-/Katalogproblem, kein Beleg dafür, dass es keinen Saturn-Eintrag gibt. Ein gezielter Alias- bzw. Plattform-Suchweg sollte für solche Fälle ergänzt werden.
- **Saturn-Screenshots sind deutlich seltener:** 15 Titel hatten bei mindestens einer Saturn-Variante Screenshots; 24 Saturn-Titelvarianten-Gruppen hatten in den gelieferten Bilddaten keinen Screenshot. Die vorhandenen 15 verteilen sich auf: Alien Trilogy, D, Deep Fear, Die Hard Trilogy, Duke Nukem 3D, Fighting Vipers, Mystaria: The Realms of Lore, Nights into Dreams, Panzer Dragoon, Sega Rally Championship, Shining Wisdom, Sonic 3D: Flickies' Island, The Story of Thor 2, Virtua Fighter 2. `Hexen` hat keinen gefundenen Saturn-Datensatz.
- **Plattform-Fallback hilft in weiteren zehn Fällen**, wenn die Disc-Plattform keine Screenshots hat und ein exakter Titel auf einer anderen Plattform Bilder liefert: Andretti Racing (PS1), Batman Forever: The Arcade Game (PS1), Command & Conquer (N64), Cyberia (PC/3DO/PS1), Discworld (PC), Road Rash (PC/Genesis/3DO), SimCity 2000 (SNES), Snatcher (Sega CD/TurboGrafx CD/MSX), Tomb Raider (mehrere Plattformen) und Ultimate Mortal Kombat 3 (mehrere Plattformen). Discworlds PC-Fallback ist bereits im Laufzeittest bestätigt. Bei den übrigen Kandidaten muss die tatsächliche Bilddatei noch heruntergeladen und auf Brauchbarkeit geprüft werden.
- **Region ist bei vielen Datensätzen nicht gepflegt.** Mehrere Saturn-Ausgaben haben `region_id` leer, obwohl es eine regionsgebundene Disc-Ausgabe ist. Dann kann die Anwendung die Disc-Region nicht mit der TGDB-Spielregion abgleichen. Der Quellcode reiht solche Datensätze jetzt hinter Varianten mit bekannter Region ein. Die erkannte Disc-Region bleibt separat und wird nicht durch TGDBs fehlende Angabe überschrieben.
- **Alternative und gebräuchliche Titel kontrolliert ergänzen.** Serien- oder Regionaltitel wie „The Story of Thor 2“ können unter einem anderen Namen geführt werden. Aliasregeln sollten Plattform und Disc-Region berücksichtigen und exakte Treffer bevorzugen, damit ähnliche Titel nicht versehentlich zusammenfallen.
- **Suchlimits berücksichtigen.** Plattformfilter helfen, wenn die plattformübergreifende Suche keinen Disc-Plattformtreffer enthält. Falls gefilterte Antworten künftig ebenfalls an ein Ergebnislimit stoßen, kann Seitennavigation ergänzt werden.
- **Screenshot-Fallbacks unabhängig vom Cover behandeln.** Die Disc-Plattform bleibt maßgeblich für Boxart und Spieldaten; Screenshots können aus einer passenden exakten Titelvariante einer anderen Plattform kommen. Nur vollständig heruntergeladene und dekodierte Bilder sollten als Cache-Treffer gelten.
- **Fehlende Regionsdaten tolerieren.** TheGamesDB liefert bei manchen Datensätzen keine `region_id`. Bekannte Regionen sollten daher bevorzugt werden, während unbekannte Varianten als nachrangiger Fallback verfügbar bleiben. Die erkannte Disc-Region darf dabei nicht durch fehlende API-Angaben ersetzt werden.

Die Tabellenwerte sind eine API-Momentaufnahme vom 12. Oktober 2026. Sie zeigen Verfügbarkeit in Suchantworten, nicht die Qualität oder Rechtmäßigkeit der einzelnen Bilder.
