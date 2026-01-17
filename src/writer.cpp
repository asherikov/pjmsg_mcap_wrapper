/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include "HeaderCdrAux.ipp"
#include "StatisticsNamesCdrAux.ipp"
#include "StatisticsValuesCdrAux.ipp"
#include "TimeCdrAux.ipp"
#include "pjmsg_mcap_wrapper/all.h"

#define MCAP_IMPLEMENTATION
#define MCAP_COMPRESSION_NO_LZ4
#define MCAP_COMPRESSION_NO_ZSTD
#define MCAP_PUBLIC __attribute__((visibility("hidden")))

#pragma GCC diagnostic push
/// @todo presumably GCC bug
#pragma GCC diagnostic ignored "-Warray-bounds"
#include <mcap/writer.hpp>
#pragma GCC diagnostic pop

#include "messages.h"

#include <random>

namespace
{
  template <typename... t_String>
  std::string str_concat(t_String&&... strings)
  {
    std::string result;
    (result += ... += std::forward<t_String>(strings));
    return result;
  }

  uint64_t now()
  {
    return (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  }

  uint32_t getRandomUInt32()
  {
    std::mt19937 gen((std::random_device())());

    std::uniform_int_distribution<uint32_t> distrib(std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());

    return (distrib(gen));
  }
} // namespace

namespace pjmsg_mcap_wrapper
{
  class Message::Implementation
  {
  public:
    plotjuggler_msgs::msg::StatisticsNames names_;
    plotjuggler_msgs::msg::StatisticsValues values_;

    bool version_updated_;

  public:
    Implementation() { setVersion(getRandomUInt32()); }

    void setVersion(const uint32_t version)
    {
      if (names_.names_version() != version) {
        names_.names_version() = version;
        values_.names_version(names_.names_version());
        version_updated_ = true;
      }
    }
  };
} // namespace pjmsg_mcap_wrapper

namespace pjmsg_mcap_wrapper
{
  Message::Message() : pimpl_(std::make_unique<Message::Implementation>())
  {
  }

  Message::~Message() = default;

  std::vector<std::string>& Message::names()
  {
    return (pimpl_->names_.names());
  }

  std::vector<double>& Message::values()
  {
    return (pimpl_->values_.values());
  }

  void Message::bumpVersion()
  {
    if (not pimpl_->version_updated_) {
      pimpl_->setVersion(pimpl_->names_.names_version() + 1);
    }
  }

  void Message::setStamp(const uint64_t timestamp)
  {
    const int32_t sec = static_cast<int32_t>(timestamp / std::nano::den);
    const uint32_t nanosec = timestamp % std::nano::den;

    pimpl_->names_.header().stamp().sec(sec);
    pimpl_->values_.header().stamp().sec(sec);
    pimpl_->names_.header().stamp().nanosec(nanosec);
    pimpl_->values_.header().stamp().nanosec(nanosec);
  }

  void Message::reserve(const std::size_t size)
  {
    pimpl_->names_.names().reserve(size);
    pimpl_->values_.values().reserve(size);
  }

  void Message::resize(const std::size_t size)
  {
    pimpl_->names_.names().resize(size);
    pimpl_->values_.values().resize(size);
  }

  std::string& Message::name(const std::size_t index)
  {
    return (names()[index]);
  }

  double& Message::value(const std::size_t index)
  {
    return (values()[index]);
  }

  std::size_t Message::size() const
  {
    return (pimpl_->names_.names().size());
  }

  void Message::setVersion(const uint32_t version)
  {
    pimpl_->setVersion(version);
  }

  uint32_t Message::getVersion() const
  {
    return pimpl_->names_.names_version();
  }
} // namespace pjmsg_mcap_wrapper

namespace pjmsg_mcap_wrapper
{
  class Writer::Implementation
  {
  protected:
    template <class t_Message>
    class Channel
    {
    protected:
      mcap::Message message_;
      eprosima::fastcdr::CdrSizeCalculator cdr_size_calculator_;

