# Camper-Wasserwaage

Eine Wasserwaage für den Camper auf einem Tessera-Display. Die Kachel zeigt eine Blase im Ring und zwei kleine
Fahrzeuge, eins von der Seite (Längsneigung, die Front rechts) und eins von hinten (Querneigung), die so kippen wie das
Fahrzeug, jeweils mit ihrem Winkel. Grün, solange es eben steht, gelb, wenn es fast eben steht, darüber rot. Ein Tipp
öffnet eine Karte, die zeigt, wie hoch der Keil unter jedem der vier Räder sein muss.

Gemacht für den Selbstbau-Levelsensor
[ESP32-Adrafruit-BNO085-HomeAssistant-Caravan-Levelsensor-MQTT](https://github.com/klehnst/ESP32-Adrafruit-BNO085-HomeAssistant-Caravan-Levelsensor-MQTT)
(ESP32 + Adafruit BNO085, MQTT-Discovery); es geht mit jedem Paar aus Längs- und Quersensor in Home Assistant.

Auf einer Zelle ist die Kachel die Blase mit beiden Winkeln darunter; breit steht die Blase links und die Fahrzeuge
rechts; quadratisch oder hoch stehen die Fahrzeuge unter der Blase.

## Einrichten

1. Den Template-Sensor aus [`home_assistant/camper_level_template.yaml`](home_assistant/camper_level_template.yaml) in
   die `configuration.yaml` von Home Assistant übernehmen und unter `variables` die Entity-IDs von Längsneigung,
   Querneigung und Keilen deines Levelsensors eintragen (Einstellungen, Geräte & Dienste, MQTT, das Gerät des Sensors).
   Home Assistant einmal neu starten. Prüfen, dass `sensor.camper_level_for_screens` die Attribute `pitch` und `roll`
   hat.
2. In Tessera **Plugins**, **Add with a link** öffnen und den Link dieses Repositorys einfügen. Das Plugin einem
   Display hinzufügen; das Display baut einmal.
3. In **Layout** die Kachel **Camper-Wasserwaage** aus der Gruppe Plugins platzieren (2 x 1 zeigt Blase und beide
   Fahrzeuge) und im Inspector `sensor.camper_level_for_screens` wählen.
4. Eine Ecke etwas anheben (oder auf einen Keil fahren) und die Richtungen prüfen: Das Seitenfahrzeug hebt das Ende,
   das höher steht, das Heckfahrzeug die Seite, die höher steht. Läuft eine Achse falsch herum,
   **Längsneigung umdrehen** oder **Querneigung umdrehen** einschalten; das dreht Blase und Fahrzeug dieser Achse
   zusammen.

## Optionen

- **Eben bis** (1°) und **Fast eben bis** (3°): wo grün zu gelb und gelb zu rot wird.
- **Bereich der Blase** (10°): bei diesem Winkel erreicht die Blase den Ring.
- **Neigung der Fahrzeuge** (3): wie stark die Fahrzeuge den Winkel übertreiben, damit auch ein Grad sichtbar ist. 1 ist
  echt; mehr als 25° kippen sie nie.
- **Längsneigung umdrehen**, **Querneigung umdrehen**: für einen andersherum eingebauten Sensor.

## Keile

Ein Tipp auf die Kachel öffnet eine Karte mit Längs- und Querneigung und dem Fahrzeug von oben, neben jedem Rad die
Keilhöhe in cm; ein Rad, das einen Keil braucht, ist in der Akzentfarbe. Die Höhen kommen vom Levelsensor (Modus
Wohnmobil, Spurweite und Radstand in seiner Weboberfläche eingestellt). Ohne Keilwerte sagt die Karte das. Die Karte
färbt mit den Standardgrenzen (1° und 3°).

## Gut zu wissen

- Die Werte kommen über Tessera aufs Display, sobald sich der Template-Sensor ändert; das Display fragt selbst nichts ab.
- Ist der Levelsensor offline (MQTT Last Will), ist der Template-Sensor nicht verfügbar und die Kachel sagt das.
- Kein Internetdienst und nichts im Netz des Displays: nur die Home-Assistant-Entität, die du wählst.
- Läuft auf jedem Board, auch dem CYD.
