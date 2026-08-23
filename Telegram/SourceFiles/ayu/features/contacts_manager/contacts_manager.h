// VanGram: cross-account contacts manager.
//
// Lists contacts of a chosen account (or all accounts), allows bulk
// deletion of selected contacts and "delete everyone except my own
// accounts" (keeps contacts whose user id belongs to any logged-in
// account of this VanGram installation).
#pragma once

class BoxContent;
namespace Ui {
class GenericBox;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Ayu::ContactsManager {

// Opens the contacts manager box (account picker -> contact list with
// checkboxes, search, delete selected / delete all-except-mine).
void ShowContactsManager(not_null<Window::SessionController*> controller);

} // namespace Ayu::ContactsManager
