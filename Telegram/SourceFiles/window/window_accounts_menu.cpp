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
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/menu/menu_add_action_callback_factory.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/layers/box_content.h"
#include "ui/layers/generic_box.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/vertical_layout_reorder.h"
#include "ui/userpic_view.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "settings/sections/settings_information.h"
#include "ayu/ui/ayu_userpic.h"
#include "ayu/features/passwords/passwords.h"
#include "ayu/ayu_tag_sync.h"
#include "core/core_settings.h"
#include "styles/style_window.h"
#include "styles/style_settings.h"
#include "styles/style_menu_icons.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMap>

#include <algorithm>

namespace Window {

namespace {

QString tagFilePath() {
	return QCoreApplication::applicationDirPath()
		+ QStringLiteral("/tdata/vangram_tags.json");
}

struct TagInfo {
	QString text;
	QString color; // hex (e.g. "#2aabee"); empty = default accent
	qint64 updated_at = 0;
	bool deleted = false;
};

QMap<quint64, TagInfo> g_tagsMap;
bool g_tagsLoaded = false;
Fn<void()> loadedReset;

QMap<quint64, TagInfo> &tagsMap() {
	if (!g_tagsLoaded) {
		g_tagsLoaded = true;
		loadedReset = [] { g_tagsMap.clear(); g_tagsLoaded = false; };
		QFile f(tagFilePath());
		if (f.open(QIODevice::ReadOnly)) {
			const auto obj = QJsonDocument::fromJson(f.readAll()).object();
			for (auto it = obj.begin(); it != obj.end(); ++it) {
				const auto v = it.value();
				if (v.isObject()) {
					const auto o = v.toObject();
					g_tagsMap[it.key().toULongLong()] = {
						o.value(QStringLiteral("text")).toString(),
						o.value(QStringLiteral("color")).toString(),
						o.value(QStringLiteral("updated_at"))
							.toVariant().toLongLong(),
						o.value(QStringLiteral("deleted")).toBool(),
					};
				} else {
					// legacy format (plain string) from older builds
					g_tagsMap[it.key().toULongLong()] = { v.toString(), QString(), 0, false };
				}
			}
		}
	}
	return g_tagsMap;
}

void reloadTagsFromDisk() {
	// drop the cached map so the next tagsMap() call re-reads the file
	// (used after a successful Pull from another device)
	if (loadedReset) {
		loadedReset();
	}
}

void saveTags() {
	QJsonObject obj;
	for (auto it = tagsMap().constBegin(); it != tagsMap().constEnd(); ++it) {
		QJsonObject o;
		o[QStringLiteral("text")] = it.value().text;
		o[QStringLiteral("color")] = it.value().color;
		o[QStringLiteral("updated_at")] = double(it.value().updated_at);
		o[QStringLiteral("deleted")] = it.value().deleted;
		obj[QString::number(it.key())] = o;
	}
	QDir().mkpath(
		QCoreApplication::applicationDirPath() + QStringLiteral("/tdata"));
	QFile f(tagFilePath());
	if (f.open(QIODevice::WriteOnly)) {
		f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
	}
	// VanGram: sync tags to other devices (best-effort, non-blocking).
	Ayu::TagsSync::Push(obj);
}

QString tagValue(quint64 key) {
	const auto &info = tagsMap().value(key);
	return info.deleted ? QString() : info.text;
}

QString tagColor(quint64 key) {
	const auto &info = tagsMap().value(key);
	return info.deleted ? QString() : info.color;
}

void setTagValue(quint64 key, const QString &text, const QString &color) {
	if (text.isEmpty()) {
		// deleting: keep a tombstone so the deletion syncs to other devices
		auto t = TagInfo{
			QString(),
			QString(),
			QDateTime::currentSecsSinceEpoch(),
			true,
		};
		tagsMap()[key] = t;
	} else {
		tagsMap()[key] = {
			text,
			color,
			QDateTime::currentSecsSinceEpoch(),
			false,
		};
	}
}

} // namespace

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

	// VanGram: tear down subscriptions/buttons early at quit, before the
	// Domain/accounts are destroyed, to avoid use-after-free during shutdown
	// (crtisvalidheappointer / debug_heap assertion on exit).
	QObject::connect(
		QCoreApplication::instance(),
		&QCoreApplication::aboutToQuit,
		&_outer,
		[=] {
			_buttons.clear();
			_sessionsLifetime.destroy();
		});

	auto &domain = Core::App().domain();

