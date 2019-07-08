// Copyright (c) 2019 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "carla/AtomicList.h"
#include "carla/NonCopyable.h"
#include "carla/client/Timestamp.h"
#include "carla/client/detail/EpisodeProxy.h"
#include "carla/rpc/ActorId.h"

#include <memory>

namespace carla {
namespace client {
namespace detail {

  class Client;
  class EpisodeState;

  class WalkerNavigation
    : public std::enable_shared_from_this<WalkerNavigation>,
      private NonCopyable {
  public:

    explicit WalkerNavigation(Client &client);

    void RegisterWalker(ActorId walker_id, ActorId controller_id) {
      _walkers.Push(WalkerHandle{walker_id, controller_id});
    }

    void Tick(const EpisodeState &episode_state) const;

  private:

    Client &_client;

    struct WalkerHandle {
      ActorId walker;
      ActorId controller;
    };

    AtomicList<WalkerHandle> _walkers;
  };

} // namespace detail
} // namespace client
} // namespace carla
