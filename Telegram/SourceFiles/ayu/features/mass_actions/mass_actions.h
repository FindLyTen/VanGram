// VanGram mass actions: bulk join / subscribe / leave with a delay.
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace Main {
class Session;
} // namespace Main

namespace Ayu {

class MassActions : public QObject {
	Q_OBJECT

public:
	enum class Action {
		JoinByLink,        // t.me/+hash  /  t.me/joinchat/hash
		SubscribeByUsername, // @username -> resolve -> join channel
		Leave,             // @username -> resolve -> leave channel
	};

	static MassActions &Instance();

	void start(
		Action action,
		const QStringList &targets,
		int delayMinSec,
		int delayMaxSec,
		Main::Session *session);
	void stop();
	[[nodiscard]] bool running() const { return _running; }

Q_SIGNALS:
	void progress(const QString &line);
	void finished();

private:
	explicit MassActions(QObject *parent = nullptr);
	void processNext();
	void finish();

	QString _current;
	int _index = 0;
	int _delayMin = 60;
	int _delayMax = 120;
	bool _running = false;
	Action _action = Action::JoinByLink;
	Main::Session *_session = nullptr;
	QStringList _targets;
};

} // namespace Ayu
