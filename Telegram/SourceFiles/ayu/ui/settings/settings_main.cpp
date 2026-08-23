// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/settings/settings_main.h"

#include "settings/sections/settings_main.h"
#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/ayu_updater.h"
#include "ayu/features/mass_actions/mass_actions.h"
#include "ayu/ui/ayu_logo.h"
#include "ayu/ui/settings/settings_appearance.h"
#include "ayu/ui/settings/settings_ayu.h"
#include "ayu/ui/settings/settings_chats.h"
#include "ayu/ui/settings/settings_filters.h"
#include "ayu/ui/settings/settings_general.h"
#include "ayu/ui/settings/settings_other.h"
#include "core/version.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_settings.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/painter.h"
#include "ui/layers/box_content.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "window/window_session_controller_link_info.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QPlainTextEdit>

namespace Settings {

using namespace Builder;

namespace {

void BuildLogo(SectionBuilder &builder) {
	builder.add([](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto logo = object_ptr<Ui::RpWidget>(ctx.container);
		const auto logoRaw = logo.data();
		logoRaw->resize(
			QSize(st::settingsCloudPasswordIconSize,
				st::settingsCloudPasswordIconSize));
		logoRaw->setNaturalWidth(st::settingsCloudPasswordIconSize);
		logoRaw->paintRequest(
		) | rpl::on_next([=] {
			auto p = QPainter(logoRaw);
			const auto image = AyuAssets::currentAppLogoPad();
			if (!image.isNull()) {
				const auto size = st::settingsCloudPasswordIconSize;
				const auto scaled = image.scaled(
					size * style::DevicePixelRatio(),
					size * style::DevicePixelRatio(),
					Qt::KeepAspectRatio,
					Qt::SmoothTransformation);
				p.drawImage(QRect(0, 0, size, size), scaled);
			}
		}, logoRaw->lifetime());
		return { .widget = std::move(logo), .align = style::al_top };
	});
}

void BuildVersionInfo(SectionBuilder &builder) {
	builder.add([](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		return {
			.widget = object_ptr<Ui::FlatLabel>(
				ctx.container,
				rpl::single(
					QString("VanGram v")
					+ QString::fromLatin1(AppVersionStr)),
				st::boxTitle),
			.align = style::al_top,
		};
	});

	builder.addSkip();

	builder.add([](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		return {
			.widget = object_ptr<Ui::FlatLabel>(
				ctx.container,
				tr::ayu_SettingsDescription(),
				st::centeredBoxLabel),
			.align = style::al_top,
		};
	});
}

void BuildCategories(SectionBuilder &builder) {
	builder.addSkip();
	builder.addSkip();
	builder.addSkip();
	builder.addSkip();
	builder.addDivider();
	builder.addSkip();

	builder.addSubsectionTitle(tr::ayu_CategoriesHeader());

	builder.addSectionButton({
		.title = rpl::single(QString("VanGram")),
		.targetSection = AyuGhost::Id(),
		.icon = { &st::menuIconGroupReactions },
	});
	builder.addSectionButton({
		.title = tr::ayu_CategoryFilters(),
		.targetSection = AyuFilters::Id(),
		.icon = { &st::menuIconTagFilter },
	});
	builder.addSectionButton({
		.title = tr::ayu_CategoryGeneral(),
		.targetSection = AyuGeneral::Id(),
		.icon = { &st::menuIconShowAll },
	});
	builder.addSectionButton({
		.title = tr::ayu_CategoryAppearance(),
		.targetSection = AyuAppearance::Id(),
		.icon = { &st::menuIconPalette },
	});
	builder.addSectionButton({
		.title = tr::ayu_CategoryChats(),
		.targetSection = AyuChats::Id(),
		.icon = { &st::menuIconChatBubble },
	});
}

void BuildUpdateButton(SectionBuilder &builder) {
	builder.addSkip();
	builder.addButton({
		.id = u"vg/check-updates"_q,
		.title = rpl::single(QString("Check for Updates")),
		.icon = { &st::menuIconShowAll },
		.onClick = [] {
			auto &u = Ayu::Updater::Instance();
			if (u.isReady()) {
				u.applyAndRestart();
			} else {
				u.check();
			}
		},
	});
	builder.addSkip();
}

void BuildMassActionsButton(SectionBuilder &builder) {
	builder.addSkip();
	builder.addButton({
		.id = u"vg/mass-actions"_q,
		.title = rpl::single(QString("Mass Actions")),
		.icon = { &st::menuIconShowAll },
		.onClick = [c = builder.controller()] {
			c->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(rpl::single(QString("Mass Actions")));

				const auto actions = std::make_shared<std::vector<QString>>();
				*actions = {
					QStringLiteral("Action: Join by invite link"),
					QStringLiteral("Action: Subscribe by @username"),
					QStringLiteral("Action: Leave"),
				};
				const auto aIdx = std::make_shared<int>(0);
				const auto aBtn = box->addRow(
					object_ptr<Ui::RoundButton>(
						box,
						rpl::single((*actions)[0]),
						st::defaultLightButton),
					st::boxRowPadding);
				aBtn->setClickedCallback([=] {
					*aIdx = (*aIdx + 1) % int(actions->size());
					aBtn->setText(rpl::single((*actions)[*aIdx]));
				});

				const auto edit = box->addRow(
					object_ptr<Ui::InputField>(
						box,
						st::defaultInputField,
						Ui::InputField::Mode::MultiLine,
						rpl::single(QString(
							"one invite link / @username per line"))),
					st::boxRowPadding);
				edit->setMinimumHeight(160);

				const auto log = box->addRow(
					object_ptr<Ui::FlatLabel>(box, QString(), st::defaultFlatLabel),
					st::boxRowPadding);
				log->setText(QStringLiteral("idle"));

				QObject::connect(
					&Ayu::MassActions::Instance(),
					&Ayu::MassActions::progress,
					box,
					[=](const QString &line) { log->setText(line); });

				box->addButton(rpl::single(QString("Start")), [=] {
					const auto targets = edit->getLastText().split(
						'\n', Qt::SkipEmptyParts);
					Ayu::MassActions::Instance().start(
						static_cast<Ayu::MassActions::Action>(*aIdx),
						targets,
						60,
						120,
						&c->session());
				});
				box->addButton(rpl::single(QString("Stop")), [=] {
					Ayu::MassActions::Instance().stop();
				});
				box->addButton(rpl::single(QString("Close")), [=] {
					box->closeBox();
				});
			}));
		},
	});
	builder.addSkip();
}

