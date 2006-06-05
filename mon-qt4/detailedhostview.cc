/*
    This file is part of Icecream.

    Copyright (c) 2004 Andre Wöbbeking <Woebbeking@web.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "detailedhostview.h"

#include "hostinfo.h"
#include "hostlistview.h"
#include "joblistview.h"

#include <qlabel.h>
#include <qlayout.h>
#include <qsplitter.h>
#include <q3vbox.h>
#include <Q3VBoxLayout>

#include <sys/utsname.h>


static QString myHostName()
{
    struct utsname uname_buf;
    if ( ::uname( &uname_buf ) == 0 )
        return uname_buf.nodename;
    else
        return QString::null;
}

#define i18n

DetailedHostView::DetailedHostView( HostInfoManager* manager,
                                    QWidget* parent,
                                    const char* name )
    : QWidget( parent, name ),
      StatusView( manager )
{
  Q3BoxLayout* topLayout = new Q3VBoxLayout( this );
  topLayout->setMargin( 10 );

  QSplitter* viewSplitter = new QSplitter( Qt::Vertical, this );
  topLayout->addWidget( viewSplitter );

  Q3VBox* hosts = new Q3VBox( viewSplitter );
  hosts->setSpacing( 10 );

  new QLabel( i18n( "Hosts" ), hosts );
  mHostListView = new HostListView( manager, hosts, "HostListView" );

  Q3VBox* locals = new Q3VBox( viewSplitter );
  locals->setSpacing( 10 );

  new QLabel( i18n( "Outgoing jobs" ), locals );
  mLocalJobsView = new JobListView( manager, locals, "LocalJobs" );
  mLocalJobsView->setClientColumnVisible( false );
  mLocalJobsView->setExpireDuration( 5 );

  Q3VBox* remotes = new Q3VBox( viewSplitter );
  remotes->setSpacing( 10 );

  new QLabel( i18n( "Incoming jobs" ), remotes );
  mRemoteJobsView = new JobListView( manager, remotes, "RemoteJobs" );
  mRemoteJobsView->setServerColumnVisible( false );
  mRemoteJobsView->setExpireDuration( 5 );

  connect(mHostListView, SIGNAL( nodeActivated( unsigned int ) ),
          this, SLOT( slotNodeActivated() ) );
}


void DetailedHostView::update( const Job &job )
{
    const unsigned int hostid = mHostListView->activeNode();

    if ( !hostid )
        return;

    if ( job.client() != hostid && job.server() != hostid )
        return;

    if ( job.client() == hostid )
        mLocalJobsView->update( job );
    if ( job.server() == hostid )
        mRemoteJobsView->update( job );
}


void DetailedHostView::checkNode( unsigned int hostid )
{
    if ( !hostid )
        return;

    mHostListView->checkNode( hostid );

    const unsigned int activeNode = mHostListView->activeNode();

    if ( !activeNode )
    {
        HostInfo* info = hostInfoManager()->find( hostid );
        if ( info->name() == myHostName() )
            mHostListView->setActiveNode( hostid );
    }
}


void DetailedHostView::removeNode( unsigned int hostid )
{
    mHostListView->removeNode( hostid );
}


void DetailedHostView::updateSchedulerState( bool online )
{
    if ( !online )
    {
        mHostListView->clear();
        mLocalJobsView->clear();
        mRemoteJobsView->clear();
    }
}


void DetailedHostView::slotNodeActivated()
{
    mLocalJobsView->clear();
    mRemoteJobsView->clear();
}


QWidget* DetailedHostView::widget()
{
  return this;
}
