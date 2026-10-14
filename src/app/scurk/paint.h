// Paint The Town: the pixel editor of one object's graphics.
#pragma once
#include <memory>

#include "app/scurk/editors.h"

namespace sc2k::scurk {
std::unique_ptr<Editor> makePaint(Env& env, ResourceEditors& host);
}
