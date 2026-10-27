// SPDX-FileCopyrightText: 2026 Drawy contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "systempalette.hpp"

#include <KConfig>
#include <KConfigGroup>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace SystemPalette
{
namespace
{
constexpr qint64 maximumInputSize = 1024 * 1024;
// Order matters: the first readable file is primary, later ones only fill gaps.
const QStringList directoryFiles{u"palette.json"_s, u"Colors.qml"_s, u"noctalia.json"_s, u"kitty.conf"_s};

constexpr double normalTextContrast = 7.0;
constexpr double secondaryTextContrast = 4.5;
constexpr double decorationContrast = 3.0;

QByteArray readBounded(const QString &path)
{
    if (!QFileInfo(path).isFile()) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > maximumInputSize) {
        return {};
    }
    return file.read(maximumInputSize);
}

std::optional<double> channel(const QString &expression)
{
    const auto parts = expression.trimmed().split(u'/');
    if (parts.size() > 2) {
        return std::nullopt;
    }
    bool valid = false;
    double value = parts.first().trimmed().toDouble(&valid);
    if (!valid) {
        return std::nullopt;
    }
    if (parts.size() == 2) {
        bool denominatorValid = false;
        const double denominator = parts.last().trimmed().toDouble(&denominatorValid);
        if (!denominatorValid || denominator <= 0) {
            return std::nullopt;
        }
        value /= denominator;
    }
    if (!std::isfinite(value) || value < 0 || value > 1) {
        return std::nullopt;
    }
    return value;
}

QColor parseColor(QString value)
{
    value = value.trimmed();
    static const QRegularExpression rgbaPattern(uR"(^Qt\.rgba\((.*)\)$)"_s);
    if (const auto rgba = rgbaPattern.match(value); rgba.hasMatch()) {
        const auto parts = rgba.captured(1).split(u',');
        if (parts.size() != 4) {
            return {};
        }
        const auto r = channel(parts[0]);
        const auto g = channel(parts[1]);
        const auto b = channel(parts[2]);
        const auto a = channel(parts[3]);
        if (!r || !g || !b || !a) {
            return {};
        }
        return QColor::fromRgbF(*r, *g, *b, *a);
    }
    static const QRegularExpression hexPattern(u"^#?([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$"_s);
    if (const auto hex = hexPattern.match(value); hex.hasMatch()) {
        return QColor(u"#"_s + hex.captured(1));
    }
    return {};
}

QString normalizedKey(QString key)
{
    key = key.toLower();
    key.remove(QRegularExpression(u"[^a-z0-9]"_s));
    return key;
}

QMap<QString, QColor> qmlColors(const QString &path)
{
    QString contents = QString::fromUtf8(readBounded(path));
    contents.remove(QRegularExpression(u"/\\*.*?\\*/"_s, QRegularExpression::DotMatchesEverythingOption));
    contents.remove(QRegularExpression(u"//[^\\n]*"_s));
    static const QRegularExpression propertyPattern(uR"REGEX(property\s+color\s+(\w+)\s*:\s*([^\r\n]+))REGEX"_s);
    QMap<QString, QColor> result;
    auto matches = propertyPattern.globalMatch(contents);
    while (matches.hasNext()) {
        const auto match = matches.next();
        QString value = match.captured(2).trimmed();
        if (value.endsWith(u';')) {
            value.chop(1);
        }
        if (value.size() >= 2 && (value.startsWith(u'"') || value.startsWith(u'\'')) && value.endsWith(value.at(0))) {
            value = value.mid(1, value.size() - 2);
        }
        if (const auto color = parseColor(value); color.isValid()) {
            result.insert(match.captured(1), color);
        }
    }
    return result;
}

QMap<QString, QColor> kittyColors(const QString &path)
{
    const auto contents = QString::fromUtf8(readBounded(path));
    static const QRegularExpression linePattern(
        uR"(^\s*(background|foreground|cursor|color(?:[0-9]|1[0-5]))\s+(#[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?)\s*(?:#.*)?$)"_s,
        QRegularExpression::MultilineOption);
    QMap<QString, QColor> result;
    auto matches = linePattern.globalMatch(contents);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result.insert(match.captured(1), parseColor(match.captured(2)));
    }
    return result;
}

