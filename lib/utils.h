#pragma once

namespace loewy {

/**
 * Clamp value between min and max bounds
 * Optimized for ARM Cortex-M7 embedded audio applications
 * - No standard library dependencies
 * - Predictable branching for real-time constraints
 * - NaN values propagate through unchanged
 */
inline float clamp(float value, float min, float max) {
    return value < min ? min : (value > max ? max : value);
}

} // namespace loewy