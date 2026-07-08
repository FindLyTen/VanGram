# VanGram

Кастомизированный клиент Telegram Desktop на базе [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop).

[ [English](README.md) | Русский ]

VanGram — персональный форк, заточенный под управление несколькими аккаунтами и
синхронизацию tdata между машинами. Наследует все возможности AyuGram и
собирается автоматически под Windows x64 через GitHub Actions.

## Функции и фишки

Унаследованы от AyuGram Desktop:

- Полный режим призрака (настраиваемый)
- История удалений и изменений сообщений
- Кастомизация шрифта
- Режим Стримера
- Локальный Telegram Premium
- Переводчик
- Улучшенный вид

Подробнее об унаследованных возможностях — в
[документации AyuGram](https://docs.ayugram.one/desktop/).

## Установка

### Windows x64

Готовые бинарники под Windows x64 собираются в CI при каждом пуше в `main`.
Возьмите свежий артефакт на вкладке
[Actions](https://github.com/FindLyTen/VanGram/actions) или со страницы
[Releases](https://github.com/FindLyTen/VanGram/releases), когда они будут
опубликованы.

> Бинарник по-прежнему называется `AyuGram.exe` для совместимости с
> существующим tdata и авто-апдейтером. Это намеренно.

### Сборка вручную

Следуйте [руководству по сборке Windows x64](docs/building-win-x64.md), если
хотите собрать VanGram сами. Гайды по macOS и Linux — в [docs/](docs/).

## Примечания для Windows

Убедитесь, что установлены эти компоненты VS Build Tools:

- C++ MFC latest (x86 & x64)
- C++ ATL latest (x86 & x64)
- последний Windows 11 SDK

## Использованные материалы

VanGram не существовал бы без этих проектов:

- [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop) — прямой апстрим
- [Telegram Desktop](https://github.com/telegramdesktop/tdesktop)
- [64Gram](https://github.com/TDesktop-x64/tdesktop)
- [Kotatogram](https://github.com/kotatogram/kotatogram-desktop)

VanGram основан на AyuGram, который разрабатывает и поддерживает
[Radolyn Labs](https://github.com/AyuGram).
