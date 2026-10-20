#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::camper_level {

// What the template sensor of the README says, kept by a tile or a card between its calls.
struct Level {
  enum Mode : uint8_t { CHOOSE, OFFLINE, LIVE } mode = CHOOSE;
  float pitch = 0, roll = 0;
  float wedge[4] = {-1, -1, -1, -1};  // front left, front right, rear left, rear right in cm; -1: none

  // Reads on_state's data: the entity's state and its attributes.
  void read(JsonObjectConst data);
  bool has_wedges() const { return wedge[0] >= 0 && wedge[1] >= 0 && wedge[2] >= 0 && wedge[3] >= 0; }
};

// The plugin: one tile type ("level") and a card with the wedges ("wedges").
class CamperLevel : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
};

}  // namespace esphome::camper_level