	// VanGram: drop an account's button the moment its session dies
	// (sessionChanges(null)) — BEFORE ~Session runs — so the button's
	// captured session/user pointers never dangle during a paint. This is
	// what makes full / non-active account logout safe. Mirrors
	// tray_accounts_menu.cpp.
	const auto watchSessions = [=] {
		_sessionsLifetime.destroy();
		auto &d = Core::App().domain();
		for (const auto &entry : d.orderedAccounts()) {
			const auto account = entry.get();
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
		// VanGram: rebuild resets the scroll position (resizeToWidth),
		// so restore it — otherwise switching accounts jumps to top.
		const auto oldTop = _scroll.scrollTop();
		_buttons.clear();
		refresh();
		_scroll.scrollToY(oldTop);
	}, _outer.lifetime());

	// VanGram: pull tags from other devices on startup (one-shot) and
	// rebuild the sidebar if anything actually changed.
	Ayu::TagsSync::Pull([=](QJsonObject) {
		crl::on_main(&_outer, [=] {
			// tags file changed on disk -> reload the in-memory map
			// (the static map is only loaded once, so rebuild via clear)
			reloadTagsFromDisk();
			_buttons.clear();
			refresh();
		});
	});
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

	// VanGram: rebuilding resets the scroll position, restore it
	// (same approach as FiltersMenu::refresh).
	const auto oldTop = _scroll.scrollTop();

	if (_reorder) {
		_reorder->cancel();
	}

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

	ensureAddButton();
	ensurePasswordsButton();

	_container->resizeToWidth(_outer.width());

	// VanGram: drag-n-drop account reorder. Same approach as the accounts
	// list in settings (Settings::AccountsList::rebuild).
	if (_list) {
		if (!_reorder) {
			_reorder = std::make_unique<Ui::VerticalLayoutReorder>(
				_list,
				&_scroll);
			_reorder->updates(
			) | rpl::on_next([=](Ui::VerticalLayoutReorder::Single data) {
				using State = Ui::VerticalLayoutReorder::State;
				if (data.state == State::Started) {
					++_reordering;
				} else {
					Ui::PostponeCall(_list, [=] {
						--_reordering;
					});
					if (data.state == State::Applied) {
						applyReorder();
					}
				}
			}, _list->lifetime());
		}
		_reorder->start();
	}

	_scroll.scrollToY(oldTop);
}

void AccountsMenu::applyReorder() {
	if (!_list) {
		return;
	}
	std::vector<uint64> order;
	order.reserve(_buttons.size());
	for (auto i = 0; i < _list->count(); i++) {
		const auto widget = _list->widgetAt(i);
		for (const auto &[account, button] : _buttons) {
			if (button.get() == widget) {
				order.push_back(account->session().uniqueId());
			}
		}
	}
	if (order.size() == _buttons.size()) {
		Core::App().settings().setAccountsOrder(order);
		Core::App().saveSettings();
	}
}

void AccountsMenu::moveAccount(
		not_null<Main::Account*> account,
		int delta) {
	if (!delta || !_list) {
		return;
	}
	// Build the full uniqueId order the same way Domain::orderedAccounts()
	// consumes it. Accounts without a session have no uniqueId — skip them.
	std::vector<uint64> order;
	std::vector<not_null<Main::Account*>> authed;
	for (const auto &entry : Core::App().domain().orderedAccounts()) {
		if (entry->sessionExists()) {
			order.push_back(entry->session().uniqueId());
			authed.push_back(entry.get());
		}
	}
	const auto wanted = account->session().uniqueId();
	const auto it = ranges::find(order, wanted);
	if (it == end(order)) {
		return;
	}
	const auto oldIndex = int(it - begin(order));
	const auto newIndex = std::clamp(
		oldIndex + delta,
		0,
		int(order.size()) - 1);
	if (newIndex == oldIndex) {
		return;
	}
	if (newIndex < oldIndex) {
		order.insert(begin(order) + newIndex, wanted);
		order.erase(begin(order) + oldIndex + 1);
	} else {
		order.insert(begin(order) + newIndex + 1, wanted);
		order.erase(begin(order) + oldIndex);
	}
	Core::App().settings().setAccountsOrder(order);
	Core::App().saveSettings();
	_buttons.clear();
	refresh();
}

