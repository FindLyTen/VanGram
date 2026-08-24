
## СКЛАД VAN7 (ПЕРВОЕ ПРАВИЛО):
Если нужен skill / скрипт / шаблон / готовое решение — СНАЧАЛА ищи на складе:
- Каталог инструментов (живой, веб): /var/www/van7_sklad/ (api + data.json)
- Скиллы: /var/www/van7_sklad/skills/
- Не нашёл → пиши с нуля. Нашёл → используй, не плоди копии внутри проектов.

# Claude Code Pointer

Read `AGENTS.md` and treat it as the canonical repository-wide instructions.

# VANGRAM — ПОЛНЫЙ КОНТЕКСТ ПРОЕКТА

## Что это
VanGram — форк AyuGram Desktop (который форк Telegram Desktop) для Windows.
Владелец: FindLyTen. Публичный репо: https://github.com/FindLyTen/VanGram
Исходники на VPS: /root/vangram-src (git). Сборка ТОЛЬКО через GitHub CI (~1ч).
Локально НЕ собирать. Правки вслепую → проверять API по существующему коду.

## Инфраструктура
- CI: .github/workflows/vangram-build.yml — Windows x64, Qt6, Debug.
  - Триггеры: push в main + workflow_dispatch (ветки запускать руками: `gh workflow run vangram-build.yml --ref <branch>`).
  - publish rolling Release с zip + manifest.json — ТОЛЬКО на push в main.
  - env TDESKTOP_API_ID=2040 — официальный публичный api_id Telegram Desktop (не секрет).
- Релизы: https://github.com/FindLyTen/VanGram/releases/latest/download/manifest.json
  {version(int), url(zip), size}. Артефакт = ТОЛЬКО Vangram.exe (не вся папка).
- Самообновление: ayu/ayu_updater.{h,cpp} — Ayu::Updater (check/download/applyAndRestart,
  createBackup/restoreBackup). Кнопка «Check for Updates» в ГЛАВНЫХ настройках
  (settings/sections/settings_main.cpp BuildHelpSection).
- ПРАВИЛА CI (выучено кровью):
  - /Z7 в Debug (CMAKE_CXX_FLAGS_DEBUG) — иначе C2471 vc143.pdb.
  - permissions: contents: write — иначе 403 на release create.
  - gh-командам нужен --repo $env:GITHUB_REPOSITORY (build-папка не git).
  - version.h читать из Join-Path $env:TBUILD "$env:REPO_NAME\Telegram\...".
  - C1060 out of heap space — плавающая, просто rerun.
- КАЖДЫЙ релиз = бамп AppVersion в Telegram/SourceFiles/core/version.h
  (сейчас 6.7.18 = 6007018). Иначе апдейтер скажет up-to-date.

## Кодовая база — ключевые места
- QT_NO_KEYWORDS: только Q_SIGNALS/Q_EMIT, никаких signals/emit!
- Строки в новых файлах: QStringLiteral (НЕ u""_q — может не подключиться).
- Redirection: req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, NoLessSafeRedirectPolicy).
- FlatLabel цвет: setTextColorOverride(QColor). Инклуд «ui/widgets/labels.h».
- Box-паттерн: c->show(Box([=](not_null<Ui::GenericBox*> box){...})); инклуды
  «ui/layers/box_content.h» + «ui/layers/generic_box.h».
- SettingsButton — «ui/widgets/buttons.h».

### VanGram-фичи (мои)
- Сайдбар аккаунтов: window/window_accounts_menu.{h,cpp} — 240px, ряды как в
  main-menu (SettingsButton + юзерпик + кольцо + тег). Owned by Window::Controller.
  - Теги акков: tdata/vangram_tags.json {uniqueId: {text, color}}; правый клик →
    PopupMenu (Move up/down/Edit tag — с batch-7).
  - Подсветка активного: сравнение с &domain().active() (НЕ maybeLastOrSome —
    это ПРОШЛЫЙ акк, ловушка!).
  - Кнопка «+» Add account: domain().addActivated(MTP::Environment{}).
  - Logout UAF-фикс: подписка на sessionChanges каждого акка (_sessionsLifetime).
  - aboutToQuit: _buttons.clear() + _sessionsLifetime.destroy() (фикс debug_heap).
  - batch-7: drag&drop реордер + scrollTop-фикс (см. раздел client-batch-7).
