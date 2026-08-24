// VanGram: 2FA password manager.
//
// Local store: tdata/vangram_passwords.json
//   { "<session uniqueId>": {
//       "password": "...", "label": "...", "notes": "...",
//       "updatedAt": "..." } }
// Plus UI to browse/copy passwords, generate new ones and enable 2FA
// on accounts that don't have it (MTPaccount_UpdatePasswordSettings
// with MTP_inputCheckPasswordEmpty, same as PasscodeBox does).
#pragma once

class BoxContent;

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Ayu::Passwords {

// Opens the password manager box: list of all accounts with stored
// passwords, copy-to-clipboard, add/edit entries, generator.
void ShowPasswordsBox(not_null<Window::SessionController*> controller);

// Opens the "enable 2FA" flow for accounts without a cloud password:
// generated (or typed) password -> MTPaccount_UpdatePasswordSettings ->
// auto-saved into the store.
void ShowEnable2FABox(not_null<Window::SessionController*> controller);

// Generates a random password: letters+digits, length chars.
[[nodiscard]] QString GeneratePassword(int length = 20);

} // namespace Ayu::Passwords
