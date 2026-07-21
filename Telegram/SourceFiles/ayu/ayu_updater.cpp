// VanGram self-updater implementation.
#include "ayu/ayu_updater.h"

#include "core/application.h"
#include "core/version.h"
#include "ui/toast/toast.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QTextStream>
#include <QtGui/QGuiApplication>

namespace Ayu {
namespace {

constexpr auto kManifestUrl =
	"https://github.com/FindLyTen/VanGram/releases/latest/download/manifest.json";

QString updateDir() {
	return QFileInfo(QGuiApplication::applicationFilePath())
			.absolutePath() + QStringLiteral("/tdata/vangram_update");
}

void toast(const QString &text) {
	Ui::Toast::Show(text);
}

} // namespace

Updater &Updater::Instance() {
	// Parented to qApp so it is torn down with Qt (during normal shutdown),
	// not as a function-static after QCoreApplication is already destroyed
	// (which caused a debug heap assertion on exit).
	static Updater *instance = nullptr;
	if (!instance) {
		instance = new Updater(qApp);
	}
	return *instance;
}

Updater::Updater(QObject *parent) : QObject(parent) {
	_nam = new QNetworkAccessManager(this);
}

void Updater::check() {
	_ready = false;
	toast(QStringLiteral("Checking for updates..."));

	auto req = QNetworkRequest(QUrl(QString::fromLatin1(kManifestUrl)));
	req.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);
	req.setHeader(
		QNetworkRequest::UserAgentHeader,
		QStringLiteral("VanGramUpdater"));

	auto reply = _nam->get(req);
	connect(reply, &QNetworkReply::finished, this, [this, reply] {
		reply->deleteLater();
		onManifest(reply);
	});
}

void Updater::onManifest(QNetworkReply *reply) {
	if (reply->error() != QNetworkReply::NoError) {
		toast(QStringLiteral("Update check failed: ") + reply->errorString());
		return;
	}
	const auto doc = QJsonDocument::fromJson(reply->readAll());
	const auto obj = doc.object();
	const auto version = obj.value(QStringLiteral("version")).toVariant().toLongLong();
	const auto url = obj.value(QStringLiteral("url")).toString();
	if (!version || url.isEmpty()) {
		toast(QStringLiteral("Update check failed: bad manifest."));
		return;
	}
	if (version <= AppVersion) {
		toast(QStringLiteral("VanGram is up to date."));
		return;
	}
	startDownload(url);
}

void Updater::startDownload(const QString &url) {
	toast(QStringLiteral("Downloading update..."));

	QDir().mkpath(updateDir());
	_zipPath = updateDir() + QStringLiteral("/update.zip");

	auto req = QNetworkRequest(QUrl(url));
	req.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);
	req.setHeader(
		QNetworkRequest::UserAgentHeader,
		QStringLiteral("VanGramUpdater"));

	auto reply = _nam->get(req);
	auto file = new QFile(_zipPath);
	if (!file->open(QIODevice::WriteOnly)) {
		delete file;
		reply->deleteLater();
		toast(QStringLiteral("Update failed: cannot write file."));
		return;
	}

	connect(reply, &QIODevice::readyRead, this, [reply, file] {
		file->write(reply->readAll());
	});
	connect(reply, &QNetworkReply::finished, this, [this, reply, file] {
		file->flush();
		file->close();
		delete file;
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			toast(QStringLiteral("Download failed: ") + reply->errorString());
			return;
		}
		onDownloaded();
	});
}

void Updater::onDownloaded() {
	_ready = true;
	toast(QStringLiteral("Update ready! Click \"Updates\" again to restart."));
}

void Updater::applyAndRestart() {
	if (!_ready) {
		return;
	}
	const auto exePath = QGuiApplication::applicationFilePath();
	const auto zipPath = _zipPath;
	const auto exDir = updateDir() + QStringLiteral("/ex");
	const auto pid = QString::number(QGuiApplication::applicationPid());
	const auto ps1 = updateDir() + QStringLiteral("/apply.ps1");

	QDir().mkpath(updateDir());
	QFile f(ps1);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
		toast(QStringLiteral("Update failed: cannot write apply script."));
		return;
	}
	auto esc = [](QString s) {
		return s.replace(QLatin1Char('\''), QStringLiteral("''"));
	};
	QTextStream s(&f);
	s << "$ErrorActionPreference='Stop'\r\n";
	s << "$p=" << pid << "\r\n";
	s << "$exe='" << esc(exePath) << "'\r\n";
	s << "$zip='" << esc(zipPath) << "'\r\n";
	s << "$ex='" << esc(exDir) << "'\r\n";
	s << "while(Get-Process -Id $p -ErrorAction SilentlyContinue){Start-Sleep -Milliseconds 500}\r\n";
	s << "Expand-Archive -Path $zip -DestinationPath $ex -Force\r\n";
	s << "$src=Get-ChildItem -Path $ex -Filter *.exe -Recurse | "
		 "Where-Object {$_.Name -notmatch 'Updater'} | "
		 "Select-Object -First 1\r\n";
	s << "if($src){Copy-Item -Path $src.FullName -Destination $exe -Force}\r\n";
	s << "Start-Sleep -Seconds 1\r\n";
	s << "Start-Process -FilePath $exe\r\n";
	f.close();

	QStringList args;
	args << QStringLiteral("-ExecutionPolicy")
		 << QStringLiteral("Bypass")
		 << QStringLiteral("-NoProfile")
		 << QStringLiteral("-WindowStyle")
		 << QStringLiteral("Hidden")
		 << QStringLiteral("-File")
		 << ps1;
	if (!QProcess::startDetached(QStringLiteral("powershell"), args)) {
		toast(QStringLiteral("Update failed: cannot start apply script."));
		return;
	}
	Core::Quit();
}

} // namespace Ayu
