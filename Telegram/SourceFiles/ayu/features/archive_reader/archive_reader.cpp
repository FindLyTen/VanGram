// VanGram: periodically mark archived chats as read on all accounts.
#include "ayu/features/archive_reader/archive_reader.h"

#include "core/application.h"
#include "main/main_domain.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "data/data_folder.h"
#include "data/data_histories.h"
#include "data/data_peer.h"
#include "dialogs/dialogs_list.h"
#include "history/history.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTimer>

#include <algorithm>

namespace Ayu::ArchiveReader {
namespace {

constexpr auto kDefaultIntervalHours = 6;

QString configPath() {
	return QCoreApplication::applicationDirPath()
		+ QStringLiteral("/tdata/vangram_archive_reader.json");
}

int loadIntervalHours() {
	QFile f(configPath());
	if (f.open(QIODevice::ReadOnly)) {
		const auto obj = QJsonDocument::fromJson(f.readAll()).object();
		return std::clamp(
			obj.value(QStringLiteral("intervalHours")).toInt(
				kDefaultIntervalHours),
			1,
			24 * 7);
	}
	return kDefaultIntervalHours;
}

void readAccountArchive(not_null<Main::Session*> session) {
	const auto owner = &session->data();
	const auto folder = owner->folderLoaded(Data::Folder::kId);
	if (!folder) {
		return;
	}
	const auto list = folder->chatsList();
	auto mark = std::vector<not_null<History*>>();
	for (const auto &row : list->indexed()->all()) {
		if (const auto history = row->history()) {
			mark.push_back(history);
		}
	}
	for (const auto history : mark) {
		history->owner().histories().readInbox(history);
	}
}

void runForAllAccounts() {
	if (!Core::IsAppLaunched()
		|| !Core::App().domain().started()
		|| Core::Quitting()) {
		return;
	}
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (const auto session = account->maybeSession()) {
			readAccountArchive(session);
		}
	}
}

} // namespace

void runOnce() {
	runForAllAccounts();
}

void init() {
	static auto started = false;
	if (started) {
		return;
	}
	started = true;

	const auto timer = new QTimer(qApp);
	const auto restart = [timer] {
		timer->setInterval(std::chrono::hours(loadIntervalHours()));
		timer->start();
	};
	QObject::connect(timer, &QTimer::timeout, timer, runForAllAccounts);

	// First pass shortly after startup (give sessions time to sync the
	// dialog list), then repeat on the configured interval.
	QTimer::singleShot(10 * 60 * 1000, timer, restart);
}

} // namespace Ayu::ArchiveReader