- Mass Actions: ayu/features/mass_actions/{mass_actions.h,cpp} — вход по инвайт-
  ссылке (MTPmessages_ImportChatInvite), подписка/выход по @username
  (ResolveUsername → Join/LeaveChannel), cooldown random 60-120s, Q_SIGNALS
  progress/finished. UI: BuildMassActionsButton в ayu/ui/settings/settings_main.cpp.
- Автоочистка кеша: ayu_infra.cpp initCacheCleaner() — раз в 24h чистит
  session->data().clearLocalStorage() у всех акков (файл-маркер tdata/vangram_cache_cleaned).
- Удалёнки: isMessageSavable (ayu/utils/telegram_helpers.cpp) — НЕ сохранять в
  Saved Messages (isSelf) и broadcast-каналах (isChannel && !isMegagroup).
- Иконки: юзер-логотип из /root/van7tg/main_logo.png (чёрный фон вырезан).
  icon256.ico + PNG set + app.png/app_icon.ico ВО ВСЕХ 12 темах ayu (qrc тащит
  app.png каждой темы). Трей всегда цветной (trayIconMonochrome форсится false,
  platform/win/tray_win.cpp). МОНОХРОМНЫЕ tray_*.svg — самолик (не юзер-логотип).
- Убрано из AyuGram: FAQ/Features/Ask/Gift из настроек; Links/Other категории;
  hamburger-меню не показывает акков (всё в сайдбаре); видимые «AyuGram»→«VanGram».

