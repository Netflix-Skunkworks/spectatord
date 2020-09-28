#pragma once
#include "../percentile_buckets.h"
#include "../registry.h"
#include <array>

namespace spectator::detail {

#include "percentile_bucket_tags.inc"

using counters_t =
    std::array<std::shared_ptr<Counter>, PercentileBucketsLength()>;

struct lazy_policy {
  static void init(Registry* /* r */, const Id& /* id */,
                   counters_t* /* counters */,
                   const std::string* /* perc_tags */) {}
  static std::shared_ptr<Counter> get_counter(Registry* r, const Id& id,
                                              counters_t* counters,
                                              size_t index,
                                              const std::string* perc_tags) {
    using spectator::refs;
    auto& c = counters->at(index);
    if (!c) {
      auto counterId =
          id.WithTags(refs().statistic(), refs().percentile(),
              refs().percentile(), intern_str(perc_tags[index]));
      counters->at(index) = r->GetCounter(std::move(counterId));
      c = counters->at(index);
    }
    return c;
  }
};

}  // namespace spectator::detail