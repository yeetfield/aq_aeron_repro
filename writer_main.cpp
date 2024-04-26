#include <Aeron.h>
#include "AeronClient.hpp"
#include <iostream>

using namespace aquatic::aeron;

int main([[maybe_unused]] int argc, const char *argv[]) {
  if (argc < 5) {
    throw std::runtime_error("Usage: ./writer [aeron directory] [publication channel] [archive channel] [archive response channel]");
  }

  const std::string aeron_directory{argv[1]};
  const std::string publication_channel{argv[2]};
  const std::string archive_channel{argv[3]};
  const std::string archive_response_channel{argv[4]};
  std::cout << "Starting a publication for " << publication_channel << ", with archive at " << archive_channel
            << " and response at " << archive_response_channel << std::endl;

  ::aeron::Context context;
  context.aeronDir(aeron_directory);
  context.preTouchMappedMemory(true);
  auto aeron = ::aeron::Aeron::connect(context);

  AeronPublication publication{aeron, publication_channel};
  AeronArchive archive{aeron, archive_channel, archive_response_channel};

  // Blocks until the publication is connected
  archive.start_recording(publication);

  for (int i = 0; i < 1'000'000; ++i) {
    publication.write();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (i % 20 == 0) std::cout << "Wrote " << (i + 1) << " times" << std::endl;
  }
}
