#include "engine/core/FixedStepClock.h"

#include <algorithm>

namespace engine::core
{
FixedStepClock::FixedStepClock(float fixed_delta_seconds,
                               int max_steps_per_frame)
    : fixed_delta_seconds_(std::max(0.001f, fixed_delta_seconds)),
      max_steps_per_frame_(std::max(1, max_steps_per_frame)),
      accumulator_seconds_(0.0f)
{
}

int FixedStepClock::ConsumeSteps(float frame_delta_seconds)
{
    if (frame_delta_seconds <= 0.0f)
    {
        return 0;
    }

    const float clamped_delta = std::min(frame_delta_seconds, 0.25f);
    accumulator_seconds_ += clamped_delta;

    int steps = 0;
    while (accumulator_seconds_ >= fixed_delta_seconds_ &&
           steps < max_steps_per_frame_)
    {
        accumulator_seconds_ -= fixed_delta_seconds_;
        ++steps;
    }

    if (steps == max_steps_per_frame_)
    {
        accumulator_seconds_ = std::min(accumulator_seconds_, fixed_delta_seconds_);
    }

    return steps;
}

float FixedStepClock::FixedDeltaSeconds() const
{
    return fixed_delta_seconds_;
}

void FixedStepClock::Reset()
{
    accumulator_seconds_ = 0.0f;
}
} // namespace engine::core
