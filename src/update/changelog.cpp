#include "changelog.hpp"
#include "updater.hpp"

#include <algorithm>

namespace acsl {

QList<ChangelogEntry> parseChangelog(const QString &text)
{
	QList<ChangelogEntry> out;
	for (QString line : text.split(QLatin1Char('\n'))) {
		line = line.trimmed();
		if (line.startsWith(QLatin1String("##"))) {
			ChangelogEntry e;
			e.version = line.mid(2).trimmed();
			if (e.version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
				e.version = e.version.mid(1);
			if (!e.version.isEmpty())
				out.append(e);
		} else if (line.startsWith(QLatin1String("- ")) && !out.isEmpty()) {
			out.last().items.append(line.mid(2).trimmed());
		}
	}
	return out;
}

QList<ChangelogEntry> changesSince(const QList<ChangelogEntry> &all, const QString &lastSeen, const QString &current, int max)
{
	QList<ChangelogEntry> out;
	for (const ChangelogEntry &e : all) {
		if (e.items.isEmpty() || Updater::compareVersions(e.version, current) > 0)
			continue; // vacia, o de una version posterior a la instalada
		if (lastSeen.isEmpty() ? Updater::compareVersions(e.version, current) == 0 : Updater::compareVersions(e.version, lastSeen) > 0)
			out.append(e);
	}
	std::sort(out.begin(), out.end(), [](const ChangelogEntry &a, const ChangelogEntry &b) {
		return Updater::compareVersions(a.version, b.version) > 0;
	});
	while (out.size() > max)
		out.removeLast();
	return out;
}

} // namespace acsl
