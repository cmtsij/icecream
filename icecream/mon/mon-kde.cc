/*
    This file is part of Icecream.

    Copyright (c) 2003 Frerich Raabe <raabe@kde.org>
    Copyright (c) 2003,2004 Stephan Kulow <coolo@kde.org>
    Copyright (c) 2003,2004 Cornelius Schumacher <schumacher@kde.org>

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

#include "mon-kde.h"

#include "detailedhostview.h"
#include "ganttstatusview.h"
#include "hostinfo.h"
#include "hostview.h"
#include "listview.h"
#include "monitor.h"
#include "starview.h"
#include "summaryview.h"

#include <services/logging.h>

#include <kaboutdata.h>
#include <kaction.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstdaction.h>

MainWindow::MainWindow( QWidget *parent, const char *name )
  : KMainWindow( parent, name ), m_view( 0 )
{
    m_hostInfoManager = new HostInfoManager;

    m_monitor = new Monitor( m_hostInfoManager, this );

    KRadioAction *a = new KRadioAction( i18n( "&List View" ), 0,
                                        this, SLOT( setupListView() ),
                                        actionCollection(), "view_list_view" );
    a->setExclusiveGroup( "viewmode" );

    a = new KRadioAction( i18n( "&Star View" ), 0,
                          this, SLOT( setupStarView() ),
                          actionCollection(), "view_star_view" );
    a->setExclusiveGroup( "viewmode" );

    a = new KRadioAction( i18n( "&Gantt View" ), 0,
                          this, SLOT( setupGanttView() ),
                          actionCollection(), "view_gantt_view" );
    a->setExclusiveGroup( "viewmode" );

    a = new KRadioAction( i18n( "Summary &View" ), 0,
                          this, SLOT( setupSummaryView() ),
                          actionCollection(), "view_foo_view" );
    a->setExclusiveGroup( "viewmode" );

    a = new KRadioAction( i18n( "&Host View" ), 0,
                          this, SLOT( setupHostView() ),
                          actionCollection(), "view_host_view" );
    a->setExclusiveGroup( "viewmode" );

    a = new KRadioAction( i18n( "&Detailed Host View" ), 0,
                          this, SLOT( setupDetailedHostView() ),
                          actionCollection(), "view_detailed_host_view" );
    a->setExclusiveGroup( "viewmode" );

    KStdAction::quit( this, SLOT( close() ), actionCollection() );

    new KAction( i18n("Stop"), 0, this, SLOT( stopView() ), actionCollection(),
                 "view_stop" );

    new KAction( i18n("Start"), 0, this, SLOT( startView() ),
                 actionCollection(), "view_start" );

    new KAction( i18n("Check Nodes"), 0, this, SLOT( checkNodes() ),
                 actionCollection(), "check_nodes" );

    new KAction( i18n("Configure View..."), 0, this, SLOT( configureView() ),
                 actionCollection(), "configure_view" );

    createGUI();
    readSettings();
//    checkScheduler();

    setAutoSaveSettings();
}

MainWindow::~MainWindow()
{
  writeSettings();

  delete m_hostInfoManager;
}

void MainWindow::readSettings()
{
  KConfig *cfg = KGlobal::config();
  cfg->setGroup( "View" );
  QString viewId = cfg->readEntry( "CurrentView", "star" );
  if ( viewId == "gantt" ) setupGanttView();
  else if ( viewId == "list" ) setupListView();
  else if ( viewId == "star" ) setupStarView();
  else if ( viewId == "host" ) setupHostView();
  else if ( viewId == "detailedhost" ) setupDetailedHostView();
  else setupSummaryView();
}

void MainWindow::writeSettings()
{
  KConfig *cfg = KGlobal::config();
  cfg->setGroup( "View" );
  cfg->writeEntry( "CurrentView", m_view->id() );
}

void MainWindow::setupView( StatusView *view, bool rememberJobs )
{
  delete m_view;
  m_view = view;
  m_monitor->setCurrentView( m_view, rememberJobs );
  setCentralWidget( m_view->widget() );
  m_view->widget()->show();
}

void MainWindow::setupListView()
{
    setupView( new ListStatusView( m_hostInfoManager, this ), true );
}

void MainWindow::setupSummaryView()
{
    setupView( new SummaryView( m_hostInfoManager, this ), false );
    KAction* radioAction = actionCollection()->action( "view_foo_view" );
    if ( radioAction )
        dynamic_cast<KRadioAction*>( radioAction )->setChecked( true );
}

void MainWindow::setupGanttView()
{
    setupView( new GanttStatusView( m_hostInfoManager, this ), false );
    KAction* radioAction = actionCollection()->action( "view_gantt_view" );
    if ( radioAction )
        dynamic_cast<KRadioAction*>( radioAction )->setChecked( true );
}

void MainWindow::setupStarView()
{
    setupView( new StarView( m_hostInfoManager, this ), false );
    KAction* radioAction = actionCollection()->action( "view_star_view" );
    if ( radioAction )
        dynamic_cast<KRadioAction*>( radioAction )->setChecked( true );
}

void MainWindow::setupHostView()
{
    setupView( new HostView( true, m_hostInfoManager, this ), false );
    KAction* radioAction = actionCollection()->action( "view_host_view" );
    if ( radioAction )
        dynamic_cast<KRadioAction*>( radioAction )->setChecked( true );
}

void MainWindow::setupDetailedHostView()
{
    setupView( new DetailedHostView( m_hostInfoManager, this ), false );
    KAction* radioAction = actionCollection()->action( "view_detailed_host_view" );
    if ( radioAction )
        dynamic_cast<KRadioAction*>( radioAction )->setChecked( true );
}

void MainWindow::stopView()
{
  m_view->stop();
}

void MainWindow::startView()
{
  m_view->start();
}

void MainWindow::checkNodes()
{
  m_view->checkNodes();
}

void MainWindow::configureView()
{
  m_view->configureView();
}

void MainWindow::setCurrentNet( const QString &netName )
{
  m_monitor->setCurrentNet( netName );
}

const char * rs_program_name = "icemon";
const char * const appName = I18N_NOOP( "Icecream Monitor" );
const char * const version = "0.1";
const char * const description = I18N_NOOP( "Icecream monitor for KDE" );
const char * const copyright = I18N_NOOP( "(c) 2003,2004, The icecream developers" );

static const KCmdLineOptions options[] =
{
  { "n", 0, 0 },
  { "netname <name>", "Icecream network name", 0 },
  KCmdLineLastOption
};

int main( int argc, char **argv )
{
  setup_debug(Debug|Info|Warning|Error,"");
  KAboutData aboutData( rs_program_name, appName, version, description,
	                KAboutData::License_BSD, copyright );
  aboutData.addAuthor( "Frerich Raabe", 0, "raabe@kde.org" );
  aboutData.addAuthor( "Stephan Kulow", 0, "coolo@kde.org" );
  aboutData.addAuthor( "Cornelius Schumacher", 0, "schumacher@kde.org" );

  KCmdLineArgs::init( argc, argv, &aboutData );
  KCmdLineArgs::addCmdLineOptions( options );

  KApplication app;
  MainWindow *mainWidget = new MainWindow( 0 );

  KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
  QString netName = QString::fromLocal8Bit( args->getOption( "netname" ) );
  if ( !netName.isEmpty() ) {
    mainWidget->setCurrentNet( netName );
  }

  app.setMainWidget( mainWidget );
  mainWidget->show();

  return app.exec();
}

#include "mon-kde.moc"
