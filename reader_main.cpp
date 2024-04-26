#include <Aeron.h>
#include "AeronClient.hpp"
#include <iostream>

using namespace aquatic::aeron;

int main([[maybe_unused]] int argc, const char *argv[]) {
  if (argc < 5) {
    throw std::runtime_error("Usage: ./reader [aeron directory] [subscription channel] [archive channel] [archive response channel] [replay_destination]");
  }

  const std::string aeron_directory{argv[1]};
  const std::string subscription_channel{argv[2]};
  const std::string archive_channel{argv[3]};
  const std::string archive_response_channel{argv[4]};
  const std::string replay_destination{argv[5]};
  std::cout << "Starting a subscription for " << subscription_channel << ", with archive at " << archive_channel
            << " and response at " << archive_response_channel << ", performing a replay-merge using replay destination "
            << replay_destination << std::endl;

  ::aeron::Context context;
  context.aeronDir(aeron_directory);
  context.preTouchMappedMemory(true);
  auto aeron = ::aeron::Aeron::connect(context);

  AeronArchive archive{aeron, archive_channel, archive_response_channel};
  const auto recording_id = archive.get_latest_recording_id();
  auto subscription = archive.replay_merge(subscription_channel, replay_destination, 0, recording_id, 0);

  while (true) subscription->poll(1);
}
