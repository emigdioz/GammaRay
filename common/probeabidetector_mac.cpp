/*
  probeabidetector_mac.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config-gammaray.h"

#include "probeabidetector.h"
#include "probeabi.h"

#include <QDebug>
#include <QProcess>
#include <QString>

#include <mach-o/loader.h>

using namespace GammaRay;

static QString qtCoreFromOtool(const QString &path)
{
  QProcess proc;
  proc.setProcessChannelMode(QProcess::SeparateChannels);
  proc.setReadChannel(QProcess::StandardOutput);
  proc.start("otool", QStringList() << "-L" << path);
  proc.waitForFinished();

  forever {
    const QByteArray line = proc.readLine();
    if (line.isEmpty())
      break;

    if (line.contains("QtCore") || line.contains("Qt5Core")) {
      const int pos = line.lastIndexOf(" (");
      if (pos <= 0)
        continue;
      return QString::fromLocal8Bit(line.left(pos).trimmed());
    }
  }

  return QString();
}

ProbeABI ProbeABIDetector::abiForExecutable(const QString& path) const
{
  const QString qtCorePath = qtCoreFromOtool(path);
  if (qtCorePath.isEmpty())
    return ProbeABI();

  return abiForQtCore(qtCorePath);
}


static QString qtCoreFromLsof(qint64 pid)
{
  QProcess proc;
  proc.setProcessChannelMode(QProcess::SeparateChannels);
  proc.setReadChannel(QProcess::StandardOutput);
  proc.start("lsof", QStringList() << "-Fn" << "-n" << "-p" << QString::number(pid));
  proc.waitForFinished();

  forever {
    const QByteArray line = proc.readLine();
    if (line.isEmpty())
      break;

    if (line.contains("QtCore") || line.contains("Qt5Core")) {
      return QString::fromLocal8Bit(line.mid(1).trimmed()); // strip the field identifier
    }
  }

  return QString();
}

ProbeABI ProbeABIDetector::abiForProcess(qint64 pid) const
{
  const QString qtCorePath = qtCoreFromLsof(pid);
  if (qtCorePath.isEmpty())
    return ProbeABI();

  return abiForQtCore(qtCorePath);
}

ProbeABI ProbeABIDetector::detectAbiForQtCore(const QString& path) const
{
  qWarning() << path;
  return ProbeABI::fromString(GAMMARAY_PROBE_ABI);
}
