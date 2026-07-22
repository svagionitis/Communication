/**
 * @file GatewayFrame.h
 * @brief Raw frame payload structure for full-duplex protocol bridge.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace Communication {

/**
 * @brief Fixed-capacity frame chunk transferred lock-free across channels.
 */
struct GatewayFrame {
    std::size_t size {0};
    uint8_t data[1024] {0};
};

} // namespace Communication
