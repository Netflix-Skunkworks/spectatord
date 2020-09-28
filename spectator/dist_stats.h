#pragma once

#include "id.h"

namespace spectator {

struct DistStats {
  Id total;
  Id totalSq;
  Id max;
  Id count;

  DistStats(const Id& base, StrRef total_stat)
      : total{base.WithStat(total_stat)},
        totalSq{base.WithStat(refs().totalOfSquares())},
        max{base.WithStat(refs().max())},
        count{base.WithStat(refs().count())} {}
};

}  // namespace spectator