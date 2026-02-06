/**
    @file
    @author Alexander Sherikov
    @copyright 2025-2026 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include "pjmsg_mcap_wrapper/message.h"
#include "3rdparty.h"
#include "message_impl.h"

#include <random>


namespace
{
    uint32_t getRandomUInt32()
    {
        std::mt19937 gen((std::random_device())());

        std::uniform_int_distribution<uint32_t> distrib(
                std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());

        return (distrib(gen));
    }
}  // namespace


namespace pjmsg_mcap_wrapper
{
    Message::Implementation::Implementation()
    {
        setVersion(getRandomUInt32());
    }

    void Message::Implementation::setVersion(const uint32_t version)
    {
        if (names_.names_version() != version)
        {
            names_.names_version() = version;
            values_.names_version(names_.names_version());
            version_updated_ = true;
        }
    }
}  // namespace pjmsg_mcap_wrapper


namespace pjmsg_mcap_wrapper
{
    Message::Message() : pimpl_(std::make_unique<Message::Implementation>())
    {
    }

    Message::~Message() = default;

    std::vector<std::string> &Message::names()
    {
        return (pimpl_->names_.names());
    }

    std::vector<double> &Message::values()
    {
        return (pimpl_->values_.values());
    }

    void Message::bumpVersion()
    {
        if (not pimpl_->version_updated_)
        {
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

    uint64_t Message::getStamp() const
    {
        return (static_cast<uint64_t>(pimpl_->values_.header().stamp().sec()) * std::nano::den)
               + pimpl_->values_.header().stamp().nanosec();
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

    std::string &Message::name(const std::size_t index)
    {
        return (names()[index]);
    }

    double &Message::value(const std::size_t index)
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
}  // namespace pjmsg_mcap_wrapper