void AccountsMenu::showAccountMenu(
		not_null<Main::Account*> account,
		Qt::KeyboardModifiers modifiers) {
	if (modifiers & Qt::ControlModifier) {
		activate(account, modifiers);
		return;
	}
	const auto i = _buttons.find(account);
	if (i == end(_buttons)) {
		return;
	}
	_popupMenu = nullptr;
	_popupMenu = base::make_unique_q<Ui::PopupMenu>(
		i->second.get(),
		st::popupMenuWithIcons);
	const auto addAction = Ui::Menu::CreateAddActionCallback(_popupMenu);
	addAction(Ui::Menu::MenuCallback::Args{
		.text = QString("Move up"),
		.handler = crl::guard(&_outer, [=] { moveAccount(account, -1); }),
	});
	addAction(Ui::Menu::MenuCallback::Args{
		.text = QString("Move down"),
		.handler = crl::guard(&_outer, [=] { moveAccount(account, 1); }),
	});
	addAction(Ui::Menu::MenuCallback::Args{
		.text = QString("Edit tag"),
		.handler = crl::guard(&_outer, [=] { editTagBox(account->session().uniqueId()); }),
		.icon = &st::menuIconEdit,
	});
	_popupMenu->popup(QCursor::pos());
}

base::unique_qptr<Ui::SettingsButton> AccountsMenu::prepareButton(
		not_null<Main::Account*> account) {
	if (!_list) {
		_list = _container->add(object_ptr<Ui::VerticalLayout>(_container));
	}
	const auto session = &account->session();
	const auto user = session->user();
	const auto key = session->uniqueId();
	const auto active = (account == &Core::App().domain().active());

	auto text = rpl::single(user->name())
		| rpl::then(session->changes().realtimeNameUpdates(user)
			| rpl::map([=] { return user->name(); }));

	auto button = base::unique_qptr<Ui::SettingsButton>(
		_list->add(object_ptr<Ui::SettingsButton>(
			_list,
			std::move(text),
			active ? st::windowAccountsActiveButton : st::mainMenuAddAccountButton)));
	const auto raw = button.get();

	// VanGram: account tag (right side). Replaces the unread badge — the
	// tag ("what this account is for") is more useful in this sidebar.
	const auto tagHolder = Settings::Badge::AddRight(raw, 0);
	tagHolder->show();
	const auto tagLabel = Ui::CreateChild<Ui::FlatLabel>(
		tagHolder,
		rpl::single(tagValue(key)),
		st::defaultFlatLabel);
	const auto tcolor = tagColor(key);
	tagLabel->setTextColorOverride(
		tcolor.isEmpty() ? st::windowBgActive->c : QColor(tcolor));
	tagLabel->setElisionMiddle(true);
	tagLabel->setMaximumWidth(150);
	tagLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	tagLabel->show();
	tagLabel->sizeValue(
	) | rpl::on_next([=](const QSize &s) {
		tagHolder->resize(s);
	}, tagHolder->lifetime());


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

	raw->setAcceptBoth(true);
	raw->clicks(
	) | rpl::on_next([=](Qt::MouseButton which) {
		if (which == Qt::LeftButton) {
			if (_reordering) {
				return;
			}
			activate(account, raw->clickModifiers());
		} else if (which == Qt::MiddleButton) {
			activate(account, Qt::ControlModifier);
		} else if (which == Qt::RightButton) {
			showAccountMenu(account, raw->clickModifiers());
		}
	}, raw->lifetime());

	return button;
}

void AccountsMenu::editTagBox(quint64 key) {
	_controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(QString("Account tag")));
		const auto field = box->addRow(
			object_ptr<Ui::InputField>(
				box,
				st::defaultInputField,
				rpl::single(QString("Short description of the account")),
				tagValue(key)),
			st::boxRowPadding);
		field->setMaxLength(40);

		// Color picker: a button that cycles through preset colors.
		const auto presets = std::make_shared<
			std::vector<std::pair<QString, QString>>>();
		*presets = {
			{ QStringLiteral("Color: Default"), QString() },
			{ QStringLiteral("Color: Blue"), QStringLiteral("#2aabee") },
			{ QStringLiteral("Color: Red"), QStringLiteral("#e74c3c") },
			{ QStringLiteral("Color: Orange"), QStringLiteral("#e67e22") },
			{ QStringLiteral("Color: Green"), QStringLiteral("#2ecc71") },
			{ QStringLiteral("Color: Purple"), QStringLiteral("#9b59b6") },
		};
		const auto chosen = std::make_shared<QString>(tagColor(key));
		const auto idx = std::make_shared<int>(0);
		for (int i = 0; i < int(presets->size()); ++i) {
			if ((*presets)[i].second == *chosen) {
				*idx = i;
				break;
			}
		}
		const auto colorBtn = box->addRow(
			object_ptr<Ui::RoundButton>(
				box,
				rpl::single((*presets)[*idx].first),
				st::defaultLightButton),
			st::boxRowPadding);
		const auto refreshBtn = [=] {
			const auto &p = (*presets)[*idx];
			colorBtn->setText(rpl::single(p.first));
			colorBtn->setTextFgOverride(
				p.second.isEmpty()
					? st::windowBgActive->c
					: QColor(p.second));
		};
		refreshBtn();
		colorBtn->setClickedCallback([=] {
			*idx = (*idx + 1) % int(presets->size());
			*chosen = (*presets)[*idx].second;
			refreshBtn();
		});

		box->addButton(rpl::single(QString("Save")), [=] {
			setTagValue(key, field->getLastText().trimmed(), *chosen);
			saveTags();
			box->closeBox();
			_buttons.clear();
			refresh();
		});
		box->addButton(rpl::single(QString("Cancel")), [=] { box->closeBox(); });
	}));
}

