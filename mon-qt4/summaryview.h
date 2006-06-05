/*
 * (C) 2004 Scott Wheeler <wheeler@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef SUMMARYVIEW_H
#define SUMMARYVIEW_H

#include "statusview.h"

#include <q3scrollview.h>
#include <q3valuevector.h>
//Added by qt3to4:
#include <QResizeEvent>
#include <Q3Frame>
#include <Q3GridLayout>
#include <Q3ValueList>
#include <QLabel>

class QLabel;
class Q3GridLayout;

class SummaryView;

class SummaryViewItem
{
public:
    SummaryViewItem(unsigned int hostid, QWidget *parent, SummaryView *view, Q3GridLayout *layout);
    ~SummaryViewItem();
    void update(const Job &job);

private:
    QLabel *addLine(const QString &caption, QWidget *parent, Q3GridLayout *grid,
                                int flags = Qt::AlignTop,
                                const QString &status = QString::null);

    struct JobHandler
    {
        JobHandler() : stateWidget(0), sourceLabel(0), stateLabel(0) {}

        Q3Frame *stateWidget;
        QLabel *sourceLabel;
        QLabel *stateLabel;
        QString currentFile;
    };

    Q3Frame *m_stateWidget;
    QLabel *m_jobsLabel;
    QLabel *m_sourceLabel;

    int m_jobCount;
    SummaryView *m_view;

    Q3ValueVector<JobHandler> m_jobHandlers;
    Q3ValueList<QWidget *> m_widgets;
};

class SummaryView : public Q3ScrollView, public StatusView
{
    Q_OBJECT

public:
    SummaryView(HostInfoManager *h, QWidget *parent, const char *name = 0);
    ~SummaryView();

    virtual QWidget *widget();
    virtual void update(const Job &job);
    virtual void checkNode(unsigned int hostid);
    virtual QString id() const { return "summary"; }

protected:
    virtual void viewportResizeEvent(QResizeEvent *e);

private:
    QMap<unsigned int, SummaryViewItem *> m_items;
    Q3GridLayout *m_layout;
    QWidget *m_base;
};

#endif
