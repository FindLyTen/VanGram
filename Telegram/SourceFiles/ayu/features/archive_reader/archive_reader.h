// VanGram: periodically mark archived chats as read on all accounts.
//
// Telegram notifications from archived chats are suppressed anyway, but
// the unread counters keep growing. This feature periodically iterates
// all logged-in accounts and marks everything in their Archive folder
// as read (same mechanism as the context-menu "Mark as read" on the
// archive folder), with a per-account pause to be gentle to the API.
#pragma once

namespace Ayu::ArchiveReader {

// Starts the repeating timer (called once from AyuInfra::init).
void init();

// Runs one pass right now (also used by the timer).
void runOnce();

} // namespace Ayu::ArchiveReader
