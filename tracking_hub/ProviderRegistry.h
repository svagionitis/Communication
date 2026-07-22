/**
 * @file ProviderRegistry.h
 * @brief Extensible registry managing active tracking providers.
 */

#pragma once

#include "ITrackingProvider.h"

#include <memory>
#include <vector>

namespace Communication {

class ProviderRegistry {
public:
    ProviderRegistry() = default;
    ~ProviderRegistry();

    void addProvider(std::unique_ptr<ITrackingProvider> provider);
    bool startAll(FrameCallback callback);
    void stopAll();

    std::size_t count() const { return m_providers.size(); }
    void clear();

private:
    std::vector<std::unique_ptr<ITrackingProvider>> m_providers;
};

} // namespace Communication
