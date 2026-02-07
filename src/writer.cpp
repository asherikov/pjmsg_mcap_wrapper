/**
    @file
    @author Alexander Sherikov
    @copyright 2025-2026 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include "pjmsg_mcap_wrapper/writer.h"
#include "3rdparty.h"
#include "util.h"
#include "message_impl.h"
#include "plotjuggler_msgs.h"

#pragma GCC diagnostic push
/// @todo presumably GCC bug
#pragma GCC diagnostic ignored "-Warray-bounds"
#include <mcap/writer.hpp>
#pragma GCC diagnostic pop


namespace
{
    uint64_t now()
    {
        return (std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count());
    }
}  // namespace


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
            uint32_t getSize(const t_Message &message)
            {
                size_t current_alignment{ 0 };
                return (cdr_size_calculator_.calculate_serialized_size(message, current_alignment)
                        + 4u /*encapsulation*/);
            }

        public:
            Channel() : cdr_size_calculator_(eprosima::fastcdr::CdrVersion::XCDRv1)
            {
            }

            void initialize(mcap::McapWriter &writer, const std::string_view &msg_topic)
            {
                mcap::Schema schema(
                        pjmsg_mcap_wrapper_private::pjmsg::Message<t_Message>::type,
                        "ros2msg",
                        pjmsg_mcap_wrapper_private::pjmsg::Message<t_Message>::schema);
                writer.addSchema(schema);

                mcap::Channel channel(msg_topic, "ros2msg", schema.id);
                writer.addChannel(channel);

                message_.channelId = channel.id;
            }

            void write(mcap::McapWriter &writer, std::vector<std::byte> &buffer, const t_Message &message)
            {
                buffer.resize(getSize(message));
                message_.data = buffer.data();

                {
                    eprosima::fastcdr::FastBuffer cdr_buffer(
                            reinterpret_cast<char *>(buffer.data()), buffer.size());  // NOLINT
                    eprosima::fastcdr::Cdr ser(
                            cdr_buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::CdrVersion::XCDRv1);
                    ser.set_encoding_flag(eprosima::fastcdr::EncodingAlgorithmFlag::PLAIN_CDR);

                    ser.serialize_encapsulation();
                    ser << message;
                    ser.set_dds_cdr_options({ 0, 0 });

                    message_.dataSize = ser.get_serialized_data_length();
                }

                message_.logTime = now();
                message_.publishTime = message_.logTime;


                const mcap::Status res = writer.write(message_);
                SHARF_THROW_IF(not res.ok(), "Failed to write a message: ", res.message);
            }
        };

    public:
        std::tuple<Channel<plotjuggler_msgs::msg::StatisticsNames>, Channel<plotjuggler_msgs::msg::StatisticsValues>>
                channels_;

        std::vector<std::byte> buffer_;
        mcap::McapWriter writer_;

    public:
        ~Implementation()
        {
            writer_.close();
        }

        void initialize(
                const std::filesystem::path &filename,
                const std::string &topic_prefix,
                const Writer::Parameters &params)
        {
            {
                mcap::McapWriterOptions options = mcap::McapWriterOptions("ros2msg");

                // Set compression based on parameters
                switch (params.compression_)
                {
                    case Writer::Parameters::Compression::ZSTD:
                        options.noChunking = false;
                        options.compression = mcap::Compression::Zstd;
                        break;
                    case Writer::Parameters::Compression::NONE:
                    default:
                        options.noChunking = true;
                        options.compression = mcap::Compression::None;
                        break;
                }

                const mcap::Status res = writer_.open(filename.native(), options);
                if (not res.ok())
                {
                    throw std::runtime_error(
                            str_concat("Failed to open ", filename.native(), " for writing: ", res.message));
                }
            }

            std::get<Channel<plotjuggler_msgs::msg::StatisticsNames>>(channels_).initialize(
                    writer_, str_concat(topic_prefix, "/names"));

            std::get<Channel<plotjuggler_msgs::msg::StatisticsValues>>(channels_).initialize(
                    writer_, str_concat(topic_prefix, "/values"));
        }

        template <class t_Message>
        void write(const t_Message &message)
        {
            std::get<Channel<t_Message>>(channels_).write(writer_, buffer_, message);
        }
    };
}  // namespace pjmsg_mcap_wrapper


namespace pjmsg_mcap_wrapper
{
    Writer::Writer() : pimpl_(std::make_unique<Writer::Implementation>())
    {
    }

    Writer::~Writer() = default;

    void Writer::initialize(
            const std::filesystem::path &filename,
            const std::string &topic_prefix,
            const Writer::Parameters &params)
    {
        pimpl_->initialize(filename, topic_prefix, params);
    }

    void Writer::flush()
    {
        // pimpl_->writer_.closeLastChunk();
        pimpl_->writer_.dataSink()->flush();
    }

    void Writer::write(const Message &message)
    {
        if (message.pimpl_->version_updated_)
        {
            pimpl_->write(message.pimpl_->names_);
            message.pimpl_->version_updated_ = false;
        }
        pimpl_->write(message.pimpl_->values_);
    }
}  // namespace pjmsg_mcap_wrapper
