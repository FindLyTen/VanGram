/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/widgets/scroll_area.h"

namespace Ui {
class VerticalLayout;
class SettingsButton;
class VerticalLayoutReorder;
class PopupMenu;
} // namespace Ui

namespace Main {
class Account;
} // namespace Main

namespace Window {

class Controller;
class SessionController;

// VanGram: persistent account-switcher sidebar.
//
// Owned by Window::Controller (NOT SessionController), because
// SessionController is destroyed and recreated on every account switch
// (see Window::Controller::showAccount). This widget must survive switches
// so it can list all accounts and keep stable state.
//
// Rows are rendered exactly like the main-menu (hamburger) account list:
// a Ui::SettingsButton (st::mainMenuAddAccountButton) with a userpic +
// active ring + an unread badge (Settings::Badge::AddUnread).
class AccountsMenu final {
public:
	AccountsMenu(
		not_null<Ui::RpWidget*> parent,
		not_null<Controller*> controller,
		Fn<void()> geometryChanged);
	~AccountsMenu();

	// Whether the sidebar currently takes up horizontal space.
	[[nodiscard]] bool shown() const { return _shown; }

	// VanGram: mass actions box (shared by the sidebar tools block and
	// the Ayu settings page).
	static void ShowMassActionsBox(
		not_null<SessionController*> controller);

	void updateGeometry();

private:
	void setup();
	void refresh();
	void setShown(bool shown);
	void editTagBox(quint64 key);
	void ensureToolsButtons();
	void ensureAddButton();
	void showAccountMenu(
			not_null<Main::Account*> account,
			Qt::KeyboardModifiers modifiers);
	void moveAccount(
		not_null<Main::Account*> account,
		int delta);
	void applyReorder();
	[[nodiscard]] base::unique_qptr<Ui::SettingsButton> prepareButton(
		not_null<Main::Account*> account);
	void activate(
		not_null<Main::Account*> account,
		Qt::KeyboardModifiers modifiers);

	const not_null<Controller*> _controller;
	const not_null<Ui::RpWidget*> _parent;
	Fn<void()> _geometryChanged;
	Ui::RpWidget _outer;
	Ui::ScrollArea _scroll;
	not_null<Ui::VerticalLayout*> _container;
	Ui::VerticalLayout *_list = nullptr;

	base::flat_map<Main::Account*, base::unique_qptr<Ui::SettingsButton>> _buttons;
	Ui::VerticalLayout *_tools = nullptr;
	base::unique_qptr<Ui::SettingsButton> _massActionsButton;
	base::unique_qptr<Ui::SettingsButton> _contactsButton;
	base::unique_qptr<Ui::SettingsButton> _passwordsButton;
	base::unique_qptr<Ui::SettingsButton> _addButton;
	std::unique_ptr<Ui::VerticalLayoutReorder> _reorder;
	int _reordering = 0;
	base::unique_qptr<Ui::PopupMenu> _popupMenu;

	rpl::lifetime _sessionsLifetime;

	bool _shown = false;

};

} // namespace Window
