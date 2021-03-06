/***************************************************************************
 *   Copyright (C) 2008 by Jakub Stachowski <qbast@go2.pl>                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "converter.h"

#include <QAbstractTextDocumentLayout>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFrame>
#include <QTextDocumentFragment>
#include <QDebug>
#include <QFile>

#include <KLocalizedString>
#include <core/action.h>

using namespace Mobi;

Converter::Converter() 
{
  
}

Converter::~Converter()
{    
}

void Converter::handleMetadata(const QMap<Mobipocket::Document::MetaKey,QString> &metadata)
{
  QMapIterator<Mobipocket::Document::MetaKey,QString> it(metadata);
  while (it.hasNext()) {
    it.next();
    switch (it.key()) {
        case Mobipocket::Document::Title: emit addMetaData(Okular::DocumentInfo::Title, it.value()); break;
        case Mobipocket::Document::Author: emit addMetaData(Okular::DocumentInfo::Author, it.value()); break;
        case Mobipocket::Document::Description: emit addMetaData(Okular::DocumentInfo::Description, it.value()); break;
        case Mobipocket::Document::Subject: emit addMetaData(Okular::DocumentInfo::Subject, it.value()); break;
        case Mobipocket::Document::Copyright: emit addMetaData(Okular::DocumentInfo::Copyright, it.value()); break;
    }
  }
}

QTextDocument* Converter::convert( const QString &fileName )
{
  MobiDocument* newDocument=new MobiDocument(fileName);
  if (!newDocument->mobi()->isValid()) {
    emit error(i18n("Error while opening the Mobipocket document."), -1);
    delete newDocument;
    return nullptr;
  }
  if (newDocument->mobi()->hasDRM()) {
    emit error(i18n("This book is protected by DRM and can be displayed only on designated device"), -1);
    delete newDocument;
    return nullptr;
  }
  
  handleMetadata(newDocument->mobi()->metadata());
  newDocument->setPageSize(QSizeF(600, 800));

  QTextFrameFormat frameFormat;
  frameFormat.setMargin( 20 );
  QTextFrame *rootFrame = newDocument->rootFrame();
  rootFrame->setFrameFormat( frameFormat ); 
  QMap<QString,QPair<int,int> > links;
  QMap<QString,QTextBlock> targets;

  // go over whole document and add all <a> tags to links or targets map
  for (QTextBlock it = newDocument->begin(); it != newDocument->end(); it = it.next()) 
   for (QTextBlock::iterator fit=it.begin(); !fit.atEnd(); ++fit) {
    QTextFragment frag=fit.fragment();
    QTextCharFormat format=frag.charFormat();
    if (!format.isAnchor()) continue;
    //link
    if (!format.anchorHref().isEmpty()) links[format.anchorHref()]=
      QPair<int,int>(frag.position(), frag.position()+frag.length());
    const QStringList anchors = format.anchorNames();
    if (!anchors.isEmpty()) {
      // link targets
      for (const QString &name : anchors) 
    targets[QLatin1Char('#')+name]=it;
    }
  }

  // create link actions
  QMapIterator<QString,QPair<int,int> > it(links);
  while (it.hasNext()) {
    it.next();
    QUrl u(it.key());
    // external or internal link
    if (!u.isRelative()) emit addAction(new Okular::BrowseAction(QUrl(it.key())), it.value().first, it.value().second);
    else {
      // is there valid target?
      if (!targets.contains( it.key() ) || !targets[it.key()].isValid()) continue;
      emit addAction(new Okular::GotoAction(QString(), calculateViewport( newDocument, targets[it.key()] )),
	    it.value().first, it.value().second);
    }
    
  }

  return newDocument;
}
