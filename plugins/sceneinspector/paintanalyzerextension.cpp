/*
  paintanalyzerextension.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

  Licensees holding valid commercial KDAB GammaRay licenses may use this file in
  accordance with GammaRay Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

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

#include "paintanalyzerextension.h"

#include <core/propertycontroller.h>
#include <core/paintanalyzer.h>
#include <core/metaobjectrepository.h>
#include <core/metaobject.h>

#include <QDebug>
#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QStyleOptionGraphicsItem>
#include <QPainter>

using namespace GammaRay;

PaintAnalyzerExtension::PaintAnalyzerExtension(PropertyController* controller):
    PropertyControllerExtension(controller->objectBaseName() + ".qgvPainting"),
    m_paintAnalyzer(new PaintAnalyzer(controller->objectBaseName() + ".qgvPainting.analyzer", controller))
{
}

PaintAnalyzerExtension::~PaintAnalyzerExtension()
{
}

bool PaintAnalyzerExtension::setQObject(QObject *object)
{
    if (!PaintAnalyzer::isAvailable())
        return false;

    if (auto qgvObj = qobject_cast<QGraphicsObject*>(object)) {
        analyzePainting(qgvObj);
        return true;
    }

    return false;
}

bool PaintAnalyzerExtension::setObject(void *object, const QString &typeName)
{
    if (!PaintAnalyzer::isAvailable())
        return false;

    const auto mo = MetaObjectRepository::instance()->metaObject(typeName);
    if (!mo)
        return false;
    if (const auto item = mo->castTo(object, QStringLiteral("QGraphicsItem"))) {
        analyzePainting(static_cast<QGraphicsItem*>(item));
        return true;
    }
    return false;
}

void PaintAnalyzerExtension::analyzePainting(QGraphicsItem* item)
{
    m_paintAnalyzer->beginAnalyzePainting();
    m_paintAnalyzer->setBoundingRect(item->boundingRect());

    QStyleOptionGraphicsItem option;
    option.state = QStyle::State_None;
    option.rect = item->boundingRect().toRect();
    option.levelOfDetail = 1;
    option.exposedRect = item->boundingRect();

#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
    option.styleObject = item->toGraphicsObject();
    if (!option.styleObject)
        option.styleObject = item->scene();
#endif

    if (item->isSelected())
        option.state |= QStyle::State_Selected;
    if (item->isEnabled())
        option.state |= QStyle::State_Enabled;
    if (item->hasFocus())
        option.state |= QStyle::State_HasFocus;

    {
        QPainter p(m_paintAnalyzer->paintDevice());
        item->paint(&p, &option);
    }
    m_paintAnalyzer->endAnalyzePainting();
}