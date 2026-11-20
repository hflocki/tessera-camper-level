# Camper level

A spirit level for a camper on a Tessera screen. The tile shows a bubble in a ring and two small vans, one from the side
(pitch, the front on the right) and one from behind (roll), that tilt as the vehicle does, each with its angle. Green
while the vehicle stands level, amber while it nearly does, red above. A tap opens a card with how high a wedge each of
the four wheels needs.

It is made for the DIY level sensor
[ESP32-Adrafruit-BNO085-HomeAssistant-Caravan-Levelsensor-MQTT](https://github.com/klehnst/ESP32-Adrafruit-BNO085-HomeAssistant-Caravan-Levelsensor-MQTT)
(ESP32 + Adafruit BNO085, MQTT discovery), and works with any pair of pitch and roll sensors in Home Assistant.

On one cell the tile is the bubble with both angles under it; on a wide tile the bubble is on the left and the vans on
the right; on a square or tall one the vans stand under the bubble.

## Set up

1. Put the template sensor of [`home_assistant/camper_level_template.yaml`](home_assistant/camper_level_template.yaml)
   into Home Assistant's `configuration.yaml`, and write the entity ids of your level sensor's pitch, roll and wedges
   into its `variables` (Settings, Devices & services, MQTT, the sensor's device). Restart Home Assistant once. Check
   that `sensor.camper_level_for_screens` has the attributes `pitch` and `roll`.
2. In Tessera, open **Plugins**, **Add with a link**, and paste the link of this repository. Add the plugin to a screen;
   the screen builds once.
3. In **Layout**, place the **Camper level** tile from the library's Plugins group (2 x 1 shows the bubble and both
   vans) and choose `sensor.camper_level_for_screens` in its inspector.
4. Lift one corner a little (or drive onto a wedge) and check the directions: the side van lifts the end that is
   higher, the rear van the side that is higher. If an axis goes the wrong way, turn **Turn pitch around** or
   **Turn roll around** on; it turns the bubble and the van of that axis together.

## Options

- **Level up to** (1° by default) and **Nearly level up to** (3°): where green turns amber, and amber red.
- **Bubble range** (10°): the angle at which the bubble reaches the ring.
- **Tilt of the vans** (3): how much the vans exaggerate the angle, so that a degree shows. 1 is true to life; they never
  tilt more than 25°.
- **Turn pitch around**, **Turn roll around**: for a sensor mounted the other way round.

## Wedges

A tap on the tile opens a card with pitch and roll and the van from above, with the wedge height in cm beside each
wheel; a wheel that needs one is in the accent colour. The heights come from the level sensor (its motorhome mode, with
the track width and the wheelbase set in its web page). Without wedge values the card says so. The card colours with the
default limits (1° and 3°).

## Good to know

- The values reach the screen through Tessera whenever the template sensor changes; the screen asks nothing itself.
- While the level sensor is offline (its MQTT Last Will), the template sensor is unavailable and the tile greys out.
- No internet service and nothing on the screen's own network: only the Home Assistant entity you choose.
- Works on every board, the CYD included.

## A hobby project

This plugin is a private, non-commercial hobby project. I build and maintain it in my spare time and share it
free of charge as open source under the [MIT License](LICENSE). It is free and will stay free: there is no paid
version, no paid feature and no paid support.

Bug reports and ideas are welcome, but please keep in mind:

- There is no support and no guaranteed response time. I answer when I find the time, which can take days or weeks.
- Not every request gets built. I add what fits the plugin and what I can keep working over time.
- Issues are for bugs and concrete ideas, not for chatting. Please use the forms, keep to one topic per issue, and
  add a 👍 reaction instead of a "+1" or "same here" comment.
- Pull requests are welcome; for a larger change, open an issue first so we can agree on the way.

### No warranty, no liability

The plugin is provided "as is", without any warranty and without guaranteed support, updates or fixes. You use it at
your own risk. To the extent permitted by law, I accept no liability for any damage or data loss resulting from its
use. The full terms are in the [MIT License](LICENSE).

### Buy me a coffee

If the plugin is useful to you and you would like to say thanks, you are welcome to buy me a coffee. A donation is
entirely voluntary: it is a thank-you, not a payment. It does not buy support, features, a response time or any other
service, and the plugin stays exactly the same for everyone, whether they donate or not.

[![Buy me a coffee](https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20coffee&emoji=&slug=hflocki&button_colour=FFDD00&font_colour=000000&font_family=Cookie&outline_colour=000000&coffee_colour=ffffff)](https://www.buymeacoffee.com/hflocki)
