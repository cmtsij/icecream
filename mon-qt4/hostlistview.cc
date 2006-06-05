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

#include "hostlistview.h"


#include <qfontmetrics.h>
#include <qpainter.h>

#define i18n

enum Columns
{
    ColumnID,
    ColumnName,
    ColumnColor,
    ColumnIP,
    ColumnPlatform,
    ColumnMaxJobs,
    ColumnSpeed,
    ColumnLoad
};


HostListViewItem::HostListViewItem( Q3ListView* parent, const HostInfo& info )
    :  Q3ListViewItem( parent ),
       mHostInfo( info ),
       mActive( false )
{
    updateText( info );
}


const HostInfo& HostListViewItem::hostInfo() const
{
    return mHostInfo;
}


void HostListViewItem::setActiveNode( bool active )
{
    mActive = active;

    repaint();
}


void HostListViewItem::updateText( const HostInfo& info )
{
    mHostInfo = info;
    setText( ColumnID, QString::number( info.id() ) );
    setText( ColumnName, info.name() );
    setText( ColumnColor, HostInfo::colorName( info.color() ) );
    setText( ColumnIP, info.ip() );
    setText( ColumnPlatform, info.platform() );
    setText( ColumnMaxJobs, QString::number( info.maxJobs() ) );
    //setText( ColumnSpeed, KGlobal::locale()->formatNumber( info.serverSpeed(), 1 ) );
    setText( ColumnLoad, QString::number( info.serverLoad() ) );
}


template <typename Type>
int compare( Type i1,
             Type i2 )
{
    if ( i1 < i2 )
        return -1;
    else if ( i2 < i1 )
        return 1;
    else
        return 0;
}


int HostListViewItem::compare( Q3ListViewItem *i, int col,
                               bool ) const
{
    const HostListViewItem* first = this;
    const HostListViewItem* other = dynamic_cast<HostListViewItem*>( i );

    switch ( col )
    {
    case ColumnID:
        return ::compare( first->mHostInfo.id(), other->mHostInfo.id() );
    case ColumnMaxJobs:
        return ::compare( first->mHostInfo.maxJobs(), other->mHostInfo.maxJobs() );
    case ColumnSpeed:
        return ::compare( first->mHostInfo.serverSpeed(), other->mHostInfo.serverSpeed() );
    case ColumnLoad:
        return ::compare( first->mHostInfo.serverLoad(), other->mHostInfo.serverLoad() );
    default:
        return first->text(col).compare( other->text( col ) );
    }
}


void HostListViewItem::paintCell( QPainter* painter,
                                  const QColorGroup& cg,
                                  int column,
                                  int width,
                                  int align )
{
    const QFont oldFont( painter->font() );

    if ( mActive )
    {
        QFont font( oldFont );
        font.setBold( true );
        painter->setFont( font );
    }

    Q3ListViewItem::paintCell( painter, cg, column, width, align );

    painter->setFont( oldFont );
}


int HostListViewItem::width( const QFontMetrics& fm,
                             const Q3ListView* lv,
                             int column ) const
{
    int width(0);
    if ( mActive )
    {
        QFont font( lv->font() );
        font.setBold( true );
        const QFontMetrics metrics( font );
        width = metrics.width( text( column ) ) + lv->itemMargin() * 2 + 2;
    }
    else
    {
        width = Q3ListViewItem::width( fm, lv, column );
    }

    return width;
}


HostListView::HostListView( HostInfoManager* manager,
                            QWidget* parent,
                            const char* name )
    : Q3ListView( parent, name ),
      mHostInfoManager( manager ),
      mActiveNode( 0 )
{
    addColumn( i18n( "ID" ) );
    addColumn( i18n( "Name" ) );
    addColumn( i18n( "Color" ) );
    addColumn( i18n( "IP" ) );
    addColumn( i18n( "Platform" ) );
    addColumn( i18n( "Max Jobs" ) );
    addColumn( i18n( "Speed" ) );
    addColumn( i18n( "Load" ) );

    setColumnAlignment( ColumnID, Qt::AlignRight );
    setColumnAlignment( ColumnMaxJobs, Qt::AlignRight );
    setColumnAlignment( ColumnSpeed, Qt::AlignRight );
    setColumnAlignment( ColumnLoad, Qt::AlignRight );

    setAllColumnsShowFocus(true);

    connect(this, SIGNAL( doubleClicked( Q3ListViewItem*, const QPoint&, int ) ),
            this, SLOT( slotNodeActivated( Q3ListViewItem* ) ) );
    connect(this, SIGNAL( returnPressed( Q3ListViewItem* ) ),
            this, SLOT( slotNodeActivated( Q3ListViewItem* ) ) );
    connect(this, SIGNAL( spacePressed( Q3ListViewItem* ) ),
            this, SLOT( slotNodeActivated( Q3ListViewItem* ) ) );
    connect( &mUpdateSortTimer, SIGNAL( timeout()), SLOT( updateSort()));
}


unsigned int HostListView::activeNode() const
{
    return mActiveNode;
}


void HostListView::setActiveNode( unsigned int hostid )
{
    if ( mActiveNode == hostid )
        return;

    setActiveNode( mActiveNode, false);
    setActiveNode( hostid, true );

    mActiveNode = hostid;

    emit nodeActivated( hostid );
}


void HostListView::checkNode( unsigned int hostid )
{
    const HostInfo* info = mHostInfoManager->find( hostid );

    ItemMap::iterator it = mItems.find( hostid );
    if ( it == mItems.end() )
        mItems[hostid] = new HostListViewItem( this, *info );
    else
        ( *it )->updateText( *info );
    mUpdateSortTimer.start( 0, true );
}


void HostListView::removeNode( unsigned int hostid )
{
    ItemMap::iterator it = mItems.find( hostid );
    if ( it != mItems.end() )
    {
        delete *it;
        mItems.erase( it );
    }

    if ( hostid == mActiveNode )
        setActiveNode( 0 );
}


void HostListView::clear()
{
    mItems.clear();

    Q3ListView::clear();

    setActiveNode( 0 );
}


void HostListView::slotNodeActivated( Q3ListViewItem* item )
{
    HostListViewItem* hostItem = dynamic_cast<HostListViewItem*>( item );
    if ( hostItem )
        setActiveNode( hostItem->hostInfo().id() );
}


void HostListView::setActiveNode( unsigned int hostid,
                                  bool active )
{
    ItemMap::iterator it = mItems.find( hostid );
    if ( it != mItems.end() )
        (*it)->setActiveNode( active );
}

void HostListView::updateSort()
{
    if( sortColumn() != 0 )
        sort();
}
