#include "common_refs.h"

namespace spectator {

Refs& refs() {
  static auto* the_refs = new Refs();
  return *the_refs;
}

}  // namespace spectator