void BuildBackupButtons(SectionBuilder &builder) {
	builder.addSkip();
	builder.addButton({
		.id = u"vg/backup"_q,
		.title = rpl::single(QString("Backup everything")),
		.icon = { &st::menuIconDownload },
		.onClick = [] {
			const auto zip = QFileDialog::getSaveFileName(
				nullptr,
				QStringLiteral("Save VanGram backup"),
				QStringLiteral("VanGram-backup.zip"),
				QStringLiteral("ZIP (*.zip)"));
			if (!zip.isEmpty()) {
				Ayu::Updater::Instance().createBackup(zip);
			}
		},
	});
	builder.addButton({
		.id = u"vg/restore"_q,
		.title = rpl::single(QString("Restore from backup")),
		.icon = { &st::menuIconIpAddress },
		.onClick = [] {
			const auto zip = QFileDialog::getOpenFileName(
				nullptr,
				QStringLiteral("Open VanGram backup"),
				QString(),
				QStringLiteral("ZIP (*.zip)"));
			if (!zip.isEmpty()) {
				Ayu::Updater::Instance().restoreBackup(zip);
			}
		},
	});
	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = AyuMain::Id(),
	.parentId = MainId(),
	.title = &tr::ayu_AyuPreferences,
	.icon = &st::menuIconPremium,
}, [](SectionBuilder &builder) {
	BuildLogo(builder);
	builder.addSkip();
	BuildVersionInfo(builder);
	BuildUpdateButton(builder);
	BuildBackupButtons(builder);
	BuildMassActionsButton(builder);
	BuildCategories(builder);
});

} // namespace

rpl::producer<QString> AyuMain::title() {
	return rpl::single(QString(""));
}

AyuMain::AyuMain(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AyuMain::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AyuMainId() {
	return AyuMain::Id();
}

} // namespace Settings
