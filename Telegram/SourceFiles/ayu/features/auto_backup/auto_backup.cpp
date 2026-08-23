// VanGram: automatic backup on app quit.
#include "ayu/features/auto_backup/auto_backup.h"

#include "ayu/ayu_updater.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <algorithm>

namespace Ayu::AutoBackup {
namespace {

QString settingsDir() {
	return QCoreApplication::applicationDirPath()
		+ QStringLiteral("/tdata");
}

QString markerPath() {
	return settingsDir() + QStringLiteral("/vangram_last_backup");
}

QString configPath() {
	return settingsDir() + QStringLiteral("/vangram_auto_backup.json");
}

constexpr auto kDefaultIntervalHours = 24;
// Quitting again soon after a backup must not trigger another one.
constexpr auto kQuitGraceSeconds = 10 * 60;

struct Config {
	bool enabled = true;
	int intervalHours = kDefaultIntervalHours;
};

Config loadConfig() {
	Config c;
	QFile f(configPath());
	if (f.open(QIODevice::ReadOnly)) {
		const auto obj = QJsonDocument::fromJson(f.readAll()).object();
		c.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
		const auto h = obj.value(QStringLiteral("intervalHours")).toInt(
			kDefaultIntervalHours);
		c.intervalHours = std::clamp(h, 6, 24 * 7);
	}
	return c;
}

qint64 lastBackupSecs() {
	QFile f(markerPath());
	if (f.open(QIODevice::ReadOnly)) {
		const auto dt = QDateTime::fromString(
			QString::fromUtf8(f.readAll()).trimmed());
		if (dt.isValid()) {
			return dt.secsTo(QDateTime::currentDateTime());
		}
	}
	return -1;
}

void markBackupNow() {
	QDir().mkpath(settingsDir());
	QFile f(markerPath());
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		f.write(QDateTime::currentDateTime().toString().toUtf8());
	}
}

} // namespace

qint64 secondsSinceLastBackup() {
	return lastBackupSecs();
}

bool maybeBackupOnQuit() {
	const auto config = loadConfig();
	if (!config.enabled) {
		return false;
	}
	const auto last = lastBackupSecs();
	const auto due = (last < 0)
		|| (last >= qint64(config.intervalHours) * 3600);
	// If we just did a backup (e.g. the helper relaunched us and the user
	// quits again), let it pass through without another backup.
	if (!due || (last >= 0 && last < kQuitGraceSeconds)) {
		return false;
	}

	const auto stamp = QDateTime::currentDateTime().toString(
		QStringLiteral("yyyyMMdd-HHmmss"));
	const auto zip = QCoreApplication::applicationDirPath()
		+ QStringLiteral("/tdata/vangram_backup-") + stamp
		+ QStringLiteral(".zip");

	// Write the marker BEFORE starting the flow: the helper relaunches the
	// app, and a failing upload must not loop backups on every quit.
	markBackupNow();

	Ayu::Updater::Instance().createBackup(zip, true);
	return true;
}

} // namespace Ayu::AutoBackup
