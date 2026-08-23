// VanGram: cross-account contacts manager.
#include "ayu/features/contacts_manager/contacts_manager.h"

#include "core/application.h"
#include "main/main_domain.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "dialogs/dialogs_list.h"
#include "apiwrap.h"
#include "window/window_session_controller.h"
#include "ui/layers/box_content.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/scroll_area.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/text/text_utilities.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_boxes.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QSet>

#include <algorithm>

namespace Ayu::ContactsManager {
namespace {

constexpr auto kDeleteChunk = 25;

struct ContactItem {
	not_null<Main::Session*> session;
	not_null<UserData*> user;
	QString name;
	QString detail; // @username or +phone
	bool mine = false; // one of my own logged-in accounts
};

std::vector<not_null<Main::Session*>> authedSessions() {
	std::vector<not_null<Main::Session*>> result;
	if (!Core::IsAppLaunched() || !Core::App().domain().started()) {
		return result;
	}
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (const auto session = account->maybeSession()) {
			result.push_back(session);
		}
	}
	return result;
}

QSet<PeerId> myAccountPeerIds() {
	auto ids = QSet<PeerId>();
	for (const auto session : authedSessions()) {
		ids.insert(session->user()->id);
	}
	return ids;
}

std::vector<ContactItem> contactsOf(
		not_null<Main::Session*> session,
		const QSet<PeerId> &mine) {
	std::vector<ContactItem> result;
	const auto list = session->data().contactsList();
	for (const auto &row : list->all()) {
		const auto history = row->history();
		if (!history) {
			continue;
		}
		const auto user = history->peer->asUser();
		if (!user || !user->isContact()) {
			continue;
		}
		auto detail = user->username().isEmpty()
			? (user->phone().isEmpty()
				? QString()
				: QStringLiteral("+") + user->phone())
			: QStringLiteral("@") + user->username();
		result.push_back({
			session,
			user,
			user->name(),
			std::move(detail),
			mine.contains(user->id),
		});
	}
	return result;
}

void deleteContacts(
		not_null<Main::Session*> session,
		std::vector<not_null<UserData*>> users) {
	for (auto from = begin(users); from != end(users);) {
		const auto to = std::min(from + kDeleteChunk, end(users));
		auto inputs = std::vector<MTPInputUser>();
		inputs.reserve(to - from);
		for (auto it = from; it != to; ++it) {
			inputs.push_back((*it)->inputUser());
		}
		session->api().request(MTPcontacts_DeleteContacts(
			MTP_vector<MTPInputUser>(std::move(inputs))
		)).done([=](const MTPUpdates &result) {
			session->api().applyUpdates(result);
		}).send();
		from = to;
	}
	// Refresh the local contact list right away.
	session->data().contactsLoaded() = false;
	session->api().requestContacts();
}

void deleteAllExceptMineAllAccounts() {
	const auto mine = myAccountPeerIds();
	for (const auto session : authedSessions()) {
		auto victims = std::vector<not_null<UserData*>>();
		for (const auto &item : contactsOf(session, mine)) {
			if (!item.mine) {
				victims.push_back(item.user);
			}
		}
		if (!victims.empty()) {
			deleteContacts(session, std::move(victims));
		}
	}
}

void showListBox(
	not_null<Window::SessionController*> controller,
	not_null<Main::Session*> session) {
	const auto mine = myAccountPeerIds();
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(QString("Contacts")));
		const auto content = box->verticalLayout();

		const auto info = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				QString(),
				st::defaultFlatLabel),
			st::boxRowPadding);

		// Checkboxes for every contact (mine shown but disabled).
		struct Row {
			Ui::Checkbox *check = nullptr;
			ContactItem item;
		};
		const auto rows = std::make_shared<std::vector<Row>>();
		const auto checkedIds = std::make_shared<QSet<PeerId>>();

		const auto listWrap = content->add(
			object_ptr<Ui::ScrollArea>(content),
			st::boxRowPadding);
		const auto list = Ui::CreateChild<Ui::VerticalLayout>(
			listWrap.get());
		listWrap->setOwnedWidget(object_ptr<Ui::VerticalLayout>(list));
		// Fixed height, scrollable inside.
		listWrap->setMaximumHeight(320);

		const auto rebuild = [=] {
			// Destroys all previously added checkbox widgets.
			list->clear();
			auto items = contactsOf(session, mine);
			std::sort(
				begin(items),
				end(items),
				[](const ContactItem &a, const ContactItem &b) {
					return a.name.compare(
						b.name,
						Qt::CaseInsensitive) < 0;
				});
			rows->clear();
			checkedIds->clear();
			for (const auto &item : items) {
				auto label = item.name;
				if (!item.detail.isEmpty()) {
					label += QStringLiteral("  ") + item.detail;
				}
				if (item.mine) {
					label += QStringLiteral("  [my account]");
				}
				const auto check = list->add(
					object_ptr<Ui::Checkbox>(
						list,
						label,
						false,
						st::defaultBoxCheckbox),
					st::boxRowPadding);
				check->setDisabled(item.mine);
				check->checkedChanges(
				) | rpl::on_next([=](bool checked) {
					if (checked) {
						checkedIds->insert(item.user->id);
					} else {
						checkedIds->remove(item.user->id);
					}
				}, check->lifetime());
				rows->push_back({ check, item });
			}
			info->setText(QStringLiteral("Contacts: %1").arg(
				rows->size()));
			list->resizeToWidth(st::boxWidth);
		};
		rebuild();

		box->addButton(rpl::single(QString("Delete selected")), [=] {
			auto victims = std::vector<not_null<UserData*>>();
			for (const auto &row : *rows) {
				if (checkedIds->contains(row.item.user->id)
					&& !row.item.mine) {
					victims.push_back(row.item.user);
				}
			}
			if (!victims.empty()) {
				deleteContacts(session, std::move(victims));
			}
			box->closeBox();
		});
		box->addButton(rpl::single(QString("Delete all except mine")), [=] {
			deleteAllExceptMineAllAccounts();
			box->closeBox();
		});
		box->addButton(rpl::single(QString("Close")), [=] {
			box->closeBox();
		});
	}));
}

} // namespace

void ShowContactsManager(
		not_null<Window::SessionController*> controller) {
	const auto sessions = authedSessions();
	if (sessions.empty()) {
		return;
	}
	// TODO: account picker when more than one session exists; for now
	// open the active session's list (the all-accounts action is inside).
	showListBox(controller, &controller->session());
}

} // namespace Ayu::ContactsManager
