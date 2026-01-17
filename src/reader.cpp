/**
    @file
    @author  Damien SIX
*/

#include "pjmsg_mcap_wrapper/reader.h"

#include "HeaderCdrAux.ipp"
#include "StatisticsNamesCdrAux.ipp"
#include "StatisticsValuesCdrAux.ipp"
#include "TimeCdrAux.ipp"

#define MCAP_IMPLEMENTATION
#define MCAP_PUBLIC __attribute__((visibility("hidden")))

#pragma GCC diagnostic push
/// @todo presumably GCC bug
#pragma GCC diagnostic ignored "-Warray-bounds"
#include <mcap/reader.hpp>
#pragma GCC diagnostic pop

#include "messages.h"

#include <fstream>
#include <optional>
#include <unordered_map>

namespace
{
  template <typename... t_String>
  std::string str_concat(t_String&&... strings)
  {
    std::string result;
    (result += ... += std::forward<t_String>(strings));
    return result;
  }
} // namespace

namespace pjmsg_mcap_wrapper
{
  class Reader::Implementation
  {
  protected:
    template <class t_Message>
    class Channel
    {
    public:
      mcap::ChannelId channel_id_;
      bool has_data_;
      t_Message message_;

    public:
      Channel() : channel_id_(0), has_data_(false) {}

      void setChannelId(const mcap::ChannelId id) { channel_id_ = id; }

      bool matches(const mcap::ChannelId id) const { return (channel_id_ == id); }

      void read(const mcap::Message& mcap_message)
      {
        eprosima::fastcdr::FastBuffer cdr_buffer(reinterpret_cast<char*>(const_cast<std::byte*>(mcap_message.data)), // NOLINT
                                                 mcap_message.dataSize);
        eprosima::fastcdr::Cdr deser(cdr_buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::CdrVersion::XCDRv1);

        deser.read_encapsulation();
        deser >> message_;

        has_data_ = true;
      }

      const t_Message& getMessage() const { return message_; }
    };

  public:
    std::tuple<Channel<plotjuggler_msgs::msg::StatisticsNames>, Channel<plotjuggler_msgs::msg::StatisticsValues>> channels_;

    std::ifstream file_stream_;
    mcap::McapReader reader_;
    std::unique_ptr<mcap::LinearMessageView> message_view_;
    std::optional<mcap::LinearMessageView::Iterator> current_message_;

    uint32_t current_names_version_;
    bool names_updated_;

  public:
    Implementation() : current_names_version_(0), names_updated_(false) {}

    ~Implementation()
    {
      message_view_.reset();
      reader_.close();
      file_stream_.close();
    }

    void initialize(const std::filesystem::path& filename)
    {
      file_stream_.open(filename, std::ios::binary);
      if (not file_stream_.is_open()) {
        throw std::runtime_error(str_concat("Failed to open ", filename.native(), " for reading"));
      }

      const mcap::Status res = reader_.open(file_stream_);
      if (not res.ok()) {
        throw std::runtime_error(str_concat("Failed to open MCAP file: ", res.message));
      }

      // Map channel names to channel IDs
      std::unordered_map<std::string, mcap::ChannelId> channel_map;
      for (const auto& [channel_id, channel_ptr] : reader_.channels()) {
        channel_map[channel_ptr->topic] = channel_id;
      }

      // Find and set channel IDs for our message types
      for (const auto& [topic, channel_id] : channel_map) {
        if (topic.find("/names") != std::string::npos) {
          std::get<Channel<plotjuggler_msgs::msg::StatisticsNames>>(channels_).setChannelId(channel_id);
        } else if (topic.find("/values") != std::string::npos) {
          std::get<Channel<plotjuggler_msgs::msg::StatisticsValues>>(channels_).setChannelId(channel_id);
        }
      }

      // Create message view for linear iteration
      message_view_ = std::make_unique<mcap::LinearMessageView>(reader_.readMessages());
      current_message_ = message_view_->begin();
    }

    template <class t_Message>
    void readIfMatches(const mcap::Message& mcap_message)
    {
      if (std::get<Channel<t_Message>>(channels_).matches(mcap_message.channelId)) {
        std::get<Channel<t_Message>>(channels_).read(mcap_message);
      }
    }

    bool hasNext() const { return (message_view_ && current_message_.has_value() && *current_message_ != message_view_->end()); }

    bool read(Message& message)
    {
      if (not hasNext()) {
        return false;
      }

      // Reset data flags
      std::get<Channel<plotjuggler_msgs::msg::StatisticsNames>>(channels_).has_data_ = false;
      std::get<Channel<plotjuggler_msgs::msg::StatisticsValues>>(channels_).has_data_ = false;

      // Read messages until we get a values message (which is what we return)
      while (hasNext()) {
        const mcap::MessageView& msg_view = **current_message_;
        ++(*current_message_);

        readIfMatches<plotjuggler_msgs::msg::StatisticsNames>(msg_view.message);
        readIfMatches<plotjuggler_msgs::msg::StatisticsValues>(msg_view.message);

        // If we got a names message, update the version tracking
        if (std::get<Channel<plotjuggler_msgs::msg::StatisticsNames>>(channels_).has_data_) {
          const auto& names_msg = std::get<Channel<plotjuggler_msgs::msg::StatisticsNames>>(channels_).getMessage();
          if (names_msg.names_version() != current_names_version_) {
            current_names_version_ = names_msg.names_version();
            names_updated_ = true;

            // Copy names to output message
            message.names() = names_msg.names();
            message.setVersion(current_names_version_);
          }
        }

        // If we got a values message, we can return
        if (std::get<Channel<plotjuggler_msgs::msg::StatisticsValues>>(channels_).has_data_) {
          const auto& values_msg = std::get<Channel<plotjuggler_msgs::msg::StatisticsValues>>(channels_).getMessage();

          // Copy values to output message
          message.values() = values_msg.values();

          // Set timestamp from the values message
          const auto& stamp = values_msg.header().stamp();
          const uint64_t timestamp = static_cast<uint64_t>(stamp.sec()) * std::nano::den + static_cast<uint64_t>(stamp.nanosec());
          message.setStamp(timestamp);

          names_updated_ = false;
          return true;
        }
      }

      return false;
    }
  };
} // namespace pjmsg_mcap_wrapper

namespace pjmsg_mcap_wrapper
{
  Reader::Reader() : pimpl_(std::make_unique<Reader::Implementation>())
  {
  }

  Reader::~Reader() = default;

  void Reader::initialize(const std::filesystem::path& filename)
  {
    pimpl_->initialize(filename);
  }

  bool Reader::read(Message& message)
  {
    return pimpl_->read(message);
  }

  bool Reader::hasNext() const
  {
    return pimpl_->hasNext();
  }
} // namespace pjmsg_mcap_wrapper
