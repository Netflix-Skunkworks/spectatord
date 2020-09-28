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

  [[nodiscard]] StrRef name() const { return name_; }
  [[nodiscard]] StrRef count() const { return count_; }
  [[nodiscard]] StrRef gauge() const { return gauge_; }
  [[nodiscard]] StrRef totalTime() const { return totalTime_; }
  [[nodiscard]] StrRef totalAmount() const { return totalAmount_; }
  [[nodiscard]] StrRef totalOfSquares() const { return totalSq_; }
  [[nodiscard]] StrRef percentile() const { return percentile_; }
  [[nodiscard]] StrRef max() const { return max_; }
  [[nodiscard]] StrRef statistic() const { return statistic_; }

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

Refs& refs();

}  // namespace spectator