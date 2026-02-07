/**
    @file
    @author Alexander Sherikov, Damien SIX
    @copyright 2025-2026 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include "pjmsg_mcap_wrapper/reader.h"
#include "3rdparty.h"
#include "util.h"

#pragma GCC diagnostic push
/// @todo presumably GCC bug
#pragma GCC diagnostic ignored "-Warray-bounds"
#include <mcap/reader.hpp>
#pragma GCC diagnostic pop

#include <fstream>
#include <unordered_map>


namespace pjmsg_mcap_wrapper
{
    class Reader::Implementation
    {
    protected:
        class MCAPAccessor
        {
        public:
            mcap::LinearMessageView view_;
            mcap::LinearMessageView::Iterator iterator_;

        public:
            explicit MCAPAccessor(mcap::McapReader &reader) : view_(reader.readMessages()), iterator_(view_.begin())
            {
            }
        };

    protected:
        template <class t_Message>
        static void deserialize(const mcap::Message &mcap_message, t_Message &message)
        {
            eprosima::fastcdr::FastBuffer cdr_buffer(
                    reinterpret_cast<char *>(const_cast<std::byte *>(mcap_message.data)),  // NOLINT
                    mcap_message.dataSize);
            eprosima::fastcdr::Cdr deserializer(
                    cdr_buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::CdrVersion::XCDRv1);

            deserializer.read_encapsulation();
            deserializer >> message;
        }

    public:
        mcap::ChannelId names_channel_id_;
        mcap::ChannelId values_channel_id_;

        std::unordered_map<uint32_t, plotjuggler_msgs::msg::StatisticsNames> names_;

        mcap::McapReader reader_;
        std::unique_ptr<MCAPAccessor> accessor_;

    public:
        Implementation() = default;

        ~Implementation()
        {
            reader_.close();
        }

        void initialize(const std::filesystem::path &filename, const std::string &topic_prefix)
        {
            const mcap::Status res = reader_.open(filename.string());
            SHARF_THROW_IF(not res.ok(), "Failed to open MCAP file: ", res.message);

            // Read the summary section to populate internal indexes (channels, schemas, etc.)
            const mcap::Status summary_res = reader_.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan);
            SHARF_THROW_IF(not summary_res.ok(), "Failed to read MCAP summary: ", summary_res.message);


            // Find and set channel IDs for our message types
            const std::string names_topic = str_concat(topic_prefix, "/names");
            const std::string values_topic = str_concat(topic_prefix, "/values");
            for (auto const &it : reader_.channels())
            {
                if (it.second->topic == names_topic)
                {
                    names_channel_id_ = it.first;
                }
                else if (it.second->topic == values_topic)
                {
                    values_channel_id_ = it.first;
                }
            }

            accessor_ = std::make_unique<MCAPAccessor>(reader_);
        }

        bool next(Message &message)
        {
            while (accessor_->iterator_ != accessor_->view_.end())
            {
                if (accessor_->iterator_->message.channelId == names_channel_id_)
                {
                    plotjuggler_msgs::msg::StatisticsNames names;
                    deserialize(accessor_->iterator_->message, names);
                    SHARF_THROW_IF(
                            names_.find(names.names_version()) != names_.end(),
                            "Format error: reused names version id");
                    names_.emplace(names.names_version(), std::move(names));
                }
                else if (accessor_->iterator_->message.channelId == values_channel_id_)
                {
                    plotjuggler_msgs::msg::StatisticsValues values;
                    deserialize(accessor_->iterator_->message, values);
                    /// @todo Ordering may be off
                    SHARF_THROW_IF(names_.find(values.names_version()) == names_.end(), "Unknown names version id");

                    message.setVersion(values.names_version());
                    message.setStamp(
                            (static_cast<uint64_t>(values.header().stamp().sec()) * std::nano::den)
                            + values.header().stamp().nanosec());

                    message.names() = names_[values.names_version()].names();
                    message.values() = std::move(values.values());

                    ++accessor_->iterator_;
                    return (true);
                }
                ++accessor_->iterator_;
            }
            return (false);
        }
    };
}  // namespace pjmsg_mcap_wrapper


namespace pjmsg_mcap_wrapper
{
    Reader::Reader() : pimpl_(std::make_unique<Reader::Implementation>())
    {
    }

    Reader::~Reader() = default;

    void Reader::initialize(const std::filesystem::path &filename, const std::string &topic_prefix)
    {
        pimpl_->initialize(filename, topic_prefix);
    }

    bool Reader::next(Message &message)
    {
        return pimpl_->next(message);
    }
}  // namespace pjmsg_mcap_wrapper
