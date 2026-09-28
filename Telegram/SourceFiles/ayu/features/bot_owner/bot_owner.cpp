// VanGram: find which logged-in account owns a bot. See bot_owner.h.
#include "ayu/features/bot_owner/bot_owner.h"

#include "core/application.h"
#include "main/main_domain.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "apiwrap.h"
#include "window/window_session_controller.h"
#include "ui/layers/box_content.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/text/text_utilities.h"
#include "base/qt/qt_comparison.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_boxes.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QPointer>

namespace Ayu::BotOwner {
namespace {

constexpr auto kBotFatherUsername = "BotFather";
constexpr auto kHistoryLimit = 2000;

struct AccountResult {
	QString name;
	QString phone;
	bool hasBotFather = false; // chat with @BotFather exists
	QStringList ownerProofs; // matching BotFather lines
	bool talkedToBot = false; // dialog with the bot itself
	bool error = false;
};

QString normalizeUsername(QString input) {
	static const auto re = QRegularExpression(
		"(?:t\\.me/|@)([A-Za-z0-9_]{4,})");
	const auto m = re.match(input.trimmed());
	auto out = m.hasMatch() ? m.captured(1) : input.trimmed();
	while (out.startsWith('@')) {
		out.remove(0, 1);
	}
	return out.toLower();
}

// Fetches the BotFather dialog peer for the account, or nullptr.
UserData *botFather(not_null<Main::Session*> session) {
	const auto peer = session->data().peerByUsername(kBotFatherUsername);
	return peer ? peer->asUser() : nullptr;
}

void searchAccount(
		not_null<Main::Session*> session,
		const QString &username,
		Fn<void(AccountResult)> done) {
	const auto bf = botFather(session);
	if (!bf) {
		done(AccountResult{
			.name = session->user()->name(),
			.phone = session->user()->phone(),
			.error = true,
		});
		return;
	}
	session->api().request(MTPmessages_Search(
		MTP_flags(MTPmessages_Search::Flag::f_from_id),
		bf->input(),
		MTP_string(username),
		session->user()->input,
		MTP_inputPeerEmpty(),
		MTP_vector<MTPReaction>(),
		MTP_int(0), // top_msg_id
		MTP_inputMessagesFilterEmpty(),
		MTP_int(0), // min_date
		MTP_int(0), // max_date
		MTP_int(0), // offset_id
		MTP_inputPeerEmpty(), // add_offset
		MTP_int(kHistoryLimit), // limit
		MTP_int(0), // max_id
		MTP_int(0), // min_id
		MTP_long(0) // hash
	)).done([=](const MTPmessages_Messages &result) {
		auto r = AccountResult{
			.name = session->user()->name(),
			.phone = session->user()->phone(),
			.hasBotFather = true,
		};
		const auto messages = result.match(
			[](const MTPDmessages_messages &d) { return &d.vmessages().v; },
			[](const MTPDmessages_messagesSlice &d) { return &d.vmessages().v; },
			[](const MTPDmessages_channelMessages &d) { return &d.vmessages().v; },
			[](const MTPDmessages_messagesNotModified &) {
				return static_cast<const QVector<MTPMessage>*>(nullptr);
			});
		if (messages) {
			for (const auto &message : *messages) {
				message.match([&](const MTPDmessage &data) {
					const auto text = qs(data.vmessage());
					if (text.contains(username, Qt::CaseInsensitive)) {
						r.ownerProofs.push_back(
							(data.is_out()
								? QStringLiteral("me: ")
								: QStringLiteral("BotFather: "))
							+ text.left(120));
					}
				}, [](const MTPDmessageService &) {});
			}
		}
		done(r);
	}).fail([=] {
		done(AccountResult{
			.name = session->user()->name(),
			.phone = session->user()->phone(),
			.error = true,
		});
	}).send();
}

// Whether the account has a dialog with the bot itself.
void checkBotDialog(
		not_null<Main::Session*> session,
		const QString &username,
		Fn<void(bool)> done) {
	const auto bot = session->data().peerByUsername(username);
	if (const auto user = bot ? bot->asUser() : nullptr) {
		session->api().request(MTPmessages_Search(
			MTP_flags(0),
			bot->input(),
			MTP_string(QString()),
			MTP_inputPeerEmpty(),
			MTP_inputPeerEmpty(),
			MTP_vector<MTPReaction>(),
			MTP_int(0),
			MTP_inputMessagesFilterEmpty(),
			MTP_int(0),
			MTP_int(0),
			MTP_int(0),
			MTP_inputPeerEmpty(),
			MTP_int(1),
			MTP_int(0),
			MTP_int(0),
			MTP_long(0)
		)).done([=](const MTPmessages_Messages &result) {
			const auto count = result.match(
				[](const MTPDmessages_messages &d) { return d.vmessages().v.size(); },
				[](const MTPDmessages_messagesSlice &d) { return d.vmessages().v.size(); },
				[](const MTPDmessages_channelMessages &d) { return d.vmessages().v.size(); },
				[](const MTPDmessages_messagesNotModified &) { return int(0); });
			done(count > 0);
		}).fail([=] {
			done(false);
		}).send();
	} else {
		// Peer not in cache — resolve by username first (best-effort,
		// one-shot; failure simply means "no dialog").
		session->api().request(MTPcontacts_ResolveUsername(
			MTP_flags(0),
			MTP_string(username),
			MTP_string()
		)).done([=](const MTPcontacts_ResolvedPeer &result) {
			result.match([&](const MTPDcontacts_resolvedPeer &data) {
				session->data().processUsers(data.vusers());
				session->data().processChats(data.vchats());
				checkBotDialog(session, username, std::move(done));
			});
		}).fail([=] {
			done(false);
		}).send();
	}
}

} // namespace

void ShowBotOwnerBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(QString("Find bot owner")));
		const auto content = box->verticalLayout();

		const auto field = content->add(
			object_ptr<Ui::InputField>(
				content,
				st::defaultInputField,
				rpl::single(QString("@username or t.me/...")),
				QString()),
			st::boxRowPadding);
		field->setMaxLength(64);

		const auto status = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				QString(),
				st::defaultFlatLabel),
			st::boxRowPadding);
		status->setBreakEverywhere(true);

		const auto results = content->add(
			object_ptr<Ui::VerticalLayout>(content),
			st::boxRowPadding);

		box->setMaxHeight(520);
		box->addButton(rpl::single(QString("Search")), [=] {
			const auto username = normalizeUsername(field->getLastText());
			results->clear();
			if (username.size() < 4) {
				status->setText(
					QStringLiteral("Enter a valid bot username"));
				return;
			}
			status->setText(QStringLiteral("Searching..."));

			struct State {
				int total = 0;
				int done = 0;
				int owners = 0;
				int talked = 0;
			};
			const auto state = std::make_shared<State>();

			const auto accounts = [&] {
				auto out = std::vector<not_null<Main::Account*>>();
				if (!Core::IsAppLaunched()
					|| !Core::App().domain().started()) {
					return out;
				}
				for (const auto &[index, account]
					: Core::App().domain().accounts()) {
					if (account->maybeSession()) {
						out.push_back(account.get());
					}
				}
				return out;
			}();
			state->total = int(accounts.size());
			if (accounts.empty()) {
				status->setText(QStringLiteral("No logged-in accounts"));
				return;
			}

			const auto finishOne = [=] {
				++state->done;
				if (state->done == state->total) {
					status->setText(QStringLiteral(
						"Scanned %1 accounts: %2 owner(s), %3 talked to the bot")
							.arg(state->total)
							.arg(state->owners)
							.arg(state->talked));
				}
			};

			for (const auto account : accounts) {
				const auto session = account->maybeSession();
				searchAccount(session, username, [=](AccountResult r) {
					const auto weak = QPointer<Ui::GenericBox>(box);
					const auto addRow = [=](const QString &line, bool owner) {
						if (!weak) {
							return;
						}
						const auto row = results->add(
							object_ptr<Ui::FlatLabel>(
								results,
								line,
								st::defaultFlatLabel),
							style::margins());
						row->setBreakEverywhere(true);
						if (owner) {
							row->setTextColorOverride(
								st::windowBgActive->c);
						}
					};
					if (r.error) {
						addRow(QStringLiteral("%1: no BotFather chat")
							.arg(r.name), false);
					} else if (!r.ownerProofs.isEmpty()) {
						++state->owners;
						addRow(QStringLiteral("OWNER: %1").arg(r.name), true);
						for (const auto &proof : r.ownerProofs) {
							addRow(
								QStringLiteral("  ") + proof,
								true);
						}
					}
					checkBotDialog(session, username, [=](bool talked) {
						if (talked) {
							++state->talked;
							addRow(QStringLiteral("%1: talked to the bot")
								.arg(r.name), false);
						}
						finishOne();
					});
				});
			}
		});
		box->addButton(rpl::single(QString("Close")), [=] {
			box->closeBox();
		});
	}));
}

} // namespace Ayu::BotOwner
