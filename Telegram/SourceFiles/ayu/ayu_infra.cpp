// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ayu_infra.h"

#include "ayu/ayu_lang.h"
#include "ayu/ayu_settings.h"
#include "ayu/ayu_ui_settings.h"
#include "ayu/ayu_worker.h"
#include "ayu/data/ayu_database.h"
#include "ayu/features/archive_reader/archive_reader.h"
#include "ayu/features/auto_backup/auto_backup.h"
#include "ayu/ui/ayu_logo.h"
#include "features/translator/ayu_translator.h"
#include "lang/lang_instance.h"
#include "core/application.h"
#include "main/main_domain.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "ui/chat/chat_style_radius.h"
#include "utils/rc_manager.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QTimer>

#ifdef Q_OS_WIN
#include "ayu/utils/windows_utils.h"
#endif

namespace AyuInfra {

void initLang() {
	QString id = Lang::GetInstance().id();
	QString baseId = Lang::GetInstance().baseId();
	if (id.isEmpty()) {
		LOG(("Language is not loaded"));
		return;
	}
	AyuLanguage::init();
	AyuLanguage::currentInstance()->fetchLanguage(id, baseId);
}

void initUiSettings() {
	const auto &settings = AyuSettings::getInstance();

	AyuUiSettings::setMonoFont(settings.monoFont());
	AyuUiSettings::setWideMultiplier(settings.wideMultiplier());
	AyuUiSettings::setMaterialSwitches(settings.materialSwitches());
	AyuUiSettings::setAvatarCorners(settings.avatarCorners());
	Ui::SetAppliedBubbleRadius(settings.messageBubbleRadius());
}

void initDatabase() {
	AyuDatabase::initialize();
}

void initWorker() {
	AyuWorker::initialize();
}

void initRCManager() {
	RCManager::getInstance().start();
}

void initTranslator() {
	Ayu::Translator::TranslateManager::init();
}

void initIcon() {
#ifdef Q_OS_WIN
	AyuAssets::loadAppIco();
	reloadAppIconFromTaskBar();
#endif
}

// VanGram: automatic per-account cache cleanup.
namespace {

QString cacheCleanedPath() {
	return QCoreApplication::applicationDirPath()
		+ QStringLiteral("/tdata/vangram_cache_cleaned");
}

QDateTime lastCacheClean() {
	QFile f(cacheCleanedPath());
	if (f.open(QIODevice::ReadOnly)) {
		return QDateTime::fromString(QString::fromUtf8(f.readAll()).trimmed());
	}
	return {};
}

void markCacheCleaned() {
	QFile f(cacheCleanedPath());
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		f.write(QDateTime::currentDateTime().toString().toUtf8());
	}
}

void cleanAllAccountsCache() {
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (const auto session = account->maybeSession()) {
			session->data().clearLocalStorage();
		}
	}
	markCacheCleaned();
}

} // namespace

void initCacheCleaner() {
	// Once per 24h of running time, and on startup if the last cleanup was
	// more than 24h ago (covers users who launch the app periodically).
	static auto started = false;
	if (started) {
		return;
	}
	started = true;
	const auto timer = new QTimer(qApp);
	timer->setInterval(24 * 60 * 60 * 1000); // 24h
	QObject::connect(timer, &QTimer::timeout, timer, cleanAllAccountsCache);
	QTimer::singleShot(5 * 60 * 1000, timer, [timer] {
		const auto last = lastCacheClean();
		if (last.isNull()
			|| last.secsTo(QDateTime::currentDateTime()) >= 24 * 60 * 60) {
			cleanAllAccountsCache();
		}
		timer->start();
	});
}

// VanGram: automatic Mega backup on quit (see ayu/features/auto_backup).
void initAutoBackup() {
	static auto started = false;
	if (started) {
		return;
	}
	started = true;
	QObject::connect(
		QCoreApplication::instance(),
		&QCoreApplication::aboutToQuit,
		qApp,
		[] {
			Ayu::AutoBackup::maybeBackupOnQuit();
		});
}

void init() {
	initLang();
	initDatabase();
	initUiSettings();
	initIcon();
	initWorker();
	initRCManager();
	initTranslator();
	initCacheCleaner();
	initAutoBackup();
	Ayu::ArchiveReader::init();
}

}
