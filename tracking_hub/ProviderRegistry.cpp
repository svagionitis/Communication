/**
 * @file ProviderRegistry.cpp
 * @brief ProviderRegistry implementation.
 */

#include "ProviderRegistry.h"

#include <glog/logging.h>

namespace Communication {

ProviderRegistry::~ProviderRegistry()
{
    stopAll();
}

void ProviderRegistry::addProvider(std::unique_ptr<ITrackingProvider> provider)
{
    if (provider) {
        LOG(INFO) << "Registered tracking provider: " << provider->name();
        m_providers.push_back(std::move(provider));
    }
}

bool ProviderRegistry::startAll(FrameCallback callback)
{
    bool success = true;
    for (auto& provider : m_providers) {
        if (!provider->start(callback)) {
            LOG(ERROR) << "Failed to start tracking provider: " << provider->name();
            success = false;
        }
    }
    return success;
}

void ProviderRegistry::stopAll()
{
    for (auto& provider : m_providers) {
        if (provider->isRunning()) {
            provider->stop();
        }
    }
}

void ProviderRegistry::clear()
{
    stopAll();
    m_providers.clear();
}

} // namespace Communication