void AccountsMenu::activate(
		not_null<Main::Account*> account,
		Qt::KeyboardModifiers modifiers) {
	auto &domain = Core::App().domain();
	if (account == &domain.active()) {
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

void AccountsMenu::ensureAddButton() {
	if (_addButton) {
		return;
	}
	auto button = base::unique_qptr<Ui::SettingsButton>(
		_container->add(object_ptr<Ui::SettingsButton>(
			_container,
			rpl::single(QString("Add account")),
			st::mainMenuAddAccountButton)));
	const auto raw = button.get();

	// Green "+" icon in the userpic slot (same look as the main menu).
	struct IconState {
		explicit IconState(QWidget *parent) : w(parent) {
			w.setAttribute(Qt::WA_TransparentForMouseEvents);
		}
		Ui::RpWidget w;
	};
	const auto st = raw->lifetime().make_state<IconState>(raw);
	st->w.show();
	const auto iconSize = st::settingsIconAdd.width();
	raw->heightValue(
	) | rpl::on_next([=](int height) {
		const auto left = st::mainMenuAddAccountButton.iconLeft;
		const auto top = (height - iconSize) / 2;
		st->w.setGeometry(left, top, iconSize, iconSize);
	}, st->w.lifetime());
	st->w.paintRequest(
	) | rpl::on_next([=] {
		auto p = QPainter(&st->w);
		st::settingsIconAdd.paint(p, 0, 0, st->w.width());
	}, st->w.lifetime());

	raw->clicks(
	) | rpl::on_next([=](Qt::MouseButton which) {
		if (which == Qt::LeftButton) {
			Core::App().domain().addActivated(MTP::Environment{});
		}
	}, raw->lifetime());

	_addButton = std::move(button);
}

void AccountsMenu::ensurePasswordsButton() {
	// VanGram: 2FA password manager entry at the bottom of the sidebar.
	if (_passwordsButton) {
		return;
	}
	auto button = base::unique_qptr<Ui::SettingsButton>(
		_container->add(object_ptr<Ui::SettingsButton>(
			_container,
			rpl::single(QString("2FA Passwords")),
			st::mainMenuAddAccountButton)));
	const auto raw = button.get();

	// Key icon in the userpic slot.
	struct IconState {
		explicit IconState(QWidget *parent) : w(parent) {
			w.setAttribute(Qt::WA_TransparentForMouseEvents);
		}
		Ui::RpWidget w;
	};
	const auto st = raw->lifetime().make_state<IconState>(raw);
	st->w.show();
	const auto iconSize = st::settingsIconAdd.width();
	raw->heightValue(
	) | rpl::on_next([=](int height) {
		const auto left = st::mainMenuAddAccountButton.iconLeft;
		const auto top = (height - iconSize) / 2;
		st->w.setGeometry(left, top, iconSize, iconSize);
	}, st->w.lifetime());
	st->w.paintRequest(
	) | rpl::on_next([=] {
		auto p = QPainter(&st->w);
		st::menuIcon2SV.paint(p, 0, 0, st->w.width());
	}, st->w.lifetime());

	raw->clicks(
	) | rpl::on_next([=](Qt::MouseButton which) {
		if (which == Qt::LeftButton) {
			const auto window = Core::App().activePrimaryWindow();
			if (window && window->sessionController()) {
				Ayu::Passwords::ShowPasswordsBox(
					window->sessionController());
			}
		}
	}, raw->lifetime());

	_passwordsButton = std::move(button);
}

} // namespace Window
