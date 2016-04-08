/*
 * This file is part of GammaRay, the Qt application inspection and
 * manipulation tool.
 *
 * Copyright (C) 2014-2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
 * Author: Filipe Azevedo <filipe.azevedo@kdab.com>
 *
 * Licensees holding valid commercial KDAB GammaRay licenses may use this file in
 * accordance with GammaRay Commercial License Agreement provided with the Software.
 *
 * Contact info@kdab.com if any conditions of this licensing are not clear to you.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config-gammaray.h>

#include "deferredtreeview.h"

#include <QTimer>

#if defined(HAVE_PRIVATE_QT_HEADERS)
#include <private/qheaderview_p.h>
#endif

using namespace GammaRay;

namespace {
QHeaderView::ResizeMode sectionResizeMode(QHeaderView *header, int logicalIndex)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
  return header->sectionResizeMode(logicalIndex);
#else
  return header->resizeMode(logicalIndex);
#endif
}

void setSectionResizeMode(QHeaderView *header, int logicalIndex, QHeaderView::ResizeMode mode)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
  header->setSectionResizeMode(logicalIndex, mode);
#else
  header->setResizeMode(logicalIndex, mode);
#endif
}
}

HeaderView::HeaderView(Qt::Orientation orientation, QWidget *parent)
  : QHeaderView(orientation, parent)
{
}

bool HeaderView::isState(State state) const
{
#if defined(HAVE_PRIVATE_QT_HEADERS)
  QHeaderViewPrivate *d = reinterpret_cast<QHeaderViewPrivate *>(d_ptr.data());
  return d->state == QHeaderViewPrivate::State(state);
#else
  Q_UNUSED(state);
  return false;
#endif
}

DeferredTreeView::DeferredHeaderProperties::DeferredHeaderProperties()
  : initialized(false)
  , resizeMode(-1)
  , hidden(-1)
{
}

DeferredTreeView::DeferredTreeView(QWidget *parent)
  : QTreeView(parent)
  , m_expandNewContent(false)
  , m_allExpanded(false)
  , m_timer(new QTimer(this))
{
  m_timer->setSingleShot(true);
  m_timer->setInterval(125);

  setHeader(new HeaderView(header()->orientation(), this));

  // Default QTreeView header properties
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
  header()->setSectionsMovable(true);
#else
  header()->setMovable(true);
#endif
  header()->setStretchLastSection(true);
  header()->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
  // Custom
  header()->setSortIndicatorShown(true);

  setIndentation(10);
  setAllColumnsShowFocus(true);
  setUniformRowHeights(true);
  setSortingEnabled(true);

  connect(header(), SIGNAL(sectionCountChanged(int,int)), SLOT(sectionCountChanged()));
  connect(m_timer, SIGNAL(timeout()), this, SLOT(timeout()));
}

void DeferredTreeView::setModel(QAbstractItemModel *model)
{
  QTreeView::setModel(model);

  if (model) {
    triggerExpansion(QModelIndex());
  }
}

QHeaderView::ResizeMode DeferredTreeView::deferredResizeMode(int logicalIndex) const
{
  const auto it = m_sectionsProperties.constFind(logicalIndex);
  const int resizeMode = it != m_sectionsProperties.constEnd() ? (*it).resizeMode : -1;
  return resizeMode != -1 ? QHeaderView::ResizeMode(resizeMode) : sectionResizeMode(header(), logicalIndex);
}

void DeferredTreeView::setDeferredResizeMode(int logicalIndex, QHeaderView::ResizeMode mode)
{
  auto it = m_sectionsProperties.find(logicalIndex);

  if (it != m_sectionsProperties.end()) {
    (*it).resizeMode = mode;
  } else {
    DeferredHeaderProperties properties;
    properties.resizeMode = mode;
    m_sectionsProperties[logicalIndex] = properties;
  }

  if (logicalIndex < header()->count()) {
    setSectionResizeMode(header(), logicalIndex, mode);
    m_sectionsProperties[logicalIndex].initialized = true;
  }
}

bool DeferredTreeView::deferredHidden(int logicalIndex) const
{
  const auto it = m_sectionsProperties.constFind(logicalIndex);
  const int hidden = it != m_sectionsProperties.constEnd() ? (*it).hidden : -1;
  return hidden != -1 ? hidden == 1 : header()->isSectionHidden(logicalIndex);
}

void DeferredTreeView::setDeferredHidden(int logicalIndex, bool hidden)
{
  auto it = m_sectionsProperties.find(logicalIndex);

  if (it != m_sectionsProperties.end()) {
    (*it).hidden = hidden;
  } else {
    DeferredHeaderProperties properties;
    properties.hidden = hidden ? 1 : 0;
    m_sectionsProperties[logicalIndex] = properties;
  }

  if (logicalIndex < header()->count()) {
    header()->setSectionHidden(logicalIndex, hidden);
    m_sectionsProperties[logicalIndex].initialized = true;
  }
}

bool DeferredTreeView::expandNewContent() const
{
  return m_expandNewContent;
}

void DeferredTreeView::setExpandNewContent(bool expand)
{
  m_expandNewContent = expand;
}

bool DeferredTreeView::stretchLastSection() const
{
  return header()->stretchLastSection();
}

void DeferredTreeView::setStretchLastSection(bool stretch)
{
  header()->setStretchLastSection(stretch);
}

void DeferredTreeView::resetDeferredInitialized()
{
  for (auto it = m_sectionsProperties.begin(), end = m_sectionsProperties.end(); it != end; ++it) {
    (*it).initialized = false;
  }
}

void DeferredTreeView::rowsInserted(const QModelIndex &parent, int start, int end)
{
  QTreeView::rowsInserted(parent, start, end);
  triggerExpansion(parent);
}

void DeferredTreeView::sectionCountChanged()
{
  const int sections = header()->count();

  if (sections == 0) {
    resetDeferredInitialized();
    return;
  }

  for (auto it = m_sectionsProperties.begin(), end = m_sectionsProperties.end(); it != end; ++it) {
    if ((*it).initialized) {
      continue;
    }

    if (it.key() < sections) {
      if ((*it).resizeMode != -1) {
        setSectionResizeMode(header(), it.key(), QHeaderView::ResizeMode((*it).resizeMode));
      }

      if ((*it).hidden) {
        header()->setSectionHidden(it.key(), (*it).hidden == 1);
      }

      (*it).initialized = true;
    }
  }
}

void DeferredTreeView::triggerExpansion(const QModelIndex &parent)
{
  if (m_expandNewContent) {
    m_insertedRows << QPersistentModelIndex(parent);
    m_timer->start();
  }
}

void DeferredTreeView::timeout()
{
  const QModelIndex selectedRow = selectionModel()->selectedRows().value(0);

  if (m_allExpanded) {
    for (auto it = m_insertedRows.constBegin(), end = m_insertedRows.constEnd(); it != end; ++it) {
      if (it->isValid()) {
        expand(*it);
      }
    }
  } else {
    m_allExpanded = true;
    expandAll();
  }

  m_insertedRows.clear();

  if (selectedRow.isValid()) {
    scrollTo(selectedRow);
  }

  emit newContentExpanded();
}