/*
    This file is part of Icecream.

    Copyright (c) 2003 Frerich Raabe <raabe@kde.org>
    Copyright (c) 2004 Cornelius Schumacher <schumacher@kde.org>

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

#include "starview.h"

#include "hostinfo.h"

#include <klocale.h>
#include <kdebug.h>
#include <kiconloader.h>
#include <kdialog.h>

#include <qlayout.h>
#include <q3valuelist.h>
#include <qtooltip.h>
#include <qslider.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qregexp.h>
#include <qcheckbox.h>
//Added by qt3to4:
#include <Q3HBoxLayout>
#include <Q3Frame>
#include <QResizeEvent>
#include <Q3VBoxLayout>

#include <math.h>

static bool suppressDomain = false;

StarViewConfigDialog::StarViewConfigDialog( QWidget *parent )
  : QDialog( parent )
{
  Q3BoxLayout *topLayout = new Q3VBoxLayout( this );
  topLayout->setMargin( KDialog::marginHint() );
  topLayout->setSpacing( KDialog::spacingHint() );

  QLabel *label = new QLabel( i18n("Number of nodes per ring:"), this );
  topLayout->addWidget( label );

  Q3BoxLayout *nodesLayout = new Q3HBoxLayout( topLayout );

  int nodesPerRing = 25;

  mNodesPerRingSlider = new QSlider( 1, 50, 1, nodesPerRing, Qt::Horizontal, this );
  nodesLayout->addWidget( mNodesPerRingSlider );
  connect( mNodesPerRingSlider, SIGNAL( valueChanged( int ) ),
           SIGNAL( configChanged() ) );
  connect( mNodesPerRingSlider, SIGNAL( valueChanged( int ) ),
           SLOT( slotNodesPerRingChanged( int ) ) );

  mNodesPerRingLabel = new QLabel( QString::number( nodesPerRing ), this );
  nodesLayout->addWidget( mNodesPerRingLabel );

  label = new QLabel( i18n("Architecture filter:"), this );
  topLayout->addWidget( label );
  mArchFilterEdit = new QLineEdit( this );
  topLayout->addWidget( mArchFilterEdit );
  connect( mArchFilterEdit, SIGNAL( textChanged( const QString & ) ),
           SIGNAL( configChanged() ) );

  mSuppressDomainName = new QCheckBox( i18n("Suppress domain name"), this);
  topLayout->addWidget( mSuppressDomainName );
  connect( mSuppressDomainName, SIGNAL( toggled ( bool ) ),
           SLOT( slotSuppressDomainName ( bool ) ) );

  Q3Frame *hline = new Q3Frame( this );
  hline->setFrameShape( Q3Frame::HLine );
  topLayout->addWidget( hline );

  Q3BoxLayout *buttonLayout = new Q3HBoxLayout( topLayout );

  buttonLayout->addStretch( 1 );

  QPushButton *button = new QPushButton( i18n("&Close"), this );
  buttonLayout->addWidget( button );
  connect( button, SIGNAL( clicked() ), SLOT( hide() ) );
}

void StarViewConfigDialog::slotNodesPerRingChanged( int nodes )
{
  mNodesPerRingLabel->setText( QString::number( nodes ) );
}

void StarViewConfigDialog::setMaxNodes( int maxNodes )
{
  mNodesPerRingSlider->setMaxValue( maxNodes + 1 );
}

int StarViewConfigDialog::nodesPerRing()
{
  return mNodesPerRingSlider->value();
}

QString StarViewConfigDialog::archFilter()
{
  return mArchFilterEdit->text();
}

void StarViewConfigDialog::slotSuppressDomainName( bool b )
{
  suppressDomain = b;
  configChanged();
}


HostItem::HostItem( const QString &text, Q3Canvas *canvas, HostInfoManager *m )
  : Q3CanvasText( text, canvas ), mHostInfo( 0 ), mHostInfoManager( m ),
    m_stateItem( 0 )
{
  init();

  updateName();
}

HostItem::HostItem( HostInfo *hostInfo, Q3Canvas *canvas, HostInfoManager *m )
  : Q3CanvasText( canvas ), mHostInfo( hostInfo ),
    mHostInfoManager( m ), m_stateItem( 0 )
{
  init();
}

HostItem::~HostItem()
{
}

void HostItem::init()
{
  setZ( 100 );

  mBaseWidth = 0;
  mBaseHeight = 0;

  m_boxItem = new Q3CanvasEllipse( canvas() );
  m_boxItem->setZ( 80 );
  m_boxItem->show();

  setHostColor( QColor( 200, 200, 200 ) );

  mIsActiveClient = false;
  mIsCompiling = false;

  m_client = 0;
}

void HostItem::deleteSubItems()
{
  delete m_boxItem;

  QMap<Job,Q3CanvasEllipse *>::ConstIterator it;
  for( it = m_jobHalos.begin(); it != m_jobHalos.end(); ++it ) {
    delete it.data();
  }
  m_jobHalos.clear();

  delete m_stateItem;
}

void HostItem::setHostColor( const QColor &color )
{
  m_boxItem->setBrush( color );

  setColor( StatusView::textColor( color ) );
}

QString HostItem::hostName() const
{
  return mHostInfo->name();
}

void HostItem::updateName()
{
  if (mHostInfo) {
      QString s = mHostInfo->name();
      if (suppressDomain) {
          int l = s.find('.');
          if (l>0)
              s.truncate(l);
      }
      setText(s);
  }

  QRect r = boundingRect();
  mBaseWidth = r.width() + 10 ;
  mBaseHeight = r.height() + 10 ;

  // don't move the sub items
  Q3CanvasText::moveBy( centerPosX() - x() - r.width() / 2,
                       centerPosY() - y() - r.height() / 2 );

  m_boxItem->setSize( mBaseWidth, mBaseHeight );

  updateHalos();
}

void HostItem::moveBy( double dx, double dy )
{
  Q3CanvasText::moveBy( dx, dy );

  m_boxItem->moveBy( dx, dy );

  QMap<Job,Q3CanvasEllipse*>::ConstIterator it;
  for( it = m_jobHalos.begin(); it != m_jobHalos.end(); ++it ) {
    it.data()->moveBy( dx, dy );
  }
}

void HostItem::setCenterPos( double x, double y )
{
    // move all items (also the sub items)
    moveBy( x - centerPosX(), y - centerPosY() );
}

void HostItem::update( const Job &job )
{
  setIsCompiling( job.state() == Job::Compiling );
  setClient( job.client() );

  if ( job.state() == Job::WaitingForCS ) return;

  bool finished = job.state() == Job::Finished ||
                  job.state() == Job::Failed;

  JobList::Iterator it = m_jobs.find( job.jobId() );
  bool newJob = ( it == m_jobs.end() );

  if ( newJob && finished ) return;
  if ( !newJob && !finished ) return;

  if ( newJob ) {
    m_jobs.insert( job.jobId(), job );
    createJobHalo( job );
  } else if ( finished ) {
    deleteJobHalo( job );
    m_jobs.remove( it );
  }
}

void HostItem::createJobHalo( const Job &job )
{
  Q3CanvasEllipse *halo = new Q3CanvasEllipse( mBaseWidth, mBaseHeight,
                                             canvas() );
  halo->setZ( 70 - m_jobHalos.size() );
  halo->move( centerPosX(), centerPosY() );
  halo->show();

  m_jobHalos.insert( job, halo );

  updateHalos();
}

void HostItem::deleteJobHalo( const Job &job )
{
  QMap<Job,Q3CanvasEllipse*>::Iterator it = m_jobHalos.find( job );
  if ( it == m_jobHalos.end() ) return;

  Q3CanvasEllipse *halo = *it;
  delete halo;
  m_jobHalos.remove( it );

  updateHalos();
}

void HostItem::updateHalos()
{
  int count = 1;

  QMap<Job,Q3CanvasEllipse*>::Iterator it;
  for( it = m_jobHalos.begin(); it != m_jobHalos.end(); ++it ) {
    Q3CanvasEllipse *halo = it.data();
    halo->setSize( mBaseWidth + count * 6, mBaseHeight + count * 6 );
    halo->setBrush( mHostInfoManager->hostColor( it.key().client() ) );
    ++count;
  }
}

StarView::StarView( HostInfoManager *m, QWidget *parent, const char *name )
  : QWidget( parent, name, Qt::WNoAutoErase | Qt::WResizeNoErase ), StatusView( m )
{
    mConfigDialog = new StarViewConfigDialog( this );
    connect( mConfigDialog, SIGNAL( configChanged() ),
             SLOT( slotConfigChanged() ) );

    m_canvas = new Q3Canvas( this );
    m_canvas->resize( width(), height() );

    Q3HBoxLayout *layout = new Q3HBoxLayout( this );
    layout->setMargin( 0 );

    m_canvasView = new Q3CanvasView( m_canvas, this );
    m_canvasView->setVScrollBarMode( Q3ScrollView::AlwaysOff );
    m_canvasView->setHScrollBarMode( Q3ScrollView::AlwaysOff );
    layout->addWidget( m_canvasView );

    m_schedulerItem = new HostItem( "", m_canvas, hostInfoManager() );
    centerSchedulerItem();
    m_schedulerItem->show();

    createKnownHosts();
}

void StarView::update( const Job &job )
{
#if 0
  kdDebug() << "StarView::update() " << job.jobId()
            << " server: " << job.server() << " client: " << job.client()
            << " state: " << job.stateAsString() << endl;
#endif

  if ( job.state() == Job::WaitingForCS ) {
    drawNodeStatus();
    m_canvas->update();
    return;
  }

  bool finished = job.state() == Job::Finished || job.state() == Job::Failed;

  QMap<unsigned int,HostItem *>::Iterator it;
  it = mJobMap.find( job.jobId() );
  if ( it != mJobMap.end() ) {
    (*it)->update( job );
    if ( finished ) {
      mJobMap.remove( it );
      unsigned int clientid = job.client();
      HostItem *clientItem = findHostItem( clientid );
      if ( clientItem ) clientItem->setIsActiveClient( false );
    }
    drawNodeStatus();
    m_canvas->update();
    return;
  }

  unsigned int hostid = processor( job );
  if ( !hostid ) {
    kdDebug() << "Empty host" << endl;
    return;
  }

  HostItem *hostItem = findHostItem( hostid );
  if ( !hostItem ) return;

  hostItem->update( job );

  if ( !finished ) mJobMap.insert( job.jobId(), hostItem );

  if ( job.state() == Job::Compiling ) {
    unsigned int clientid = job.client();
    HostItem *clientItem = findHostItem( clientid );
    if ( clientItem ) {
      clientItem->setClient( clientid );
      clientItem->setIsActiveClient( true );
    }
  }

  drawNodeStatus();
  m_canvas->update();
}

HostItem *StarView::findHostItem( unsigned int hostid )
{
  HostItem *hostItem = 0;
  QMap<unsigned int, HostItem*>::iterator it = m_hostItems.find( hostid );
  if ( it != m_hostItems.end() ) hostItem = it.data();
  return hostItem;
}

void StarView::checkNode( unsigned int hostid )
{
//  kdDebug() << "StarView::checkNode() " << hostid << endl;

  if ( !hostid ) return;

  if ( !filterArch( hostid ) ) return;

  HostItem *hostItem = findHostItem( hostid );
  if ( !hostItem ) {
    hostItem = createHostItem( hostid );
    arrangeHostItems();
    drawNodeStatus();
  }
}

void StarView::removeNode( unsigned int hostid )
{
//  kdDebug() << "StarView::removeNode() " << hostid << endl;

  HostItem *hostItem = findHostItem( hostid );

  if ( hostItem && hostItem->hostInfo()->isOffline() ) {
    removeItem( hostItem );
  }
}

void StarView::forceRemoveNode( unsigned int hostid )
{
  HostItem *hostItem = findHostItem( hostid );

  if ( hostItem ) {
    removeItem( hostItem );
  }
}

void StarView::removeItem( HostItem *hostItem )
{
#if 0
  kdDebug() << "StarView::removeItem() " << hostid << " ("
            << int( hostItem ) << ")" << endl;
#endif

  m_hostItems.remove( hostItem->hostInfo()->id() );

  Q3ValueList<unsigned int> obsoleteJobs;

  QMap<unsigned int,HostItem *>::Iterator it;
  for( it = mJobMap.begin(); it != mJobMap.end(); ++it ) {
#if 0
    kdDebug() << " JOB: " << it.key() << " (" << int( it.data() )
              << ")" << endl;
#endif
    if ( it.data() == hostItem ) {
#if 0
      kdDebug() << " Delete Job " << it.key() << endl;
#endif
      obsoleteJobs.append( it.key() );
    }
  }

  Q3ValueList<unsigned int>::ConstIterator it2;
  for( it2 = obsoleteJobs.begin(); it2 != obsoleteJobs.end(); ++it2 ) {
    mJobMap.remove( *it2 );
  }

  hostItem->deleteSubItems();
  delete hostItem;

  arrangeHostItems();
  drawNodeStatus();

  m_canvas->update();
}

void StarView::updateSchedulerState( bool online )
{
  QString txt;
  if ( online ) {
    txt = i18n("Scheduler");
  } else {
    txt = "";
  }
  m_schedulerItem->deleteSubItems();
  delete m_schedulerItem;

  if ( !online ) {
    QMap<unsigned int,HostItem *>::ConstIterator it;
    for( it = m_hostItems.begin(); it != m_hostItems.end(); ++it ) {
      (*it)->deleteSubItems();
      delete *it;
    }
    m_hostItems.clear();
    mJobMap.clear();
  }

  m_schedulerItem = new HostItem( txt, m_canvas, hostInfoManager() );
  m_schedulerItem->show();
  centerSchedulerItem();
  m_canvas->update();
}

QWidget *StarView::widget()
{
  return this;
}

void StarView::resizeEvent( QResizeEvent * )
{
    m_canvas->resize( width(), height() );
    centerSchedulerItem();
    arrangeHostItems();
    drawNodeStatus();
    m_canvas->update();
}

bool StarView::event ( QEvent* e )
{
    if (e->type() != QEvent::ToolTip) return QWidget::event(e);

    QPoint p ( static_cast<QHelpEvent*>(e)->pos());

    HostItem *item = 0;
    Q3CanvasItemList items = m_canvas->collisions( p );
    Q3CanvasItemList::ConstIterator it;
    for( it = items.begin(); it != items.end(); ++it ) {
        if ( (*it)->rtti() == HostItem::RttiHostItem ) {
            item = static_cast<HostItem *>( *it );
            break;
        }
    }
    if ( item ) {
        HostInfo *hostInfo = item->hostInfo();
        if ( !hostInfo ) return QWidget::event(e);

        QPoint gp( static_cast<QHelpEvent*>(e)->globalPos());
        QToolTip::showText(QPoint(gp.x() - 20, gp.y() - 20),
                "<p><table><tr><td>"
                "<img source=\"computer\"><br><b>" + item->hostName() +
                "</b><br>" +

                "<table>" +
                "<tr><td>" + i18n("IP:") + "</td><td>" + hostInfo->ip()
                + "</td></tr>" +
                "<tr><td>" + i18n("Platform:") + "</td><td>" +
                hostInfo->platform() + "</td></tr>"
                "<tr><td>" + i18n("Flavor:") + "</td><td>" +
                HostInfo::colorName( hostInfo->color() ) + "</td></tr>" +
                "<tr><td>" + i18n("Id:") + "</td><td>" +
                QString::number( hostInfo->id() ) + "</td></tr>" +
                "<tr><td>" + i18n("Speed:") + "</td><td>" +
                QString::number( hostInfo->serverSpeed() ) + "</td></tr>" +
                "</table>"

                "</td></tr></table></p>" );
    }
    return QWidget::event(e);
}

void StarView::centerSchedulerItem()
{
    m_schedulerItem->setCenterPos( width() / 2, height() / 2 );
}

void StarView::slotConfigChanged()
{
//  kdDebug() << "StarView::slotConfigChanged()" << endl;

  HostInfoManager::HostMap hostMap = hostInfoManager()->hostMap();
  HostInfoManager::HostMap::ConstIterator it;
  for( it = hostMap.begin(); it != hostMap.end(); ++it ) {
    if ( filterArch( *it ) ) checkNode( it.key() );
    else forceRemoveNode( it.key() );
  }

  arrangeHostItems();
  drawNodeStatus();
  m_canvas->update();
}

void StarView::arrangeHostItems()
{
//  kdDebug() << "StarView::arrangeHostItems()" << endl;

  int count = m_hostItems.count();

//  kdDebug() << "  Count: " << count << endl;

  int nodesPerRing = mConfigDialog->nodesPerRing();

  int ringCount = int( count / nodesPerRing ) + 1;

//  kdDebug() << "  Rings: " << ringCount << endl;
  double radiusFactor = 2.5;
  if (suppressDomain) radiusFactor = 4;
  const int xRadius = qRound( m_canvas->width() / radiusFactor );
  const int yRadius = qRound( m_canvas->height() / radiusFactor );

  const double step = 2 * M_PI / count;

  double angle = 0.0;
  int i = 0;
  QMap<unsigned int, HostItem*>::ConstIterator it;
  for ( it = m_hostItems.begin(); it != m_hostItems.end(); ++it ) {
    double factor = 1 - ( 1.0 / ( ringCount + 1 ) ) * ( i % ringCount );

    double xr = xRadius * factor;
    double yr = yRadius * factor;

    HostItem *item = it.data();

    item->updateName();

    item->setCenterPos( width() / 2 + cos( angle ) * xr,
                        height() / 2 + sin( angle ) * yr );

    angle += step;
    ++i;
  }

  m_canvas->update();
}

HostItem *StarView::createHostItem( unsigned int hostid )
{
  HostInfo *i = hostInfoManager()->find( hostid );

  if ( !i || i->isOffline() || i->name().isEmpty() )
    return 0;

//  kdDebug() << "New node for " << hostid << " (" << i->name() << ")" << endl;

  //assert( !i->name().isEmpty() );

  HostItem *hostItem = new HostItem( i, m_canvas, hostInfoManager() );
  hostItem->setHostColor( hostColor( hostid ) );
  m_hostItems.insert( hostid, hostItem );
  hostItem->show();

  arrangeHostItems();

  if ( m_hostItems.count() > 25 ) {
    mConfigDialog->setMaxNodes( m_hostItems.count() );
  }

  return hostItem;
}

void StarView::drawNodeStatus()
{
  QMap<unsigned int, HostItem*>::ConstIterator it;
  for ( it = m_hostItems.begin(); it != m_hostItems.end(); ++it ) {
    drawState( *it );
  }
}

void StarView::drawState( HostItem *node )
{
    delete node->stateItem();
    Q3CanvasLine *newItem = 0;

    QColor color;
    unsigned int client = node->client();
    if ( !client ) color = Qt::green;
    else color = hostColor( client );

    if ( node->isCompiling() ) {
      newItem = new Q3CanvasLine( m_canvas );
      newItem->setPen( color );
    } else if ( node->isActiveClient() ) {
      newItem = new Q3CanvasLine( m_canvas );
      newItem->setPen( QPen( color, 0, Qt::DashLine ) );
    }

    if ( newItem ) {
      newItem->setPoints( qRound( node->centerPosX() ),
                          qRound( node->centerPosY() ),
                          qRound( m_schedulerItem->centerPosX() ),
                          qRound( m_schedulerItem->centerPosY() ) );
      newItem->show();
    }

    node->setStateItem( newItem );
}

void StarView::createKnownHosts()
{
  HostInfoManager::HostMap hosts = hostInfoManager()->hostMap();

  HostInfoManager::HostMap::ConstIterator it;
  for( it = hosts.begin(); it != hosts.end(); ++it ) {
    unsigned int id = (*it)->id();
    if ( !findHostItem( id ) ) createHostItem( id );
  }

  m_canvas->update();
}

void StarView::configureView()
{
  mConfigDialog->show();
  mConfigDialog->raise();
}

bool StarView::filterArch( unsigned int hostid )
{
  HostInfo *i = hostInfoManager()->find( hostid );
  if ( !i ) {
    kdError() << "No HostInfo for id " << hostid << endl;
    return false;
  }

  return filterArch( i );
}

bool StarView::filterArch( HostInfo *i )
{
  if ( mConfigDialog->archFilter().isEmpty() ) return true;

  QRegExp regExp( mConfigDialog->archFilter() );

  if ( regExp.search( i->platform() ) >= 0 ) {
    return true;
  }

  return false;
}
