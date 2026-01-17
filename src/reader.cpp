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
  /// Internal event type for decoding - represents exactly one decoded MCAP message
  struct ExtractedEvent {
    enum class Type {
      Names,
      Values
    };

    Type type;
    mcap::ChannelId channel_id;
    uint64_t log_time; // from MCAP
    uint64_t pub_time; // from MCAP publishTime

    plotjuggler_msgs::msg::StatisticsNames names;
    plotjuggler_msgs::msg::StatisticsValues values;
  };

  class Reader::Implementation
  {
  protected:
    template <class t_Message>
    class Channel
    {
    public:
      mcap::ChannelId channel_id_;
      t_Message message_;

    public:
      Channel() : channel_id_(0) {}

      void setChannelId(const mcap::ChannelId id) { channel_id_ = id; }

      bool matches(const mcap::ChannelId id) const { return (channel_id_ == id); }

      void read(const mcap::Message& mcap_message)
      {
        eprosima::fastcdr::FastBuffer cdr_buffer(reinterpret_cast<char*>(const_cast<std::byte*>(mcap_message.data)), // NOLINT
                                                 mcap_message.dataSize);
        eprosima::fastcdr::Cdr deser(cdr_buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::CdrVersion::XCDRv1);

        deser.read_encapsulation();
        deser >> message_;
      }

      const t_Message& getMessage() const { return message_; }
    };

  public:
    std::tuple<Channel<plotjuggler_msgs::msg::StatisticsNames>, Channel<plotjuggler_msgs::msg::StatisticsValues>> channels_;

    std::ifstream file_stream_;
    mcap::McapReader reader_;
    std::unique_ptr<mcap::LinearMessageView> message_view_;
    std::optional<mcap::LinearMessageView::Iterator> current_message_;

  public:
    Implementation() = default;

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

      // Read the summary section to populate internal indexes (channels, schemas, etc.)
      const mcap::Status summary_res = reader_.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan);
      if (not summary_res.ok()) {
        throw std::runtime_error(str_concat("Failed to read MCAP summary: ", summary_res.message));
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

    bool hasNext() const { return (message_view_ && current_message_.has_value() && *current_message_ != message_view_->end()); }

    /// Returns exactly one decoded MCAP message
    /// Skips unrelated channels
    bool readNextInternal(ExtractedEvent& out)
    {
      while (hasNext()) {
        const mcap::MessageView& mv = **current_message_;
        const auto& msg = mv.message;

        out.channel_id = msg.channelId;
        out.log_time = msg.logTime;
        out.pub_time = msg.publishTime;

        // Check if this is a Names message
        if (std::get<Channel<plotjuggler_msgs::msg::StatisticsNames>>(channels_).matches(msg.channelId)) {
          auto& ch = std::get<Channel<plotjuggler_msgs::msg::StatisticsNames>>(channels_);
          ch.read(msg);

          out.type = ExtractedEvent::Type::Names;
          out.names = ch.getMessage();
          ++(*current_message_);
          return true;
        }

        // Check if this is a Values message
        if (std::get<Channel<plotjuggler_msgs::msg::StatisticsValues>>(channels_).matches(msg.channelId)) {
          auto& ch = std::get<Channel<plotjuggler_msgs::msg::StatisticsValues>>(channels_);
          ch.read(msg);

          out.type = ExtractedEvent::Type::Values;
          out.values = ch.getMessage();
          ++(*current_message_);
          return true;
        }

        // else: unrelated channel → skip
        ++(*current_message_);
      }
      return false;
    }

    /// Public API: converts ExtractedEvent to Message
    /// Returns exactly one message per call - either Names or Values
    bool readNext(Message& message)
    {
      ExtractedEvent event;

      if (!readNextInternal(event)) {
        return false;
      }

      if (event.type == ExtractedEvent::Type::Names) {
        // Return a Names message
        const auto& names_msg = event.names;
        message.names() = names_msg.names();
        message.setVersion(names_msg.names_version());

        // Set timestamp from the names message
        const auto& stamp = names_msg.header().stamp();
        const uint64_t timestamp = static_cast<uint64_t>(stamp.sec()) * std::nano::den + static_cast<uint64_t>(stamp.nanosec());
        message.setStamp(timestamp);

        // Clear values since this is a Names message
        message.values().clear();
        return true;
      } else if (event.type == ExtractedEvent::Type::Values) {
        // Return a Values message
        const auto& values_msg = event.values;
        message.values() = values_msg.values();

        // Set timestamp from the values message
        const auto& stamp = values_msg.header().stamp();
        const uint64_t timestamp = static_cast<uint64_t>(stamp.sec()) * std::nano::den + static_cast<uint64_t>(stamp.nanosec());
        message.setStamp(timestamp);

        // Clear names since this is a Values message
        message.names().clear();
        return true;
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

  bool Reader::readNext(Message& message)
  {
    return pimpl_->readNext(message);
  }

  bool Reader::hasNext() const
  {
    return pimpl_->hasNext();
  }
} // namespace pjmsg_mcap_wrapper