QMap<QString, QColor> jsonColors(const QJsonObject &values)
{
    QMap<QString, QColor> result;
    for (auto entry = values.constBegin(); entry != values.constEnd(); ++entry) {
        if (entry.value().isString()) {
            if (const auto color = parseColor(entry.value().toString()); color.isValid()) {
                result.insert(entry.key(), color);
            }
        }
    }
    return result;
}

std::optional<RawPalette> readFile(const QString &path)
{
    RawPalette palette;
    if (path.endsWith(u".qml"_s, Qt::CaseInsensitive)) {
        palette.colors = qmlColors(path);
    } else if (path.endsWith(u".conf"_s, Qt::CaseInsensitive)) {
        palette.colors = kittyColors(path);
    } else {
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(readBounded(path), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            return std::nullopt;
        }
        auto data = document.object();
        for (int depth = 0; depth < 4; ++depth) {
            const auto nested = {u"colors"_s, u"colours"_s, u"palette"_s};
            const auto found = std::ranges::find_if(nested, [&](const QString &key) {
                return data.value(key).isObject();
            });
            if (found == nested.end()) {
                break;
            }
            data = data.value(*found).toObject();
        }
        const auto declared = data.value(u"mode"_s).toString().toLower();
        if (data.value(u"dark"_s).isObject() || data.value(u"light"_s).isObject()) {
            // noctalia ships both modes in one file.
            const QString primary = declared == u"light"_s ? u"light"_s : u"dark"_s;
            const QString other = primary == u"dark"_s ? u"light"_s : u"dark"_s;
            palette.colors = jsonColors(data.value(primary).toObject());
            palette.mode = primary;
            palette.alternate = jsonColors(data.value(other).toObject());
            palette.alternateMode = other;
        } else {
            palette.colors = jsonColors(data);
            if (declared == u"dark"_s || declared == u"light"_s) {
                palette.mode = declared;
            }
        }
    }
    if (palette.colors.isEmpty()) {
        return std::nullopt;
    }
    return palette;
}

QString candidatePath(const QString &directory, const QString &name)
{
    return QDir(directory).filePath(name);
}

