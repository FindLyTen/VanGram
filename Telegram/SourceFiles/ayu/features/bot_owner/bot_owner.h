// VanGram: find which logged-in account owns a bot.
//
// "Owns" = the bot username appears in that account's chat with
// @BotFather (/newbot, /token, /revoke etc. leave traces there).
// Also reports accounts that merely talked to the bot (dialog exists).
#pragma once

class BoxContent;

namespace Ui {
class GenericBox;
class InputField;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Ayu::BotOwner {

// Opens the "Find bot owner" box: input for a bot link/@username,
// then scans every logged-in account and shows the results.
void ShowBotOwnerBox(not_null<Window::SessionController*> controller);

} // namespace Ayu::BotOwner
