`pjmsg_mcap_writer`
===================

Logging library that writes `plotjuggler_msgs` to `MCAP` files without `ROS`
dependencies. Library incorporates `FastCDR` serializer, `MCAP` library, and
pregenerated `plotjuggler_msgs` data structures: none of these dependencies are
exposed through the API to avoid possible conflicts.
