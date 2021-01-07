#pragma once

#include "string_intern.h"

namespace spectator {
class Refs final {
 public:
  Refs()
      : name_(intern_str("name")),
        count_(intern_str("count")),
        gauge_(intern_str("gauge")),
        totalTime_(intern_str("totalTime")),
        totalAmount_(intern_str("totalAmount")),
        totalSq_(intern_str("totalOfSquares")),
        percentile_(intern_str("percentile")),
        max_(intern_str("max")),
        statistic_(intern_str("statistic")) {}

  [[nodiscard]] auto name() const -> StrRef { return name_; }
  [[nodiscard]] auto count() const -> StrRef { return count_; }
  [[nodiscard]] auto gauge() const -> StrRef { return gauge_; }
  [[nodiscard]] auto totalTime() const -> StrRef { return totalTime_; }
  [[nodiscard]] auto totalAmount() const -> StrRef { return totalAmount_; }
  [[nodiscard]] auto totalOfSquares() const -> StrRef { return totalSq_; }
  [[nodiscard]] auto percentile() const -> StrRef { return percentile_; }
  [[nodiscard]] auto max() const -> StrRef { return max_; }
  [[nodiscard]] auto statistic() const -> StrRef { return statistic_; }

 private:
  StrRef name_;
  StrRef count_;
  StrRef gauge_;
  StrRef totalTime_;
  StrRef totalAmount_;
  StrRef totalSq_;
  StrRef percentile_;
  StrRef max_;
  StrRef statistic_;
};

auto refs() -> Refs&;

}  // namespace spectator
