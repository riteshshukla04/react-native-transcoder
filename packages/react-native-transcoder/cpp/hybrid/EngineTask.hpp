#pragma once

#include "media/Engine.hpp"

#include <NitroModules/Promise.hpp>

#include <exception>
#include <memory>
#include <type_traits>
#include <utility>

namespace margelo::nitro::transcoder {

// Runs `work` on an engine-owned worker and settles a Nitro Promise with it.
// Nothing here ever touches the JS thread beyond creating the Promise itself.
template<typename TResult, typename TWork>
std::shared_ptr<Promise<TResult>> runOnEngine(TWork&& work) {
  auto promise = Promise<TResult>::create();
  MediaEngine::shared().executor().submit([promise, work = std::forward<TWork>(work)]() mutable {
    try {
      if constexpr (std::is_void_v<TResult>) {
        work();
        promise->resolve();
      } else {
        promise->resolve(work());
      }
    } catch (...) {
      promise->reject(std::current_exception());
    }
  });
  return promise;
}

} // namespace margelo::nitro::transcoder
