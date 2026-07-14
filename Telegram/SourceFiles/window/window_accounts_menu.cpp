/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "window/window_accounts_menu.h"

#include "window/window_controller.h"
#include "core/application.h"
#include "main/main_domain.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "data/data_changes.h"
#include "data/data_user.h"
#include "data/data_peer_values.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/userpic_view.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "settings/sections/settings_information.h"
#include "ayu/ui/ayu_userpic.h"
#include "styles/style_window.h"
#include "styles/style_settings.h"

namespace Window {

AccountsMenu::AccountsMenu(
	not_null<Ui::RpWidget*> parent,
	not_null<Controller*> controller,
	Fn<void()> geometryChanged)
: _controller(controller)
, _parent(parent)
, _geometryChanged(std::move(geometryChanged))
, _outer(_parent)
, _scroll(&_outer)
, _container(
	_scroll.setOwnedWidget(
		object_ptr<Ui::VerticalLayout>(&_scroll))) {
	setup();
}

AccountsMenu::~AccountsMenu() = default;

void AccountsMenu::setup() {
	_outer.setAttribute(Qt::WA_OpaquePaintEvent);
	_outer.hide();
	_outer.paintRequest(
	) | rpl::on_next([=](QRect clip) {
		auto p = QPainter(&_outer);
		p.setPen(Qt::NoPen);
		p.setBrush(st::windowAccountsButton.textBg);
		p.drawRect(clip);
	}, _outer.lifetime());

	updateGeometry();

	auto &domain = Core::App().domain();

	// VanGram: drop an account's button the moment its session dies
	// (sessionChanges(null)) — BEFORE ~Session runs — so the button's
	// captured session/user pointers never dangle during a paint. This is
	// what makes full / non-active account logout safe. Mirrors
	// tray_accounts_menu.cpp.
	const auto watchSessions = [=] {
		_sessionsLifetime.destroy();
		for (const auto &account : domain.orderedAccounts()) {
			account->sessionChanges(
			) | rpl::on_next([=](Main::Session*) {
				_buttons.clear();
				refresh();
			}, _sessionsLifetime);
		}
	};

	(rpl::single(rpl::empty)
		| rpl::then(domain.accountsChanges())
	) | rpl::on_next([=] {
		watchSessions();
		refresh();
	}, _outer.lifetime());

	// Rebuild on account switch so the active ring is repainted correctly.
	domain.activeChanges(
	) | rpl::on_next([=](not_null<Main::Account*>) {
		_buttons.clear();
		refresh();
	}, _outer.lifetime());
}

void AccountsMenu::updateGeometry() {
	_parent->heightValue(
	) | rpl::on_next([=](int height) {
		const auto width = st::windowAccountsWidth;
		_outer.setGeometry({ 0, 0, width, height });
		_scroll.setGeometry({ 0, 0, width, height });
		_container->resizeToWidth(width);
		_container->move(0, 0);
	}, _outer.lifetime());
}

void AccountsMenu::setShown(bool shown) {
	if (_shown == shown) {
		return;
	}
	_shown = shown;
	_outer.setVisible(shown);
	if (_geometryChanged) {
		_geometryChanged();
	}
}

void AccountsMenu::refresh() {
	auto &domain = Core::App().domain();
	const auto &accounts = domain.orderedAccounts();

	auto authed = std::vector<not_null<Main::Account*>>();
	for (const auto &account : accounts) {
		if (account->sessionExists()) {
			authed.push_back(account);
		}
	}

	setShown(authed.size() > 1);

	auto now = base::flat_map<
		Main::Account*,
		base::unique_qptr<Ui::SettingsButton>>();
	for (const auto account : authed) {
		auto i = _buttons.find(account);
		if (i != end(_buttons)) {
			now.emplace(account, std::move(i->second));
		} else {
			now.emplace(account, prepareButton(account));
		}
	}
	_buttons = std::move(now);

	_container->resizeToWidth(_outer.width());
}

base::unique_qptr<Ui::SettingsButton> AccountsMenu::prepareButton(
		not_null<Main::Account*> account) {
	if (!_list) {
		_list = _container->add(object_ptr<Ui::VerticalLayout>(_container));
	}
	const auto session = &account->session();
	const auto user = session->user();
	const auto active = (account
		== Core::App().domain().maybeLastOrSomeAuthedAccount());

	auto text = rpl::single(user->name())
		| rpl::then(session->changes().realtimeNameUpdates(user)
			| rpl::map([=] { return user->name(); }));

	auto button = base::unique_qptr<Ui::SettingsButton>(
		_list->add(object_ptr<Ui::SettingsButton>(
			_list,
			std::move(text),
			st::mainMenuAddAccountButton)));
	const auto raw = button.get();

	// Unread badge — reuses the exact same badge style as the main menu.
	{
		const auto compute = [=]() -> Settings::Badge::UnreadBadge {
			const auto s = account->maybeSession();
			const auto d = s ? &s->data() : nullptr;
			return {
				.count = d ? d->unreadBadge() : 0,
				.muted = d ? d->unreadBadgeMuted() : false,
			};
		};
		Settings::Badge::AddUnread(
			raw,
			rpl::single(compute())
				| rpl::then(Core::App().domain().unreadBadgeChanges()
					| rpl::map([=] { return compute(); })));
	}

	struct State {
		explicit State(QWidget *parent) : userpic(parent) {
			userpic.setAttribute(Qt::WA_TransparentForMouseEvents);
		}
		Ui::RpWidget userpic;
		Ui::PeerUserpicView view;
	};
	const auto state = raw->lifetime().make_state<State>(raw);
	state->userpic.show();

	const auto userpicSkip = 2 * st::mainMenuAccountLine + st::lineWidth;
	const auto userpicSize = st::mainMenuAccountSize + userpicSkip * 2;
	raw->heightValue(
	) | rpl::on_next([=](int height) {
		const auto left = st::mainMenuAddAccountButton.iconLeft
			+ (st::settingsIconAdd.width() - userpicSize) / 2;
		const auto top = (height - userpicSize) / 2;
		state->userpic.setGeometry(left, top, userpicSize, userpicSize);
	}, state->userpic.lifetime());

	state->userpic.paintRequest(
	) | rpl::on_next([=] {
		auto p = Painter(&state->userpic);
		const auto size = st::mainMenuAccountSize;
		const auto line = st::mainMenuAccountLine;
		const auto skip = 2 * line + st::lineWidth;
		const auto full = size + skip * 2;
		user->paintUserpicLeft(p, state->view, skip, skip, full, size);
		if (active) {
			const auto shift = st::lineWidth + (line * 0.5);
			const auto diameter = full - 2 * shift;
			const auto rect = QRectF(shift, shift, diameter, diameter);
			auto hq = PainterHighQualityEnabler(p);
			auto pen = st::windowBgActive->p;
			pen.setWidthF(line);
			p.setPen(pen);
			p.setBrush(Qt::NoBrush);
			AyuUserpic::PaintShape(p, rect);
		}
	}, state->userpic.lifetime());

	// Keep userpic in sync with photo changes.
	session->changes().peerFlagsValue(
		user,
		Data::PeerUpdate::Flag::Photo
	) | rpl::on_next([=] {
		state->userpic.update();
	}, state->userpic.lifetime());

	raw->clicks(
	) | rpl::on_next([=](Qt::MouseButton which) {
		if (which == Qt::LeftButton) {
			activate(account, raw->clickModifiers());
		}
	}, raw->lifetime());

	return button;
}

void AccountsMenu::activate(
		not_null<Main::Account*> account,
		Qt::KeyboardModifiers modifiers) {
	auto &domain = Core::App().domain();
	if (account == domain.maybeLastOrSomeAuthedAccount()) {
		return;
	}
	if (modifiers & Qt::ControlModifier) {
		Core::App().ensureSeparateWindowFor(account);
		return;
	}
	if (const auto window = Core::App().separateWindowFor(account)) {
		window->activate();
		return;
	}
	domain.maybeActivate(account);
}

} // namespace Window
