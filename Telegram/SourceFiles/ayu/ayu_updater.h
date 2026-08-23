// VanGram self-updater.
//
// Checks GitHub Releases for a newer build (manifest.json), downloads it
// and applies it on restart through a generated PowerShell helper which
// waits for the app to exit, extracts the archive and relaunches the exe.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace Ayu {

class Updater : public QObject {
public:
	static Updater &Instance();

	// Asynchronously fetch the latest manifest and download the update.
	void check();

	// Apply a previously downloaded update and restart the app.
	void applyAndRestart();

	// VanGram integrated backup/restore. Both write a PowerShell helper
	// that runs after the app quits (so tdata files are closed and the
	// copy is consistent), then relaunch the app.
	// silent = no toast (used by the auto-backup-on-quit flow).
	void createBackup(const QString &zipPath, bool silent = false);
	void restoreBackup(const QString &zipPath);

	[[nodiscard]] bool isReady() const { return _ready; }

private:
	Updater(QObject *parent = nullptr);
	void onManifest(QNetworkReply *reply);
	void startDownload(const QString &url);
	void onDownloaded();
	void runScriptAndQuit(const QString &ps1Path, bool silent = false);

	QNetworkAccessManager *_nam = nullptr;
	QString _zipPath;
	bool _ready = false;
};

} // namespace Ayu
