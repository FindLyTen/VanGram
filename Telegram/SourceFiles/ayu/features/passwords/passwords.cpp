// VanGram: 2FA password manager.
#include "ayu/features/passwords/passwords.h"

#include "core/application.h"
#include "core/core_cloud_password.h"
#include "main/main_domain.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "data/data_user.h"
#include "mtproto/mtproto_dc_options.h"
#include "apiwrap.h"
#include "window/window_session_controller.h"
#include "ui/layers/box_content.h"
#include "ui/layers/generic_box.h"
#include "ui/toast/toast.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/scroll_area.h"
#include "ui/wrap/vertical_layout.h"
#include "base/random.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_boxes.h"
#include "styles/style_window.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtGui/QGuiApplication>

#include <algorithm>
#include <random>

namespace Ayu::Passwords {
namespace {

struct Entry {
	QString password;
	QString label;
	QString notes;
	QString updatedAt;
};

QString storePath() {
	return QCoreApplication::applicationDirPath()
		+ QStringLiteral("/tdata/vangram_passwords.json");
}

QMap<quint64, Entry> &store() {
	static QMap<quint64, Entry> map;
	static bool loaded = false;
	if (!loaded) {
		loaded = true;
		QFile f(storePath());
		if (f.open(QIODevice::ReadOnly)) {
			const auto obj = QJsonDocument::fromJson(f.readAll()).object();
			for (auto it = obj.begin(); it != obj.end(); ++it) {
				if (it.value().isObject()) {
					const auto o = it.value().toObject();
					map[it.key().toULongLong()] = {
						o.value(QStringLiteral("password")).toString(),
						o.value(QStringLiteral("label")).toString(),
						o.value(QStringLiteral("notes")).toString(),
						o.value(QStringLiteral("updatedAt")).toString(),
					};
				}
			}
		}
	}
	return map;
}

void saveStore() {
	QJsonObject obj;
	for (auto it = store().constBegin(); it != store().constEnd(); ++it) {
		QJsonObject o;
		o[QStringLiteral("password")] = it.value().password;
		o[QStringLiteral("label")] = it.value().label;
		o[QStringLiteral("notes")] = it.value().notes;
		o[QStringLiteral("updatedAt")] = it.value().updatedAt;
		obj[QString::number(it.key())] = o;
	}
	QDir().mkpath(
		QCoreApplication::applicationDirPath() + QStringLiteral("/tdata"));
	QFile f(storePath());
	if (f.open(QIODevice::WriteOnly)) {
		f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
	}
}

void savePassword(
		not_null<Main::Session*> session,
		const QString &password,
		const QString &label) {
	store()[session->uniqueId()] = {
		password,
		label.isEmpty() ? session->user()->name() : label,
		QString(),
		QDateTime::currentDateTime().toString(Qt::ISODate),
	};
	saveStore();
}

// Enables 2FA on an account that has no cloud password yet.
// Mirrors PasscodeBox::setNewCloudPassword (empty check).
void enable2FA(
		not_null<Main::Session*> session,
		const QString &password,
		const QString &label,
		Fn<void(bool, const QString &)> done) {
	session->api().request(MTPaccount_GetPassword(
	)).done([=](const MTPaccount_Password &result) {
		auto algo = Core::CloudPasswordAlgo();
		result.match([&](const MTPDaccount_password &data) {
			algo = Core::ParseCloudPasswordAlgo(data.vnew_algo());
		});
		if (v::is_null(algo)) {
			done(false, QStringLiteral("unsupported password algorithm"));
			return;
		}
		const auto hash = Core::ComputeCloudPasswordDigest(
			algo,
			bytes::make_span(password.toUtf8()));
		if (hash.modpow.empty()) {
			done(false, QStringLiteral("digest computation failed"));
			return;
		}
		using Flag = MTPDaccount_passwordInputSettings::Flag;
		const auto settings = MTP_account_passwordInputSettings(
			MTP_flags(Flag::f_new_algo
				| Flag::f_new_password_hash
				| Flag::f_hint),
			Core::PrepareCloudPasswordAlgo(algo),
			MTP_bytes(hash.modpow),
			MTP_string(QString()), // hint
			MTP_string(QString()), // email (none — temp numbers)
			MTPSecureSecretSettings());
		session->api().request(MTPaccount_UpdatePasswordSettings(
			MTP_inputCheckPasswordEmpty(),
			settings
		)).done([=] {
			savePassword(session, password, label);
			done(true, QString());
		}).fail([=](const MTP::Error &error) {
			done(false, error.type());
		}).handleFloodErrors().send();
	}).fail([=](const MTP::Error &error) {
		done(false, error.type());
	}).send();
}

struct AccountInfo {
	not_null<Main::Session*> session;
	QString name;
	quint64 uniqueId = 0;
	bool hasStored = false;
};

std::vector<AccountInfo> accountInfos() {
	std::vector<AccountInfo> result;
	if (!Core::IsAppLaunched() || !Core::App().domain().started()) {
		return result;
	}
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (const auto session = account->maybeSession()) {
			result.push_back({
				session,
				session->user()->name(),
				session->uniqueId(),
				store().contains(session->uniqueId()),
			});
		}
	}
	return result;
}

} // namespace