    protected:
      uint32_t getSize(const t_Message& message)
      {
        size_t current_alignment{ 0 };
        return (cdr_size_calculator_.calculate_serialized_size(message, current_alignment) + 4u /*encapsulation*/);
      }

    public:
      Channel() : cdr_size_calculator_(eprosima::fastcdr::CdrVersion::XCDRv1) {}

      void initialize(mcap::McapWriter& writer, const std::string_view& msg_topic)
      {
        mcap::Schema schema(pjmsg_mcap_wrapper_private::pjmsg::Message<t_Message>::type, "ros2msg",
                            pjmsg_mcap_wrapper_private::pjmsg::Message<t_Message>::schema);
        writer.addSchema(schema);

        mcap::Channel channel(msg_topic, "ros2msg", schema.id);
        writer.addChannel(channel);

        message_.channelId = channel.id;
      }

      void write(mcap::McapWriter& writer, std::vector<std::byte>& buffer, const t_Message& message)
      {
        buffer.resize(getSize(message));
        message_.data = buffer.data();

        {
          eprosima::fastcdr::FastBuffer cdr_buffer(reinterpret_cast<char*>(buffer.data()), buffer.size()); // NOLINT
          eprosima::fastcdr::Cdr ser(cdr_buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::CdrVersion::XCDRv1);
          ser.set_encoding_flag(eprosima::fastcdr::EncodingAlgorithmFlag::PLAIN_CDR);

          ser.serialize_encapsulation();
          ser << message;
          ser.set_dds_cdr_options({ 0, 0 });

          message_.dataSize = ser.get_serialized_data_length();
        }

        message_.logTime = now();
        message_.publishTime = message_.logTime;

        const mcap::Status res = writer.write(message_);
        if (not res.ok()) {
          throw std::runtime_error(str_concat("Failed to write a message: ", res.message));
        }
      }
    };

  public:
    std::tuple<Channel<plotjuggler_msgs::msg::StatisticsNames>, Channel<plotjuggler_msgs::msg::StatisticsValues>> channels_;

    std::vector<std::byte> buffer_;
    mcap::McapWriter writer_;

  public:
    ~Implementation() { writer_.close(); }

    void initialize(const std::filesystem::path& filename, const std::string& topic_prefix)
    {
      {
        mcap::McapWriterOptions options = mcap::McapWriterOptions("ros2msg");
        /// @todo needed if compression is used, delays writing
        options.noChunking = true;
        const mcap::Status res = writer_.open(filename.native(), options);
        if (not res.ok()) {
          throw std::runtime_error(str_concat("Failed to open ", filename.native(), " for writing: ", res.message));
        }
      }

      std::get<Channel<plotjuggler_msgs::msg::StatisticsNames>>(channels_).initialize(writer_, str_concat(topic_prefix, "/names"));

      std::get<Channel<plotjuggler_msgs::msg::StatisticsValues>>(channels_).initialize(writer_, str_concat(topic_prefix, "/values"));
    }

    template <class t_Message>
    void write(const t_Message& message)
    {
      std::get<Channel<t_Message>>(channels_).write(writer_, buffer_, message);
    }
  };
} // namespace pjmsg_mcap_wrapper

namespace pjmsg_mcap_wrapper
{
  Writer::Writer() : pimpl_(std::make_unique<Writer::Implementation>())
  {
  }

  Writer::~Writer() = default;

  void Writer::initialize(const std::filesystem::path& filename, const std::string& topic_prefix)
  {
    pimpl_->initialize(filename, topic_prefix);
  }

  void Writer::flush()
  {
    // pimpl_->writer_.closeLastChunk();
    pimpl_->writer_.dataSink()->flush();
  }

  void Writer::write(const Message& message)
  {
    if (message.pimpl_->version_updated_) {
      pimpl_->write(message.pimpl_->names_);
      message.pimpl_->version_updated_ = false;
    }
    pimpl_->write(message.pimpl_->values_);
  }
} // namespace pjmsg_mcap_wrapper
