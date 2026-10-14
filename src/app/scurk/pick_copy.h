// Pick & Copy: the Source object set on the left, the Working set on the right, one object group at a time; select objects of the source and copy them over.
#pragma once
#include <memory>

#include "app/scurk/editors.h"

namespace sc2k::scurk {
std::unique_ptr<Editor> makePickCopy(Env& env, ResourceEditors& host);
}
