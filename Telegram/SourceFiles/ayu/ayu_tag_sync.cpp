// VanGram: account tags sync between devices via VPS receiver.
#include "ayu/ayu_tag_sync.h"

#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>

namespace Ayu {
namespace TagsSync {
namespace {

constexpr auto kBaseUrl = "http://100.97.181.59:3001";
// Token mirrors the one used by auto-backup (ayu_updater.cpp).
// Loaded lazily from an embedded constant; keep in sync with the receiver.
constexpr auto kToken = "WNoYMFnDra0ieat1LHaXBCrIF7C_YF-e";

QString tagsFilePath() {
	return QCoreApplication::applicationDirPath()
			+ QStringLiteral("/tdata/vangram_tags.json");
}

QNetworkAccessManager &nam() {
	static QNetworkAccessManager *m = nullptr;
	if (!m) {
		m = new QNetworkAccessManager(qApp);
	}
	return *m;
}

QNetworkRequest makeRequest(const QString &path) {
	QNetworkRequest req(QUrl(kBaseUrl + path));
	req.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);
	req.setRawHeader("X-Token", kToken);
	req.setHeader(
		QNetworkRequest::ContentTypeHeader,
		QStringLiteral("application/json"));
	return req;
}

qint64 entryUpdated(const QJsonObject &o) {
	return o.value(QStringLiteral("updated_at")).toVariant().toLongLong();
}

QJsonObject mergeEntries(const QJsonObject &local, const QJsonObject &remote) {
	QJsonObject merged = local;
	for (auto it = remote.begin(); it != remote.end(); ++it) {
		if (!it.value().isObject()) {
			continue;
		}
		const auto r = it.value().toObject();
		const auto cur = merged.value(it.key());
		if (!cur.isObject()
			|| entryUpdated(r) > entryUpdated(cur.toObject())) {
			merged.insert(it.key(), r);
		}
	}
	return merged;
}

QJsonObject loadLocal() {
	QJsonObject out;
	QFile f(tagsFilePath());
	if (f.open(QIODevice::ReadOnly)) {
		const auto doc = QJsonDocument::fromJson(f.readAll());
		if (doc.isObject()) {
			out = doc.object();
		}
	}
	return out;
}

void saveLocal(const QJsonObject &tags) {
	const auto dir = QFileInfo(tagsFilePath()).absolutePath();
	QDir().mkpath(dir);
	QFile f(tagsFilePath());
	if (f.open(QIODevice::WriteOnly)) {
		f.write(QJsonDocument(tags).toJson(QJsonDocument::Compact));
	}
}

} // namespace

bool Push(const QJsonObject &tags) {
	QJsonObject payload;
	payload.insert(QStringLiteral("tags"), tags);
	const auto data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
	auto reply = nam().post(makeRequest(QStringLiteral("/tags")), data);
	QObject::connect(reply, &QNetworkReply::finished, reply, [reply] {
		reply->deleteLater();
	});
	return true;
}

bool PushFromFile() {
	return Push(loadLocal());
}

void Pull(Fn<void(QJsonObject)> done) {
	auto reply = nam().get(makeRequest(QStringLiteral("/tags")));
	QObject::connect(reply, &QNetworkReply::finished, reply, [=] {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			if (done) {
				done(loadLocal());
			}
			return;
		}
		const auto doc = QJsonDocument::fromJson(reply->readAll());
		const auto remote = doc.object()
			.value(QStringLiteral("tags")).toObject();
		const auto merged = mergeEntries(loadLocal(), remote);
		saveLocal(merged);
		if (done) {
			done(merged);
		}
	});
}

} // namespace TagsSync
} // namespace Ayu
