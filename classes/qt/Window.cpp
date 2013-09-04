#include <QtGui>
#include <QSizePolicy>

#include "../config.h"
#include "GLWidget.h"
#include "Window.h"


Window::Window() {
	setlocale( LC_ALL, "C" );

	mGlWidget = new GLWidget;

	QAction *actionExit = new QAction( tr( "&Exit" ), this );
	actionExit->setShortcuts( QKeySequence::Quit );
	actionExit->setStatusTip( tr( "Quit the application." ) );
	connect( actionExit, SIGNAL( triggered() ), this, SLOT( close() ) );

	QMenu *menuFile = new QMenu( tr( "&File" ) );
	menuFile->addAction( actionExit );

	QMenuBar *menubar = new QMenuBar( this );
	menubar->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	menubar->addMenu( menuFile );

	mStatusBar = new QStatusBar( this );
	mStatusBar->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	mStatusBar->showMessage( tr( "0 FPS" ) );

	QVBoxLayout *mainLayout = new QVBoxLayout();
	mainLayout->setSpacing( 0 );
	mainLayout->setMargin( 0 );
	mainLayout->addWidget( menubar );
	mainLayout->addWidget( mGlWidget );
	mainLayout->addWidget( mStatusBar );

	this->setLayout( mainLayout );
	this->setWindowTitle( tr( CFG_TITLE ) );
}


void Window::keyPressEvent( QKeyEvent *e ) {
	if( e->key() == Qt::Key_Escape ) {
		close();
	}
	else {
		QWidget::keyPressEvent( e );
	}
}


void Window::updateStatus( const char *msg ) {
	mStatusBar->showMessage( tr( msg ) );
}
