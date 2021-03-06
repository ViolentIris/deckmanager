#include "mainwindow.h"
#include "config.h"
#include "iconbutton.h"
#include "card.h"
#include "carditem.h"
#include "cardslist.h"
#include "decklist.h"
#include "carddetails.h"
#include "remote.h"
#include "locallist.h"
#include "cardfilter.h"
#include "replaylist.h"
#include "packview.h"
#include "pref.h"
#include "deckview.h"
#include "signaltower.h"
#include <QDebug>
#include <QSplitter>
#include <QTimer>
#include <QTabBar>
#include <functional>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), currentId(0)
{
    setWindowTitle("deckmanager - by qww6");
    if(config->bg)
    {
        QString bg = config->getStr("pref", "bg", "");
        setStyleSheet("QMainWindow{border-image: url(" + bg + ")}");
    }

    qint32 max = config->getStr("pref", "max", "0").toInt();
    qint32 width = config->getStr("pref", "width", "0").toInt();
    qint32 height = config->getStr("pref", "height", "0").toInt();

    if (width == 0)
    {
        width = size().width();
    }

    if (height == 0)
    {
        height = size().height();
    }

    resize(width, height);

    if (max > 0) {
        setWindowState(Qt::WindowMaximized);
    }

    auto modelTabBar = new QTabBar;
    auto sp = new QSplitter(Qt::Horizontal, this);
    sp->setHandleWidth(3);
    auto deckListView = new DeckListView;
    auto deckView = new DeckView(nullptr, modelTabBar);
    auto cardListView = new CardsListView(nullptr);

    auto cardDetails = new CardDetails;
    auto localList = new LocalList;
    auto filter = new CardFilter;

    auto replayList = new ReplayList;
    auto replayRefresh = new IconButton(":/icons/refresh.png", config->getStr("action", "refresh", "刷新"));

    auto packView = new PackView(nullptr);
    auto pref = new Pref;
    auto replayWidget = new QWidget;
    auto vbox = new QVBoxLayout;

    auto getCurrentResults = [=]() -> Type::Deck&
    {
        return cardListView->getList();
    };
    filter->getCurrent = getCurrentResults;

    auto getDeck = [=]() -> Type::DeckP
    {
        return deckView->getDeck();
    };
    filter->getDeck = getDeck;

    vbox->addWidget(replayList);
    vbox->addWidget(replayRefresh);
    replayWidget->setLayout(vbox);

    dialog = new ScriptView(this);

    tab = new MTabWidget;
    tab->setStyleSheet("QWidget{font-size: 13px}");
    cardDetails->setStyleSheet("QWidget{font-size: 15px}");

    connect(cardDetails, &CardDetails::clickId, dialog, &ScriptView::setId);
    connect(tower, &SignalTower::clickId, filter, &CardFilter::searchSet);
    connect(deckView, &DeckView::deckText, dialog, &ScriptView::setDeck);

    connect(replayRefresh, &IconButton::clicked, replayList, &ReplayList::refresh);

    connect(tower, &SignalTower::currentIdChanged, this, &MainWindow::changeId);

    connect(this, &MainWindow::currentIdChanged, [=](quint32 id) {
        cardDetails->setId(id);
        deckView->setCurrentCardId(id);
        cardListView->setCurrentCardId(id);
    });


    connect(deckListView, &DeckListView::selectDeck, deckView, &DeckView::loadRemoteDeck);
    connect(localList, &LocalList::deckStream, deckView, &DeckView::loadDeck);
    connect(localList, &LocalList::saveDeck, deckView, &DeckView::saveDeck);
    connect(filter, &CardFilter::result, cardListView, &CardsListView::setCards);

    connect(replayList, &ReplayList::deckStream,deckView, &DeckView::loadDeck);
    connect(packView, &PackView::cards, cardListView, &CardsListView::setCards);

    connect(pref, &Pref::lflistChanged, [=]() {
        deckView->update();
        cardListView->update();
        packView->update();
    });
    connect(pref, &Pref::lfList, cardListView, &CardsListView::setCards);

    connect(tower, &SignalTower::details, this, &MainWindow::toDetails);

    connect(deckView, &DeckView::save, [=](QString name){
        tab->setCurrentIndex(1, 0);
        localList->setPathFocus(name);
    });

    connect(deckView, &DeckView::statusChanged, this, &MainWindow::setWindowTitle);
    connect(deckView, &DeckView::refreshLocals, localList, &LocalList::refresh);

    connect(this, &MainWindow::destroyed, cardPool->getThread(), &LoadThread::terminate);

    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [=]() {
       deckView->checkLeave();
       cardListView->checkLeave();
       packView->checkLeave();
    });
    timer->start(200);


    tab->addTabBar();
    tab->addTabBar();
    tab->addWidget(1, localList, config->getStr("tab", "local", "本地"));
    tab->addWidget(1, deckListView, config->getStr("tab", "remote", "远程"));
    tab->addWidget(1, replayWidget, config->getStr("tab", "replay", "录像"));
    tab->addWidget(0, cardDetails, config->getStr("tab", "card", "卡"));
    tab->addWidget(0, filter, config->getStr("tab", "search", "卡池"));
    tab->addWidget(0, packView, config->getStr("tab", "pack", "卡包"));
    tab->addWidget(0, pref, config->getStr("tab", "pref", "选项"));
    tab->setCurrentIndex(1, 0);

    sp->addWidget(tab);
    sp->addWidget(deckView);
    sp->addWidget(cardListView);
    sp->setStretchFactor(1, 1);
    sp->setStyleSheet("QSplitter:handle{background:transparent}");

    tab->changeSize();

    auto hboxtop = new QHBoxLayout;

    auto vboxtop = new QVBoxLayout;
    hboxtop->addWidget(modelTabBar);


    auto toolbar = new QToolBar;

    auto newAction = new QAction(toolbar);
    newAction->setIcon(QIcon(":/icons/add.png"));
    newAction->setToolTip(config->getStr("action", "new", "新建"));

    toolbar->setStyleSheet("QToolTip{font-size:12px}QToolBar{background: rgba(255, 255, 255, 200)}");
    toolbar->setFixedHeight(24);
    toolbar->addAction(newAction);

    connect(newAction, &QAction::triggered, deckView, &DeckView::newTab);

    auto spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolbar->addWidget(spacer);

    hboxtop->addWidget(toolbar);

    vboxtop->addLayout(hboxtop);
    vboxtop->addWidget(sp, 1);
    auto widgettop = new QWidget;
    vboxtop->setMargin(0);
    vboxtop->setSpacing(0);
    hboxtop->setSpacing(0);

    widgettop->setLayout(vboxtop);
    setCentralWidget(widgettop);

    deckView->setStatus();
    deckListView->getList(1);
    localList->refresh();
    cardListView->refresh();
    replayList->refresh();
    packView->refresh();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Q && event->modifiers() == Qt::AltModifier)
    {
        config->autoSwitch = !config->autoSwitch;
        if(config->autoSwitch)
        {
            tab->setCurrentIndex(0, 0);
        }
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::toDetails(quint32)
{
    static QPair<int, int> prev = tab->currentIndex();

    if(tab->currentIndex() == qMakePair(0, 0))
    {
        tab->setCurrentIndex(prev);
    }
    else
    {
        prev = tab->currentIndex();
        tab->setCurrentIndex(0, 0);
    }
}

void MainWindow::changeId(quint32 id)
{
    if(id == currentId)
    {
        return;
    }

    currentId = id;

    if(!(QApplication::keyboardModifiers() & Qt::ShiftModifier))
    {
        emit currentIdChanged(id);
        if(config->autoSwitch)
        {
            tab->setCurrentIndex(0, 0);
        }
    }
}

MainWindow::~MainWindow()
{

}