QString expanded(QString path)
{
    if (path == u"~"_s) {
        path = QDir::homePath();
    } else if (path.startsWith(u"~/"_s)) {
        path = QDir::homePath() + path.mid(1);
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QColor find(const QMap<QString, QColor> &colors, const QStringList &aliases, const QColor &fallback)
{
    for (const auto &alias : aliases) {
        const auto expected = normalizedKey(alias);
        for (auto entry = colors.constBegin(); entry != colors.constEnd(); ++entry) {
            if (normalizedKey(entry.key()) == expected && entry.value().isValid()) {
                return QColor(entry.value().rgb());
            }
        }
    }
    return fallback;
}

QColor mix(const QColor &base, const QColor &tint, double amount)
{
    return QColor::fromRgbF(base.redF() * (1 - amount) + tint.redF() * amount,
                            base.greenF() * (1 - amount) + tint.greenF() * amount,
                            base.blueF() * (1 - amount) + tint.blueF() * amount);
}

double luminance(const QColor &color)
{
    const auto linear = [](double component) {
        return component <= .04045 ? component / 12.92 : std::pow((component + .055) / 1.055, 2.4);
    };
    return .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
}

QColor backgroundOf(const QMap<QString, QColor> &colors)
{
    return find(colors, {u"background"_s, u"base"_s, u"mSurface"_s, u"surface"_s}, {});
}

QString inferMode(const QMap<QString, QColor> &colors)
{
    const auto background = backgroundOf(colors);
    return background.isValid() && luminance(background) >= .18 ? u"light"_s : u"dark"_s;
}

bool sameTheme(const QMap<QString, QColor> &first, const QMap<QString, QColor> &second)
{
    const auto a = backgroundOf(first);
    const auto b = backgroundOf(second);
    return a.isValid() && b.isValid() && a.rgb() == b.rgb();
}

bool passes(const QColor &foreground, const QList<QColor> &backgrounds, double target)
{
    return std::ranges::all_of(backgrounds, [&](const QColor &background) {
        return contrast(foreground, background) >= target;
    });
}

// Nudges lightness (hue and saturation kept) toward whichever side of the backgrounds has
// room; only when neither side can reach the target does it settle for the better extreme.
QColor ensure(const QColor &color, const QList<QColor> &backgrounds, double target)
{
    if (passes(color, backgrounds, target)) {
        return color;
    }
    const double hue = std::max<double>(0.0, color.hslHueF());
    const double saturation = color.hslSaturationF();
    const double start = color.lightnessF();
    double meanLuminance = 0;
    for (const auto &background : backgrounds) {
        meanLuminance += luminance(background) / backgrounds.size();
    }
    const int preferred = meanLuminance < .18 ? 1 : -1;
    for (const int direction : {preferred, -preferred}) {
        for (int step = 1; step <= 100; ++step) {
            const double lightness = start + direction * step / 100.0;
            if (lightness < 0 || lightness > 1) {
                break;
            }
            const auto candidate = QColor::fromHslF(hue, saturation, lightness);
            if (passes(candidate, backgrounds, target)) {
                return candidate;
            }
        }
    }
    const QColor black(Qt::black);
    const QColor white(Qt::white);
    const auto worst = [&](const QColor &candidate) {
        double lowest = 1e9;
        for (const auto &background : backgrounds) {
            lowest = std::min(lowest, contrast(candidate, background));
        }
        return lowest;
    };
    return worst(black) > worst(white) ? black : white;
}

Group makeGroup(const QColor &background, const QColor &alternate, const QColor &text, const QColor &muted, const QColor &accent, const QColor &red,
                const QColor &yellow, const QColor &green)
{
    const QList<QColor> backgrounds{background, alternate};
    Group group;
    group.background = background;
    group.alternate = alternate;
    group.normal = ensure(text, backgrounds, normalTextContrast);
    group.inactive = ensure(muted, backgrounds, secondaryTextContrast);
    group.link = ensure(accent, backgrounds, secondaryTextContrast);
    group.active = group.link;
    group.visited = ensure(mix(accent, text, .4), backgrounds, secondaryTextContrast);
    group.negative = ensure(red, backgrounds, secondaryTextContrast);
    group.neutral = ensure(yellow, backgrounds, secondaryTextContrast);
    group.positive = ensure(green, backgrounds, secondaryTextContrast);
    group.focus = ensure(accent, {background}, decorationContrast);
    group.hover = group.focus;
    return group;
}

QColor rgbOf(const QColor &color)
{
    return QColor(color.rgb());
}
}

double contrast(const QColor &foreground, const QColor &background)
{
    const double first = luminance(foreground);
    const double second = luminance(background);
    return (std::max(first, second) + .05) / (std::min(first, second) + .05);
}

std::optional<RawPalette> read(const QString &path)
{
    const auto binding = expanded(path);
    if (!QFileInfo(binding).isDir()) {
        return readFile(binding);
    }
    std::optional<RawPalette> result;
    for (const auto &name : directoryFiles) {
        const auto file = candidatePath(binding, name);
        if (!QFileInfo(file).isFile()) {
            continue;
        }
        const auto next = readFile(file);
        if (!next) {
            continue;
        }
        if (!result) {
            result = next;
            continue;
        }
        for (auto entry = next->colors.constBegin(); entry != next->colors.constEnd(); ++entry) {
            if (!result->colors.contains(entry.key())) {
                result->colors.insert(entry.key(), entry.value());
            }
        }
        // A companion's other-mode palette is only trusted when it describes the same theme.
        // Stale files (an older wallpaper's noctalia.json) must not leak their light colors in.
        if (result->alternate.isEmpty() && !next->alternate.isEmpty() && sameTheme(result->colors, next->colors)) {
            result->alternate = next->alternate;
            result->alternateMode = next->alternateMode;
            result->mode = next->mode;
        }
    }
    if (result && result->mode.isEmpty()) {
        result->mode = inferMode(result->colors);
    }
    return result;
}

QStringList paletteFiles(const QString &path)
{
    const auto binding = expanded(path);
    QStringList result;
    const auto add = [&result](const QString &file) {
        const QFileInfo info(file);
        if (info.exists()) {
            result.append(info.absoluteFilePath());
            const auto canonical = info.canonicalFilePath();
            if (!canonical.isEmpty()) {
                result.append(canonical);
            }
        }
    };
    add(binding);
    if (QFileInfo(binding).isDir()) {
        for (const auto &name : directoryFiles) {
            add(candidatePath(binding, name));
        }
    }
    result.removeDuplicates();
    return result;
}

QString defaultSource()
{
    const auto config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const auto state = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
    const QStringList candidates{
        config + u"/themes/.caelestia-use"_s,
        config + u"/themes/current"_s,
        state + u"/caelestia/shell-theme-palette.json"_s,
        config + u"/themes/.dynamic/noctalia"_s,
    };
    for (const auto &candidate : candidates) {
        if (QFileInfo::exists(candidate) && read(candidate)) {
            return candidate;
        }
    }
    return {};
}

Scheme map(const RawPalette &raw, Mode mode)
{
    const bool wantDark = mode == Mode::Dark;
    const QString wanted = wantDark ? u"dark"_s : u"light"_s;

    auto colors = raw.colors;
    // Mode labels lie (a noctalia "light" block can be a copy of the dark one), so judge by the
    // background actually being used.
    QString sourceMode = inferMode(colors);
    if (sourceMode != wanted && !raw.alternate.isEmpty() && inferMode(raw.alternate) == wanted) {
        colors = raw.alternate;
        sourceMode = wanted;
    }

    const QColor sourceBackground = find(colors, {u"background"_s, u"base"_s, u"mSurface"_s, u"surface"_s}, QColor(u"#182125"_s));
    const QColor sourceText = find(colors, {u"foreground"_s, u"text"_s, u"mOnSurface"_s, u"onBackground"_s, u"onSurface"_s}, QColor(u"#e3e9e5"_s));
    const QColor accent = find(colors, {u"primary"_s, u"mPrimary"_s, u"accent"_s, u"cursor"_s, u"color4"_s, u"blue"_s}, QColor(u"#a9c7b0"_s));
    const QColor red = find(colors, {u"danger"_s, u"red"_s, u"mError"_s, u"color1"_s}, QColor(u"#d94528"_s));
    const QColor yellow = find(colors, {u"warning"_s, u"yellow"_s, u"mTertiary"_s, u"color3"_s, u"peach"_s}, QColor(u"#e3a533"_s));
    const QColor green = find(colors, {u"success"_s, u"green"_s, u"color2"_s}, QColor(u"#53623a"_s));

    QColor background = sourceBackground;
    QColor text = sourceText;
    QColor muted = find(colors, {u"foregroundInactive"_s, u"subtext0"_s, u"mOnSurfaceVariant"_s, u"muted"_s, u"overlay0"_s}, {});
    if (sourceMode != wanted) {
        // No palette for this mode: keep the shell's hue, derive neutral surfaces and text.
        const double hue = std::max<double>(0.0, sourceBackground.hslHueF() >= 0 ? sourceBackground.hslHueF() : accent.hslHueF());
        const double saturation = std::min<double>(.3, sourceBackground.hslSaturationF());
        background = wantDark ? QColor::fromHslF(hue, saturation, .11) : QColor::fromHslF(hue, saturation * .6, .97);
        text = wantDark ? QColor::fromHslF(hue, saturation * .5, .92) : QColor::fromHslF(hue, saturation, .12);
        muted = {};
    }
    if (!muted.isValid()) {
        muted = mix(background, text, .62);
    }
    background = rgbOf(background);
    text = rgbOf(text);

    const auto surface = [&](double amount) {
        return mix(background, text, amount);
    };
    Scheme scheme;
    scheme.dark = wantDark;
    scheme.view = makeGroup(background, surface(.045), text, muted, accent, red, yellow, green);
    scheme.window = makeGroup(surface(.07), surface(.12), text, muted, accent, red, yellow, green);
    scheme.button = makeGroup(surface(.12), surface(.17), text, muted, accent, red, yellow, green);
    scheme.tooltip = makeGroup(surface(.10), surface(.15), text, muted, accent, red, yellow, green);
    scheme.header = scheme.window;

    // Selection: accent kept visible against the surfaces it sits on. Black or white text cannot reach
    // the target on a mid-tone, so pick the text first and move the background away from it.
    const QColor visibleAccent = ensure(accent, {scheme.view.background, scheme.window.background}, decorationContrast);
    const double selectionHue = std::max<double>(0.0, visibleAccent.hslHueF());
    const QColor dark = QColor::fromHslF(selectionHue, .2, .06);
    const QColor light = QColor::fromHslF(selectionHue, .2, .98);
    const QColor selectionText = contrast(dark, visibleAccent) >= contrast(light, visibleAccent) ? dark : light;
    const QColor selectionBackground = ensure(visibleAccent, {selectionText}, normalTextContrast);
    const QColor selectionAlternate = mix(selectionBackground, selectionText == dark ? light : dark, .08);
    const QList<QColor> selectionBackgrounds{selectionBackground, selectionAlternate};
    Group selection;
    selection.background = selectionBackground;
    selection.alternate = selectionAlternate;
    selection.normal = selectionText;
    selection.inactive = ensure(mix(selectionText, selectionBackground, .25), selectionBackgrounds, secondaryTextContrast);
    selection.active = selection.link = selection.visited = selection.negative = selection.neutral = selection.positive = selectionText;
    selection.focus = selection.hover = ensure(selectionText, {selectionBackground}, decorationContrast);
    scheme.selection = selection;
    return scheme;
}

bool writeColorsFile(const Scheme &scheme, const QString &name, const QString &path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    KConfig config(path, KConfig::SimpleConfig);
    KConfigGroup general(&config, u"General"_s);
    general.writeEntry("ColorScheme", name);
    general.writeEntry("Name", name);
    const auto write = [&config](const QString &group, const Group &values) {
        KConfigGroup target(&config, group);
        target.writeEntry("BackgroundNormal", values.background);
        target.writeEntry("BackgroundAlternate", values.alternate);
        target.writeEntry("ForegroundNormal", values.normal);
        target.writeEntry("ForegroundInactive", values.inactive);
        target.writeEntry("ForegroundActive", values.active);
        target.writeEntry("ForegroundLink", values.link);
        target.writeEntry("ForegroundVisited", values.visited);
        target.writeEntry("ForegroundNegative", values.negative);
        target.writeEntry("ForegroundNeutral", values.neutral);
        target.writeEntry("ForegroundPositive", values.positive);
        target.writeEntry("DecorationFocus", values.focus);
        target.writeEntry("DecorationHover", values.hover);
    };
    write(u"Colors:View"_s, scheme.view);
    write(u"Colors:Window"_s, scheme.window);
    write(u"Colors:Button"_s, scheme.button);
    write(u"Colors:Selection"_s, scheme.selection);
    write(u"Colors:Tooltip"_s, scheme.tooltip);
    write(u"Colors:Header"_s, scheme.header);

    // Explicit effects: unfocused windows keep full contrast, disabled text fades toward the window.
    KConfigGroup disabled(&config, u"ColorEffects:Disabled"_s);
    disabled.writeEntry("Color", scheme.window.background);
    disabled.writeEntry("ColorAmount", 0.0);
    disabled.writeEntry("ColorEffect", 0);
    disabled.writeEntry("ContrastAmount", 0.65);
    disabled.writeEntry("ContrastEffect", 1);
    disabled.writeEntry("IntensityAmount", 0.1);
    disabled.writeEntry("IntensityEffect", 2);
    KConfigGroup inactive(&config, u"ColorEffects:Inactive"_s);
    inactive.writeEntry("ChangeSelectionColor", true);
    inactive.writeEntry("Color", scheme.window.inactive);
    inactive.writeEntry("ColorAmount", 0.0);
    inactive.writeEntry("ColorEffect", 0);
    inactive.writeEntry("ContrastAmount", 0.0);
    inactive.writeEntry("ContrastEffect", 0);
    inactive.writeEntry("Enable", false);
    inactive.writeEntry("IntensityAmount", 0.0);
    inactive.writeEntry("IntensityEffect", 0);
    return config.sync();
}
}