## VPS-инфраструктура (этот сервер)
- van7tg (веб-ферма 57 акков): ОСТАНОВЛЕН и disable по решению юзера.
  sessions/*.session + accounts.json + tags.json ЦЕЛЕЕ — НЕ ТРОГАТЬ.
- vangram-backup receiver: /root/vangram-backup/receiver.py, systemd vangram-backup,
  порт 3001 ТОЛЬКО на Tailscale IP 100.97.181.59. POST /backup (X-Token из
  /root/vangram-backup-token.txt) → rclone copy в mega-tg:tg-backup/.
- rclone remote mega-tg = Mega muzofon.md@gmail.com (пароль obscured в
  /root/.config/rclone/rclone.conf). Второй remote «MEGA Sklad» — не трогать.
- Tailscale: VPS=100.97.181.59, ноут юзера=100.90.157.106.

## КРИТИЧЕСКИЕ ПРАВИЛА
- ОДИН auth_key = ОДНО устройство/IP в момент. Клон tdata на 2 устройства
  одновременно = смерть сессий. Перенос (по очереди) — ок. QR/отдельный вход
  на каждом устройстве — ок (мультидевайс штатный).
- Пароли/токены НЕ класть в репо (он публичный). Секреты — только локальные
  конфиги VPS (rclone.conf, token-файлы).
- Никогда не коммитить .session/tdata/accounts.json (gitignore уже настроен).
- Сборка веток: gh workflow run vangram-build.yml --ref <branch>; влить в main
  → релиз опубликуется автоматом → юзер обновится кнопкой в клиенте.

## ОТКРЫТЫЕ ЗАДАЧИ (бэклог)
- 6.7.18 Mass Actions: сборка 30366769046 УПАЛА (C2672 addRow QPlainTextEdit
  в settings_main.cpp Mass Actions box) — фикс уже в client-batch-7
  (d0cd848: замена на Ui::InputField MultiLine + getLastText).
- Краш при смене темы/цвета (likely AyuGram MessageShot colorizer hook,
  settings_chat.cpp ~290, 2371+) → потом тёмно-синяя тема юзера.
- Upstream sync с tdesktop (у юзера «обновите версию Telegram» на новых смс).
- Установщик/лаунчер (полная папка с DLL, не только exe) — юзер хочет ставить
  на новые устройства.
- Mega-пароль юзера скомпрометирован (прислан в чат) — юзер должен сменить,
  потом обновить rclone config update mega-tg pass "<новый>".

## ВЕТКА client-batch-7 (в работе, НЕ влита в main)
План юзера: сайдбар-реордер, авто-бэкап MEGA, авто-архив, кеш, контакты,
пароли 2FA. WP6 (бот-уведомлятор) — ОТМЕНЁН юзером. Всё сборки — batch-7,
одна CI-сборка в конце, потом main + релиз 6.7.19 (бамп AppVersion!).

### Готово в batch-7:
- WP1 (1d3b6ab) сайдбар: drag&drop (VerticalLayoutReorder(_list,&_scroll)),
  персист через Core::App().settings().setAccountsOrder+saveSettings;
  правый клик = PopupMenu (Move up/Move down/Edit tag);
  фикс «кидает в начало» = save scrollTop → refresh → scrollToY(oldTop)
  в refresh() И activeChanges(). window_accounts_menu.{h,cpp}.
- WP2 (12dee7a) авто-бэкап: ayu/features/auto_backup/ — aboutToQuit хук
  (initAutoBackup в ayu_infra.cpp), маркер tdata/vangram_last_backup,
  конфиг tdata/vangram_auto_backup.json (enabled, intervalHours 6..168,
  дефолт 24), grace 10 мин (повторный выход без бэкапа), маркер пишется
  ДО createBackup (нет цикла), silent-режим createBackup(zip,true).
  UI: Auto-backup on quit ON/OFF + interval 12/24/48 (rpl::variable label).
- WP3 (12dee7a) архив: ayu/features/archive_reader/ — таймер, дефолт 6ч,
  конфиг tdata/vangram_archive_reader.json (intervalHours 1..168),
  readAccountArchive = folderLoaded(Data::Folder::kId)->chatsList()
  → histories().readInbox(history). UI: интервал 3/6/12/24 + Read now.
  Clean cache now кнопка = clearLocalStorage() по всем аккам.
- WP4 (e9b3c36) контакты: ayu/features/contacts_manager/ — box с чекбоксами
  (Ui::Checkbox defaultBoxCheckbox, мои акки disabled), удаление чанками
  по 25 (MTPcontacts_DeleteContacts), «Delete all except mine» по всем
  аккам (match по PeerId моих сессий), requestContacts() рефреш.
  Вход: Preferences → Contacts manager.
- CMake (b4a27b6): новые файлы в Telegram/CMakeLists.txt (явный список!).

### API-заметки (выучено при WP1-4):
- addRow принимает только RpWidget-виджеты (не QPlainTextEdit!).
- SettingsButton label обновлять через rpl::variable<QString> (setLabel нет).
- Ui::Menu::MenuCallback::Args для addItem в PopupMenu;
  иконки: menu_icons.style (menuIconUp/Down НЕ существуют).
- VerticalLayout::clear() удаляет детей; reorder: VerticalLayoutReorder.
- accountsOrder: core_settings.h:871 setAccountsOrder(vector<uint64>).
- contactsLoaded() = false + api().requestContacts() для рефреша.
- contacts: session->data().contactsList()->all() → row->history()->peer
  →asUser() → isContact().
- Core::Quit() внутри aboutToQuit безопасен (Quitting() guard).
- ranges::find доступен без include (range-v3 транзитивно).

### WP5 (в работе): менеджер паролей 2FA
- ayu/features/passwords/ (passwords.h создан): store
  tdata/vangram_passwords.json {uniqueId: {password,label,notes,updatedAt}},
  GeneratePassword(20), ShowPasswordsBox, ShowEnable2FABox.
- Включение 2FA без старого пароля: MTPaccount_GetPassword() →
  Core::ParseCloudPasswordAlgo (newAlgo из MTPDaccount_password) →
  Core::ComputeCloudPasswordDigest(algo, utf8) →
  MTPaccount_UpdatePasswordSettings(MTP_inputCheckPasswordEmpty(),
  MTP_account_passwordInputSettings(f_new_algo|f_new_password_hash|f_hint,
  PrepareCloudPasswordAlgo, MTP_bytes(hash.modpow), hint, email=пусто,
  MTPSecureSecretSettings())) + .handleFloodErrors()
  (образец: PasscodeBox::setNewCloudPassword passcode_box.cpp:858+).
- UI план: box список всех акков (имя+тег+статус есть/нет пароля),
  копирование в буфер, генератор, «включить 2FA» для безпарольных.
  После успеха — автосохранение в store + кнопка в Preferences
  (BuildPasswordsButton) И отдельная кнопка внизу сайдбара
  (в ensureAddButton, место свободно).

## ИНФРА-ЗАМЕТКИ
- GitHub ключ: gh CLI авторизован (FindLyTen, repo+workflow scopes).
- Сборка ~1ч, C1060 плавающая → просто rerun. Влить batch-6 → main НЕ надо:
  batch-7 включает его с фиксами.
- vangram-backup receiver: порт 3001, X-Token из /root/vangram-backup-token.txt,
  rclone → mega-tg:tg-backup/ (MEGA muzofon.md@gmail.com).
