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

	[[nodiscard]] bool isReady() const { return _ready; }

private:
	Updater();
	void onManifest(QNetworkReply *reply);
	void startDownload(const QString &url);
	void onDownloaded();

	QNetworkAccessManager *_nam = nullptr;
	QString _zipPath;
	bool _ready = false;
};

} // namespace Ayu
