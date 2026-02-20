#pragma once

namespace engine::core
{
class FixedStepClock
{
public:
    explicit FixedStepClock(float fixed_delta_seconds = 1.0f / 120.0f,
                            int max_steps_per_frame = 8);

    int ConsumeSteps(float frame_delta_seconds);

    [[nodiscard]] float FixedDeltaSeconds() const;

    void Reset();

private:
    float fixed_delta_seconds_;
    int max_steps_per_frame_;
    float accumulator_seconds_;
};
} // namespace engine::core
