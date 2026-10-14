// Place & Print: the city editor. It is an adapter between the resource-editor host (SCURK menu, file browser, prompts) and the application's own map view,
// which main.cpp runs in an "editing" mode (free tools, no simulation) through the CityBridge.
#pragma once
#include <memory>

#include "app/scurk/editors.h"

namespace sc2k::scurk {
std::unique_ptr<Editor> makePlacePrint(Env& env, ResourceEditors& host, CityBridge& bridge);
}
