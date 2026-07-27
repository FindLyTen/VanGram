// VanGram mass actions implementation.
#include "ayu/features/mass_actions/mass_actions.h"

#include "main/main_session.h"
#include "data/data_session.h"
#include "data/data_peer.h"
#include "data/data_channel.h"
#include "data/data_types.h"
#include "apiwrap.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtCore/QRandomGenerator>

namespace Ayu {
namespace {

QString extractHash(QString link) {
	link = link.trimmed();
	const auto plus = link.indexOf('+');
	if (plus >= 0) {
		return link.mid(plus + 1);
	}
	const auto jc = link.indexOf(QStringLiteral("joinchat/"), 0, Qt::CaseInsensitive);
	if (jc >= 0) {
		return link.mid(jc + 8);
	}
	const auto sl = link.lastIndexOf('/');
	if (sl >= 0) {
		return link.mid(sl + 1);
	}
	return link;
}

QString extractUsername(QString link) {
	link = link.trimmed();
	if (link.startsWith('@')) {
		return link.mid(1);
	}
	const auto tm = link.indexOf(QStringLiteral("t.me/"));
	if (tm >= 0) {
		auto u = link.mid(tm + 5);
		auto m = u.length();
		const auto q = u.indexOf('?');
		const auto s = u.indexOf('/');
		if (q >= 0 && q < m) {
			m = q;
		}
		if (s >= 0 && s < m) {
			m = s;
		}
		return u.left(m);
	}
	return link;
}

} // namespace

MassActions &MassActions::Instance() {
	static auto *instance = new MassActions(QCoreApplication::instance());
	return *instance;
}

MassActions::MassActions(QObject *parent) : QObject(parent) {
}

void MassActions::start(
		Action action,
		const QStringList &targets,
		int delayMinSec,
		int delayMaxSec,
		Main::Session *session) {
	if (_running) {
		return;
	}
	_action = action;
	_session = session;
	_delayMin = std::max(5, delayMinSec);
	_delayMax = std::max(_delayMin, delayMaxSec);
	_targets.clear();
	for (const auto &t : targets) {
		if (!t.trimmed().isEmpty()) {
			_targets.append(t.trimmed());
		}
	}
	_index = 0;
	_running = !_targets.isEmpty();
	if (!_running) {
		Q_EMIT progress(QStringLiteral("No targets."));
		Q_EMIT finished();
		return;
	}
	Q_EMIT progress(QStringLiteral("Starting: %1 targets, action = %2, delay %3-%4s")
		.arg(_targets.size())
		.arg(static_cast<int>(_action))
		.arg(_delayMin)
		.arg(_delayMax));
	processNext();
}

void MassActions::stop() {
	if (!_running) {
		return;
	}
	_running = false;
	Q_EMIT progress(QStringLiteral("Stopped."));
	Q_EMIT finished();
}

void MassActions::finish() {
	_running = false;
	Q_EMIT progress(QStringLiteral("Finished."));
	Q_EMIT finished();
}

void MassActions::processNext() {
	if (!_running) {
		return;
	}
	if (_index >= _targets.size()) {
		finish();
		return;
	}
	_current = _targets[_index];
	const auto total = _targets.size();
	const auto n = _index + 1;

	const auto done = [=](bool ok, const QString &msg) {
		if (!_running) {
			return;
		}
		Q_EMIT progress(QStringLiteral("[%1/%2] %3 -> %4")
			.arg(n).arg(total).arg(_current, ok ? QStringLiteral("OK") : msg));
		_index++;
		if (_index >= _targets.size()) {
			finish();
			return;
		}
		const auto span = _delayMax - _delayMin + 1;
		const auto delay = _delayMin + (span > 1 ? QRandomGenerator::global()->bounded(span) : 0);
		Q_EMIT progress(QStringLiteral("    waiting %1s...").arg(delay));
		QTimer::singleShot(delay * 1000, this, &MassActions::processNext);
	};

	if (_action == Action::JoinByLink) {
		const auto hash = extractHash(_current);
		_session->api().request(MTPmessages_ImportChatInvite(
			MTP_string(hash)
		)).done([=](const MTPUpdates &result) {
			_session->api().applyUpdates(result);
			done(true, QString());
		}).fail([=](const MTP::Error &error) {
			done(false, error.type());
		}).send();
		return;
	}

	// SubscribeByUsername / Leave: resolve username first.
	const auto username = extractUsername(_current);
	_session->api().request(MTPcontacts_ResolveUsername(
		MTP_flags(0),
		MTP_string(username),
		MTP_string()
	)).done([=](const MTPcontacts_ResolvedPeer &result) {
		result.match([&](const MTPDcontacts_resolvedPeer &data) {
			_session->data().processUsers(data.vusers());
			_session->data().processChats(data.vchats());
			const auto peerId = peerFromMTP(data.vpeer());
			const auto peer = peerId ? _session->data().peerLoaded(peerId) : nullptr;
			const auto channel = peer ? peer->asChannel() : nullptr;
			if (!channel) {
				done(false, QStringLiteral("not a channel"));
				return;
			}
			const auto isJoin = (_action == Action::SubscribeByUsername);
			const auto onDone = [=](const MTPUpdates &r) {
				_session->api().applyUpdates(r);
				done(true, QString());
			};
			const auto onFail = [=](const MTP::Error &error) {
				done(false, error.type());
			};
			if (isJoin) {
				_session->api().request(
					MTPchannels_JoinChannel(channel->inputChannel())
				).done(onDone).fail(onFail).send();
			} else {
				_session->api().request(
					MTPchannels_LeaveChannel(channel->inputChannel())
				).done(onDone).fail(onFail).send();
			}
		});
	}).fail([=](const MTP::Error &error) {
		done(false, error.type());
	}).send();
}

} // namespace Ayu
