// Novedades de cada version (data/novedades.txt): que mostrar despues de actualizar.
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace acsl {

struct ChangelogEntry {
	QString version;
	QStringList items;
};

// "## 1.8.0" abre una version y cada "- texto" es una novedad; lo demas se ignora.
QList<ChangelogEntry> parseChangelog(const QString &text);

// Lo que hay que ensenar a quien vio por ultima vez lastSeen y ahora tiene current: las
// versiones posteriores a lastSeen hasta current, de la mas nueva a la mas vieja. Si nunca
// vio ninguna (lastSeen vacio), solo la actual.
QList<ChangelogEntry> changesSince(const QList<ChangelogEntry> &all, const QString &lastSeen, const QString &current, int max = 4);

} // namespace acsl
