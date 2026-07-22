/**
 * @file ITrackingProvider.h
 * @brief Abstract interface for extensible telemetry tracking providers.
 */

#pragma once

#include "TargetTypes.h"

#include <functional>
#include <string>

namespace Communication {

using FrameCallback = std::function<void(const TargetFrame&)>;

/**
 * @brief Polymorphic backend interface for live target telemetry providers.
 */
class ITrackingProvider {
public:
    virtual ~ITrackingProvider() = default;

    /**
     * @brief Unique name of the tracking provider.
     */
    virtual std::string name() const = 0;

    /**
     * @brief Starts streaming target frames to the provided callback.
     * @param callback Function invoked when a valid TargetFrame is received.
     * @return True if started successfully.
     */
    virtual bool start(FrameCallback callback) = 0;

    /**
     * @brief Stops the provider worker threads/connections.
     */
    virtual void stop() = 0;

    /**
     * @brief Checks if provider is actively running.
     */
    virtual bool isRunning() const = 0;
};

} // namespace Communication
