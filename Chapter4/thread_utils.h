#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#include <pthread.h>
#include <sched.h>

namespace Common {
  /// Set affinity for current thread to be pinned to the provided core_id.
  inline auto setThreadCore(int core_id) noexcept {
    if (core_id < 0 || core_id >= CPU_SETSIZE) {
      return false;
    }

    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    return (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0);
  }

  /// Creates a thread instance, sets affinity on it, assigns it a name and
  /// passes the function to be run on that thread as well as the arguments to the function.
  template<typename T, typename... A>
  inline auto createAndStartThread(int core_id, std::string name, T &&func, A &&... args) -> std::thread {
    auto task = [core_id,
                 name = std::move(name),
                 func = std::forward<T>(func)](auto &&...unpacked_args) mutable {
      if (!name.empty()) {
        // Linux thread names are limited to 15 visible characters plus '\0'.
        const auto thread_name = name.substr(0, 15);
        pthread_setname_np(pthread_self(), thread_name.c_str());
      }

      if (core_id >= 0 && !setThreadCore(core_id)) {
        std::cerr << "Failed to set core affinity for " << name << " " << pthread_self() << " to " << core_id << std::endl;
        return;
      }

      if (core_id >= 0) {
        std::cerr << "Set core affinity for " << name << " " << pthread_self() << " to " << core_id << std::endl;
      }

      std::invoke(std::move(func), std::forward<decltype(unpacked_args)>(unpacked_args)...);
    };

    return std::thread(std::move(task), std::forward<A>(args)...);
  }
}
