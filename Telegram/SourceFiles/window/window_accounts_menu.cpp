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
#include "ui/wrap/vertical_layout.h"
#include "ui/userpic_view.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "styles/style_window.h"
#include "styles/style_widgets.h"

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
	(rpl::single(rpl::empty)
		| rpl::then(domain.accountsChanges())
	) | rpl::on_next([=] {
		refresh();
	}, _outer.lifetime());

	domain.activeChanges(
	) | rpl::on_next([=](not_null<Main::Account*>) {
		refreshActive();
	}, _outer.lifetime());

	domain.unreadBadgeChanges(
	) | rpl::on_next([=] {
		refreshBadges();
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

	auto now = base::flat_map<Main::Account*, base::unique_qptr<Ui::SideBarButton>>();
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

	refreshActive();
	refreshBadges();
}

void AccountsMenu::refreshActive() {
	const auto active = Core::App().domain().maybeLastOrSomeAuthedAccount();
	for (const auto &[account, button] : _buttons) {
		button->setActive(account == active);
	}
}

void AccountsMenu::refreshBadges() {
	for (const auto &[account, button] : _buttons) {
		const auto session = account->maybeSession();
		const auto data = session ? &session->data() : nullptr;
		const auto count = data ? data->unreadBadge() : 0;
		const auto muted = data ? data->unreadBadgeMuted() : false;
		button->setBadge(
			!count ? QString() : (count > 999 ? u"99+"_q : QString::number(count)),
			muted);
	}
}

base::unique_qptr<Ui::SideBarButton> AccountsMenu::prepareButton(
		not_null<Main::Account*> account) {
	if (!_list) {
		_list = _container->add(object_ptr<Ui::VerticalLayout>(_container));
	}
	const auto session = &account->session();
	const auto user = session->user();

	auto prepared = object_ptr<Ui::SideBarButton>(
		_list,
		TextWithEntities{ user->name() },
		st::windowAccountsButton);
	auto added = _list->add(std::move(prepared));
	auto button = base::unique_qptr<Ui::SideBarButton>(std::move(added));
	const auto raw = button.get();

	struct State {
		explicit State(QWidget *parent) : userpic(parent) {
			userpic.setAttribute(Qt::WA_TransparentForMouseEvents);
		}
		Ui::RpWidget userpic;
		Ui::PeerUserpicView view;
	};
	const auto state = raw->lifetime().make_state<State>(raw);
	state->userpic.show();

	const auto size = st::windowAccountsUserpicSize;
	const auto skip = st::windowAccountsUserpicSkip;
	raw->heightValue(
	) | rpl::on_next([=](int height) {
		const auto left = (st::windowAccountsWidth - size) / 2;
		state->userpic.setGeometry(left, skip, size, size);
	}, state->userpic.lifetime());

	state->userpic.paintRequest(
	) | rpl::on_next([=] {
		auto p = Painter(&state->userpic);
		user->paintUserpicLeft(p, state->view, 0, 0, size, size, true);
	}, state->userpic.lifetime());

	// Keep userpic in sync with photo changes.
	session->changes().peerFlagsValue(
		user,
		Data::PeerUpdate::Flag::Photo
	) | rpl::on_next([=] {
		state->userpic.update();
	}, state->userpic.lifetime());

	raw->setClickedCallback([=] {
		activate(account, raw->clickModifiers());
	});

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
