#pragma once

#include <client/AeronArchive.h>
#include <client/ReplayMerge.h>

#include <Aeron.h>
#include <ChannelUri.h>
#include <ChannelUriStringBuilder.h>
#include <cstddef>
#include <memory>
#include <vector>

namespace aquatic::aeron {

class AeronPublication {
  std::shared_ptr<::aeron::Aeron> aeron_;
  std::shared_ptr<::aeron::Publication> publication_;

public:
  AeronPublication(std::shared_ptr<::aeron::Aeron> aeron, const std::string &channel) : aeron_(std::move(aeron)) {
    const auto registration = aeron_->addPublication(channel, 0);

    do {
      std::this_thread::yield();
      publication_ = aeron_->findPublication(registration);
    } while (!publication_);
  }

  void write() {
    std::vector<std::byte> payload{std::byte{5}};

    ::aeron::concurrent::logbuffer::BufferClaim claim;
    while (true) {
      const auto res = publication_->tryClaim(payload.size(), claim);
      if (res >= 0) {
        auto offset = claim.offset();
        claim.buffer().putBytes(offset, reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
        offset += payload.size();
        claim.commit();
        return;
      }

      switch (res) {
      case ::aeron::NOT_CONNECTED:
      default: throw std::runtime_error("Not connected");
      }
    }
  }

  const std::string &channel() const { return publication_->channel(); }
  int stream_id() const { return publication_->streamId(); }
  int session_id() const { return publication_->sessionId(); }
  bool is_connected() const { return publication_->isConnected(); }
};

class AeronSubscription {
  std::shared_ptr<::aeron::Aeron> aeron_;
  std::shared_ptr<::aeron::Subscription> subscription_;
  bool has_seen_data = false;

public:
  AeronSubscription(std::shared_ptr<::aeron::Aeron> aeron, std::string channel, int32_t stream)
      : aeron_(std::move(aeron)) {
    const auto registration = aeron_->addSubscription(channel, stream);

    do {
      std::this_thread::yield();
      subscription_ = aeron_->findSubscription(registration);
    } while (!subscription_);
  }

  std::shared_ptr<::aeron::Subscription> subscription() { return subscription_; }

  int poll(int fragment_limit) {
    return subscription_->poll(
        [this](::aeron::concurrent::AtomicBuffer &, int32_t, int32_t, ::aeron::concurrent::logbuffer::Header &) {
          if (!has_seen_data) {
            has_seen_data = true;
            std::cout << "Got data; will not log any further in this callback." << std::endl;
          }
        },
        fragment_limit);
  }
};

class AeronArchive final {
  std::shared_ptr<::aeron::archive::client::AeronArchive> archive_;

public:
  AeronArchive(
      std::shared_ptr<::aeron::Aeron> aeron,
      const std::string &control_request_channel,
      const std::string &control_response_channel) {
    ::aeron::archive::client::Context ctx;
    ctx.aeron(std::move(aeron));
    ctx.ownsAeronClient(false);
    ctx.controlRequestChannel(control_request_channel);
    ctx.controlResponseChannel(control_response_channel);
    archive_ = ::aeron::archive::client::AeronArchive::connect(ctx);
  }

  void start_recording(const AeronPublication &publication) {
    const auto recorded_channel = ::aeron::ChannelUri::addSessionId(publication.channel(), publication.session_id());
    archive_->startRecording(
        recorded_channel, publication.stream_id(), ::aeron::archive::client::AeronArchive::REMOTE, true);

    std::cout << "Attempting recording of " << recorded_channel << std::endl;
    while (!publication.is_connected()) {
      std::this_thread::yield();
    }

    std::cout << "Started recording of channel " << recorded_channel << std::endl;
  }

  int32_t get_latest_recording_id() {
    const auto record_count = 1000;
    auto returned_results = 0;
    int64_t max_recording_id = -1;
    do {
      returned_results = archive_->listRecordingsForUri(
          max_recording_id + 1,
          record_count,
          "",
          0,
          [&max_recording_id](
              [[maybe_unused]] std::int64_t controlSessionId,
              [[maybe_unused]] std::int64_t correlationId,
              [[maybe_unused]] std::int64_t recordingId,
              [[maybe_unused]] std::int64_t startTimestamp,
              [[maybe_unused]] std::int64_t stopTimestamp,
              [[maybe_unused]] std::int64_t startPosition,
              [[maybe_unused]] std::int64_t stopPosition,
              [[maybe_unused]] std::int32_t initialTermId,
              [[maybe_unused]] std::int32_t segmentFileLength,
              [[maybe_unused]] std::int32_t termBufferLength,
              [[maybe_unused]] std::int32_t mtuLength,
              [[maybe_unused]] std::int32_t sessionId,
              [[maybe_unused]] std::int32_t streamId,
              [[maybe_unused]] const std::string &strippedChannel,
              [[maybe_unused]] const std::string &originalChannel,
              [[maybe_unused]] const std::string &sourceIdentity) {
            if (stopPosition == startPosition) return;
            max_recording_id = std::max(max_recording_id, recordingId);
          });
    } while (returned_results == record_count);

    return max_recording_id;
  }

  std::unique_ptr<AeronSubscription> replay_merge(
      const std::string &live_destination,
      const std::string &replay_endpoint,
      int32_t stream_id,
      int64_t recording_id,
      int64_t start_position) {
    auto subscription = std::make_unique<AeronSubscription>(
        archive_->context().aeron(),
        ::aeron::ChannelUriStringBuilder()
            .media(::aeron::UDP_MEDIA)
            .controlMode(::aeron::MDC_CONTROL_MODE_MANUAL)
            .build(),
        stream_id);

    const auto replay_channel = ::aeron::ChannelUriStringBuilder().media(::aeron::UDP_MEDIA).build();
    const auto replay_destination =
        ::aeron::ChannelUriStringBuilder().media(::aeron::UDP_MEDIA).endpoint(replay_endpoint).build();

    const auto description = std::string("replay_channel=") + replay_channel
                             + ", replay_destination=" + replay_destination + ", live_destination=" + live_destination
                             + ", recording_id=" + std::to_string(recording_id);
    std::cout << "Launching replay merge with " << description << std::endl;

    auto replay_merge = ::aeron::archive::client::ReplayMerge(
        subscription->subscription(),
        archive_,
        replay_channel,
        replay_destination,
        live_destination,
        recording_id,
        start_position);

    bool isLiveAdded = false;
    while (!replay_merge.isMerged()) {
      auto image = replay_merge.image();
      if (image) {
        const auto count = image->activeTransportCount();
        if (count >= 2) std::cout << "Active transport count: " << count << std::endl;
        if (!isLiveAdded && replay_merge.isLiveAdded()) {
          isLiveAdded = true;
          std::cout << "Live destination added" << std::endl;
        }
      }

      // if (replay_merge.hasFailed()) throw std::runtime_error(std::string("Replay merge failed for ") + description);
      replay_merge.doWork();
      subscription->poll(1);
    }
    std::cout << "Replay has caught up" << std::endl;

    return subscription;
  }
};

}