QString GeneratePassword(int length) {
	const auto chars = QStringLiteral(
		"abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789");
	auto rng = std::mt19937(std::random_device()());
	std::uniform_int_distribution<int> dist(0, chars.size() - 1);
	QString out;
	for (auto i = 0; i < length; ++i) {
		out += chars[dist(rng)];
	}
	return out;
}

void ShowPasswordsBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(QString("2FA Passwords")));
		const auto content = box->verticalLayout();

		const auto info = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				QString(),
				st::defaultFlatLabel),
			st::boxRowPadding);

		const auto list = content;

		const auto rebuild = [=] {
			auto accounts = accountInfos();
			auto saved = 0;
			for (const auto &acc : accounts) {
				const auto it = store().constFind(acc.uniqueId);
				const auto has = (it != store().constEnd());
				if (has) {
					++saved;
				}
				const auto label = acc.name
					+ (has
						? QStringLiteral("  ✓")
						: QStringLiteral("  — no password"));
				const auto button = list->add(
					object_ptr<Ui::SettingsButton>(
						list,
						rpl::single(label),
						st::mainMenuAddAccountButton),
					style::margins());
				const auto pass = has ? it.value().password : QString();
				const auto session = acc.session;
				button->addClickHandler([=] {
					if (pass.isEmpty()) {
						return;
					}
					QGuiApplication::clipboard()->setText(pass);
					Ui::Toast::Show(QStringLiteral("Password copied"));
				});
			}
			info->setText(QStringLiteral(
				"Saved: %1 / %2 accounts. Click a row to copy.")
				.arg(saved)
				.arg(accounts.size()));
		};
		rebuild();

		box->addButton(rpl::single(QString("Close")), [=] {
			box->closeBox();
		});
	}));
}

void ShowEnable2FABox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(QString("Enable 2FA")));
		const auto content = box->verticalLayout();

		const auto accounts = accountInfos();
		auto without = std::vector<AccountInfo>();
		for (const auto &acc : accounts) {
			if (!acc.hasStored) {
				without.push_back(acc);
			}
		}

		const auto status = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				QStringLiteral("Accounts without stored 2FA: %1")
					.arg(without.size()),
				st::defaultFlatLabel),
			st::boxRowPadding);

		const auto passLabel = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				QString(),
				st::defaultFlatLabel),
			st::boxRowPadding);

		const auto pass = std::make_shared<QString>(
			GeneratePassword(20));
		passLabel->setText(QStringLiteral("Password: %1").arg(*pass));

		const auto log = content->add(
			object_ptr<Ui::FlatLabel>(
				content,
				QStringLiteral("idle"),
				st::defaultFlatLabel),
			st::boxRowPadding);
		log->setWordWrap(true);

		content->add(
			object_ptr<Ui::RoundButton>(
				content,
				rpl::single(QString("Regenerate password")),
				st::defaultLightButton),
			st::boxRowPadding
		)->addClickHandler([=] {
			*pass = GeneratePassword(20);
			passLabel->setText(
				QStringLiteral("Password: %1").arg(*pass));
		});

		box->addButton(rpl::single(QString("Enable on all")), [=] {
			log->setText(QStringLiteral("starting..."));
			const auto queue = std::make_shared<std::vector<AccountInfo>>(
				without);
			const auto progress = std::make_shared<int>(0);
			const auto step = [=] {
				if (queue->empty()) {
					log->setText(QStringLiteral(
						"Done: %1 accounts").arg(*progress));
					return;
				}
				const auto acc = queue->front();
				queue->erase(queue->begin());
				enable2FA(acc.session, *pass, QString(), [=](
						bool ok,
						const QString &error) {
					if (ok) {
						++*progress;
						log->setText(QStringLiteral("%1: OK")
							.arg(acc.name));
					} else {
						log->setText(QStringLiteral("%1: FAILED (%2)")
							.arg(acc.name, error));
					}
					step();
				});
			};
			step();
		});
		box->addButton(rpl::single(QString("Close")), [=] {
			box->closeBox();
		});
	}));
}

} // namespace Ayu::Passwords
