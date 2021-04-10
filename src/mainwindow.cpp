#include "mainwindow.h"

#include <QRegularExpression>
#include <QStyleHints>



extern QString defaultUserAgentStr;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      notificationsTitleRegExp("^\\([1-9]\\d*\\).*"),
      trayIconRead(":/icons/app/whatsapp.svg"),
      trayIconUnread(":/icons/app/whatsapp-message.svg")
{
    qApp->setQuitOnLastWindowClosed(false);

    setWindowTitle(QApplication::applicationName());
    setWindowIcon(QIcon(":/icons/app/icon-256.png"));
    setMinimumWidth(800);
    setMinimumHeight(600);


    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());

    createActions();
    createStatusBar();
    createTrayIcon();
    createWebEngine();


    if(settings.value("lockscreen",false).toBool())
    {
        init_lock();
    }
    QTimer *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer,&QTimer::timeout,[=](){
        if(settings.value("asdfg").isValid()){
            if(lockWidget && lockWidget->isLocked==false){
                timer->stop();
                //init_accountWidget();
            }
        }
    });
    timer->start();

    init_settingWidget();

    // quit application if the download manager window is the only remaining window
    m_downloadManagerWidget.setAttribute(Qt::WA_QuitOnClose, false);

    lightPalette = qApp->palette();
    updateWindowTheme();
}

void MainWindow::updatePageTheme()
{
    QString webPageTheme = "web"; //implies light
    QString windowTheme  = settings.value("windowTheme","light").toString();
    if(windowTheme == "dark"){
        webPageTheme = "web dark";
    }
    if(webEngine && webEngine->page()){
        webEngine->page()->runJavaScript(
            "document.querySelector('body').className='"+webPageTheme+"';",
            [](const QVariant &result){
                qDebug() << "Value is: " << result.toString() << endl;
            }
        );
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    if(lockWidget != nullptr){
        lockWidget->resize(event->size());
    }
}

void MainWindow::updateWindowTheme()
{
    if(settings.value("windowTheme","light").toString() == "dark")
    {

        qApp->setStyle(QStyleFactory::create("fusion"));
        QPalette palette;
        palette.setColor(QPalette::Window,QColor("#262D31")); //whatsapp dark color
        palette.setColor(QPalette::Disabled,QPalette::Window,QColor("#3f4143")); //whatsapp dark color

        palette.setColor(QPalette::WindowText,Qt::white);
        palette.setColor(QPalette::Disabled,QPalette::WindowText,QColor(127,127,127));
        palette.setColor(QPalette::Base,QColor("#323739"));
        palette.setColor(QPalette::AlternateBase,QColor("#5f6c73"));
        palette.setColor(QPalette::ToolTipBase,Qt::white);
        palette.setColor(QPalette::ToolTipText,QColor(53,53,53));
        palette.setColor(QPalette::Text,Qt::white);
        palette.setColor(QPalette::Disabled,QPalette::Text,QColor("#646464"));
        palette.setColor(QPalette::Dark,QColor(35,35,35));
        palette.setColor(QPalette::Shadow,QColor(20,20,20));
        palette.setColor(QPalette::Button,QColor(53,53,53));
        palette.setColor(QPalette::ButtonText,Qt::white);
        palette.setColor(QPalette::Disabled,QPalette::ButtonText,QColor(127,127,127));
        palette.setColor(QPalette::BrightText,Qt::red);
        palette.setColor(QPalette::Link,QColor("skyblue"));
        palette.setColor(QPalette::Highlight,QColor(49,106,150));
        palette.setColor(QPalette::Disabled,QPalette::Highlight,QColor(80,80,80));
        palette.setColor(QPalette::HighlightedText,Qt::white);
        palette.setColor(QPalette::Disabled,QPalette::HighlightedText,QColor(127,127,127));
        qApp->setPalette(palette);
        this->webEngine->setStyleSheet("QWebEngineView{background:#131C21;}"); //whatsapp dark color
    }
    else{
        this->webEngine->setStyleSheet("QWebEngineView{background:#F0F0F0;}"); //whatsapp light color
        lightPalette.setColor(QPalette::Window,QColor("#F0F0F0"));
        qApp->setPalette(lightPalette);
    }

    if(lockWidget!=nullptr)
    {
        lockWidget->setStyleSheet("QWidget#login{background-color:palette(window)};"
                                  "QWidget#signup{background-color:palette(window)};"
                                  );
        lockWidget->applyThemeQuirks();
    }
    this->update();
}

void MainWindow::handleCookieAdded(const QNetworkCookie &cookie)
{
  qDebug() << cookie.toRawForm() << "\n\n\n";
}


void MainWindow::init_settingWidget()
{
    if(settingsWidget == nullptr)
    {
        settingsWidget = new SettingsWidget(this,webEngine->page()->profile()->cachePath()
                                            ,webEngine->page()->profile()->persistentStoragePath());
        settingsWidget->setWindowTitle(QApplication::applicationName()+" | Settings");
        settingsWidget->setWindowFlags(Qt::Dialog);

        connect(settingsWidget,SIGNAL(init_lock()),this,SLOT(init_lock()));
        connect(settingsWidget,SIGNAL(updateWindowTheme()),this,SLOT(updateWindowTheme()));
        connect(settingsWidget,SIGNAL(updatePageTheme()),this,SLOT(updatePageTheme()));
        connect(settingsWidget,&SettingsWidget::muteToggled,[=](const bool checked)
        {
            this->toggleMute(checked);
        });
        connect(settingsWidget,&SettingsWidget::userAgentChanged,[=](QString userAgentStr)
        {
            if(webEngine->page()->profile()->httpUserAgent() != userAgentStr)
            {
                settings.setValue("useragent",userAgentStr);
                this->updateSettingsUserAgentWidget();
                this->askToReloadPage();
            }
        });
        connect(settingsWidget,&SettingsWidget::autoPlayMediaToggled,[=](bool checked)
        {
            QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
            auto* webSettings = profile->settings();
            webSettings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture,checked);

            this->webEngine->page()->profile()->settings()->setAttribute(
                        QWebEngineSettings::PlaybackRequiresUserGesture,
                                   checked);
        });

        connect(settingsWidget,&SettingsWidget::dictChanged,[=](QString dictName)
        {
            if(webEngine && webEngine->page())
            {
                webEngine->page()->profile()->setSpellCheckLanguages(QStringList()<<dictName);
            }
        });

        connect(settingsWidget,&SettingsWidget::spellCheckChanged,[=](bool checked){
            if(webEngine && webEngine->page())
            {
                webEngine->page()->profile()->setSpellCheckEnabled(checked);
            }
        });

        settingsWidget->appLockSetChecked(settings.value("lockscreen",false).toBool());

        //spell checker
        settingsWidget->loadDictionaries(m_dictionaries);

        settingsWidget->resize(settingsWidget->sizeHint().width(),settingsWidget->minimumSizeHint().height());
    }
}

void MainWindow::lockApp()
{
    if(settings.value("asdfg").isValid() && settings.value("lockscreen").toBool()==false){
        QMessageBox::critical(this,QApplication::applicationName()+"| Error",
                              "Unable to lock App, Enable AppLock in settings First.");
        this->show();
        return;
    }

    if(settings.value("asdfg").isValid()){
        init_lock();
    }else{
        if(settings.value("asdfg").isValid() ==false){
            QMessageBox msgBox;
              msgBox.setText("App lock is not configured.");
              msgBox.setIconPixmap(QPixmap(":/icons/information-line.png").scaled(42,42,Qt::KeepAspectRatio,Qt::SmoothTransformation));
              msgBox.setInformativeText("Do you want to setup App lock now ?");
              msgBox.setStandardButtons(QMessageBox::Cancel );
              QPushButton *setAppLock = new QPushButton("Yes",nullptr);
              msgBox.addButton(setAppLock,QMessageBox::NoRole);
              connect(setAppLock,&QPushButton::clicked,[=](){
                    init_lock();
              });
              msgBox.exec();
        }
    }
}

void MainWindow::showSettings()
{
    if(lockWidget && lockWidget->isLocked){
        QMessageBox::critical(this,QApplication::applicationName()+"| Error",
                              "UnLock Application to access Settings.");
        this->show();
        return;
    }

    if(webEngine == nullptr){
        QMessageBox::critical(this,QApplication::applicationName()+"| Error",
                              "Unable to initialize settings module.\nIs webengine initialized?");
        return;
    }
    if(!settingsWidget->isVisible())
    {
     this->updateSettingsUserAgentWidget();
     settingsWidget->refresh();
     settingsWidget->showNormal();
    }
}

void MainWindow::updateSettingsUserAgentWidget()
{
    settingsWidget->updateDefaultUAButton(this->webEngine->page()->profile()->httpUserAgent());
}

void MainWindow::askToReloadPage()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle(QApplication::applicationName()+" | Action required");
    msgBox.setInformativeText("Page needs to be reloaded to continue.");
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
    this->doAppReload();
}

void MainWindow::showAbout()
{
    About *about = new About(this);
    about->setWindowFlag(Qt::Dialog);
    about->setMinimumSize(about->sizeHint());
    about->adjustSize();
    about->setAttribute(Qt::WA_DeleteOnClose);
    about->show();
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    if(QSystemTrayIcon::isSystemTrayAvailable() && settings.value("closeButtonActionCombo",0).toInt() == 0){
        this->hide();
        event->ignore();
        notify(QApplication::applicationName(),"Application is minimized to system tray.");
        return;
    }
    event->accept();
    qApp->quit();
    QMainWindow::closeEvent(event);
}

void MainWindow::notify(QString title,QString message)
{
    if(settings.value("disableNotificationPopups",false).toBool() == true){
        return;
    }
    auto popup = new NotificationPopup(webEngine);
    connect(popup,&NotificationPopup::notification_clicked,[=](){
        if(windowState()==Qt::WindowMinimized || windowState()!=Qt::WindowActive){
            activateWindow();
            raise();
            showNormal();
        }
    });
    popup->style()->polish(qApp);
    popup->setMinimumWidth(300);
    popup->adjustSize();
    popup->update();
    popup->present(title,message,QPixmap(":/icons/app/icon-64.png"));
}

void MainWindow::createActions()
{
    fullscreenAction = new QAction(tr("Fullscreen"),this);
    fullscreenAction->setShortcut(Qt::Key_F11);
    connect(fullscreenAction, &QAction::triggered,[=](){
        setWindowState(windowState() ^ Qt::WindowFullScreen);
    });
    this->addAction(fullscreenAction);


    minimizeAction = new QAction(tr("Mi&nimize to tray"), this);
    minimizeAction->setShortcut(QKeySequence(Qt::Modifier::CTRL + Qt::Key_H));
    connect(minimizeAction, &QAction::triggered, this, &QWidget::hide);
    addAction(minimizeAction);
    this->addAction(minimizeAction);

    restoreAction = new QAction(tr("&Restore"), this);
    connect(restoreAction, &QAction::triggered, this, &QWidget::show);
    addAction(restoreAction);

    reloadAction = new QAction(tr("Re&load"), this);
    reloadAction->setShortcut(Qt::Key_F5);
    connect(reloadAction, &QAction::triggered, this, &MainWindow::doReload);
    addAction(reloadAction);

    lockAction = new QAction(tr("Loc&k"), this);
    lockAction->setShortcut(QKeySequence(Qt::Modifier::CTRL+Qt::Key_L));
    connect(lockAction, &QAction::triggered, this, &MainWindow::lockApp);
    addAction(lockAction);
    this->addAction(lockAction);

    settingsAction = new QAction(tr("&Settings"), this);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);


    aboutAction = new QAction(tr("&About"), this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    quitAction = new QAction(tr("&Quit"), this);
    quitAction->setShortcut(QKeySequence(Qt::Modifier::CTRL + Qt::Key_Q));
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    addAction(quitAction);
    this->addAction(quitAction);
}

void MainWindow::createStatusBar()
{
    QStatusBar *statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    statusBar->hide();
    this->statusBar = statusBar;
}

void MainWindow::createTrayIcon()
{
    trayIconMenu = new QMenu(this);
    trayIconMenu->setObjectName("trayIconMenu");
    trayIconMenu->addAction(minimizeAction);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(reloadAction);
    trayIconMenu->addAction(lockAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(settingsAction);
    trayIconMenu->addAction(aboutAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(trayIconRead, this);
    trayIcon->setContextMenu(trayIconMenu);
    connect(trayIconMenu,SIGNAL(aboutToShow()),this,SLOT(check_window_state()));

    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::messageClicked,
            this, &MainWindow::messageClicked);
    connect(trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::iconActivated);

    //enable show shortcuts in menu
    if(qApp->styleHints()->showShortcutsInContextMenus())
    {
        foreach(QAction *action, trayIconMenu->actions()){
            action->setShortcutVisibleInContextMenu(true);
        }
    }
}


void MainWindow::init_lock()
{
    if(lockWidget==nullptr){
        lockWidget = new Lock(this);
        lockWidget->setObjectName("lockWidget");
    }
        lockWidget->setWindowFlags(Qt::Widget);
        lockWidget->setStyleSheet("QWidget#login{background-color:palette(window)};"
                                  "QWidget#signup{background-color:palette(window)}");
        lockWidget->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        lockWidget->setGeometry(this->rect());

        connect(lockWidget,&Lock::passwordNotSet,[=]()
        {
            settings.setValue("lockscreen",false);
            settingsWidget->appLockSetChecked(false);
        });

        connect(lockWidget,&Lock::unLocked,[=]()
        {
            //unlock event
        });

        connect(lockWidget,&Lock::passwordSet,[=](){
            //enable disable lock screen
            if(settings.value("asdfg").isValid()){
                settingsWidget->setCurrentPasswordText("Current Password: <i>"
                        +QByteArray::fromBase64(settings.value("asdfg").toString().toUtf8())+"</i>");
            }else{
               settingsWidget->setCurrentPasswordText("Current Password: <i>Require setup</i>");
            }
            settingsWidget->appLockSetChecked(settings.value("lockscreen",false).toBool());
        });
        lockWidget->applyThemeQuirks();
        lockWidget->show();
        if(settings.value("asdfg").isValid() && settings.value("lockscreen").toBool()==true){
            lockWidget->lock_app();
        }
        updateWindowTheme();
}

//check window state and set tray menus
void MainWindow::check_window_state()
{
    QObject *tray_icon_menu = this->findChild<QObject*>("trayIconMenu");
    if(tray_icon_menu != nullptr){
        if(this->isVisible()){
            ((QMenu*)(tray_icon_menu))->actions().at(0)->setDisabled(false);
            ((QMenu*)(tray_icon_menu))->actions().at(1)->setDisabled(true);
        }else{
            ((QMenu*)(tray_icon_menu))->actions().at(0)->setDisabled(true);
            ((QMenu*)(tray_icon_menu))->actions().at(1)->setDisabled(false);
        }
        if(lockWidget && lockWidget->isLocked){
            ((QMenu*)(tray_icon_menu))->actions().at(4)->setDisabled(true);
        }else{
            ((QMenu*)(tray_icon_menu))->actions().at(4)->setDisabled(false);
        }
    }
}

void MainWindow::init_globalWebProfile()
{

    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    profile->setHttpUserAgent(settings.value("useragent",defaultUserAgentStr).toString());

    QStringList dict_names;
    dict_names.append(settings.value("sc_dict","en-US").toString());

    profile->setSpellCheckEnabled(settings.value("sc_enabled",true).toBool());
    profile->setSpellCheckLanguages(dict_names);

    auto* webSettings = profile->settings();
    webSettings->setAttribute(QWebEngineSettings::AutoLoadImages, true);
    webSettings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    webSettings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    webSettings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    webSettings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    webSettings->setAttribute(QWebEngineSettings::XSSAuditingEnabled, true);
    webSettings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    webSettings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, false);
    webSettings->setAttribute(QWebEngineSettings::DnsPrefetchEnabled,true);
    webSettings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled ,true);
    webSettings->setAttribute(QWebEngineSettings::LinksIncludedInFocusChain, false);
    webSettings->setAttribute(QWebEngineSettings::FocusOnNavigationEnabled, false);
    webSettings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture,
                                                        settings.value("autoPlayMedia",false).toBool());

}

void MainWindow::createWebEngine()
{
    init_globalWebProfile();

    QSizePolicy widgetSize;
    widgetSize.setHorizontalPolicy(QSizePolicy::Expanding);
    widgetSize.setVerticalPolicy(QSizePolicy::Expanding);
    widgetSize.setHorizontalStretch(1);
    widgetSize.setVerticalStretch(1);

    m_dictionaries = Dictionaries::GetDictionaries();

    WebView *webEngine = new WebView(this,m_dictionaries);
    setCentralWidget(webEngine);
    webEngine->setSizePolicy(widgetSize);
    webEngine->show();

    this->webEngine = webEngine;
    //webEngine->setContextMenuPolicy(Qt::CustomContextMenu);

    webEngine->addAction(minimizeAction);
    webEngine->addAction(lockAction);
    webEngine->addAction(quitAction);

    connect(webEngine, &QWebEngineView::titleChanged,
            this, &MainWindow::handleWebViewTitleChanged);
    connect(webEngine, &QWebEngineView::loadStarted,
            this, &MainWindow::handleLoadStarted);
    connect(webEngine, &QWebEngineView::loadProgress,
            this, &MainWindow::handleLoadProgress);
    connect(webEngine, &QWebEngineView::loadFinished,
            this, &MainWindow::handleLoadFinished);
    connect(webEngine, &QWebEngineView::renderProcessTerminated,
            [this](QWebEnginePage::RenderProcessTerminationStatus termStatus, int statusCode) {
        QString status;
        switch (termStatus) {
        case QWebEnginePage::NormalTerminationStatus:
            status = tr("Render process normal exit");
            break;
        case QWebEnginePage::AbnormalTerminationStatus:
            status = tr("Render process abnormal exit");
            break;
        case QWebEnginePage::CrashedTerminationStatus:
            status = tr("Render process crashed");
            break;
        case QWebEnginePage::KilledTerminationStatus:
            status = tr("Render process killed");
            break;
        }
        QMessageBox::StandardButton btn = QMessageBox::question(window(), status,
                                                   tr("Render process exited with code: %1\n"                                            "Do you want to reload the page ?").arg(statusCode));
        if (btn == QMessageBox::Yes)
            QTimer::singleShot(0, [this] { this->webEngine->reload(); });
    });

    QWebEngineCookieStore *browser_cookie_store = webEngine->page()->profile()->cookieStore();
    connect( browser_cookie_store, &QWebEngineCookieStore::cookieAdded, this, &MainWindow::handleCookieAdded );

    createWebPage(false);
}

void MainWindow::createWebPage(bool offTheRecord)
{
    if (offTheRecord && !m_otrProfile)
    {
        m_otrProfile.reset(new QWebEngineProfile);
    }

    auto profile = offTheRecord ? m_otrProfile.get() : QWebEngineProfile::defaultProfile();

    QStringList dict_names;
    dict_names.append(settings.value("sc_dict","en-US").toString());

    profile->setSpellCheckEnabled(settings.value("sc_enabled",true).toBool());
    profile->setSpellCheckLanguages(dict_names);

    profile->setHttpUserAgent(settings.value("useragent",defaultUserAgentStr).toString());

        auto popup = new NotificationPopup(webEngine);
        connect(popup,&NotificationPopup::notification_clicked,[=](){
            if(windowState()==Qt::WindowMinimized || windowState()!=Qt::WindowActive){
                activateWindow();
                raise();
                showNormal();
            }
        });
        profile->setNotificationPresenter([=] (std::unique_ptr<QWebEngineNotification> notification)
        {
            if(settings.value("disableNotificationPopups",false).toBool() == true){
                return;
            }
            popup->setMinimumWidth(300);
            popup->update();
            popup->present(notification);
        });

    QWebEnginePage *page = new WebEnginePage(profile,webEngine);
    if(settings.value("windowTheme","light").toString() == "dark"){
        page->setBackgroundColor(QColor("#131C21")); //whatsapp dark bg color
    }else{
        page->setBackgroundColor(QColor("#F0F0F0")); //whatsapp light bg color
    }
    webEngine->setPage(page);
    //page should be set parent of profile to prevent
    //Release of profile requested but WebEnginePage still not deleted. Expect troubles !
    profile->setParent(page);

//    RequestInterceptor *interceptor = new RequestInterceptor(profile);
//    profile->setUrlRequestInterceptor(interceptor);
    qsrand(time(NULL));
    auto randomValue = qrand() % 300;
    page->setUrl(QUrl("https://web.whatsapp.com?v="+QString::number(randomValue)));
    //page->setUrl(QUrl("http://ktechpit.com/USS/text.html"));
    connect(profile, &QWebEngineProfile::downloadRequested,
        &m_downloadManagerWidget, &DownloadManagerWidget::downloadRequested);

    connect(webEngine->page(), SIGNAL(fullScreenRequested(QWebEngineFullScreenRequest)),
                this, SLOT(fullScreenRequested(QWebEngineFullScreenRequest)));
}

void MainWindow::fullScreenRequested(QWebEngineFullScreenRequest request)
{
    if (request.toggleOn())
    {
            webEngine->showFullScreen();
            this->showFullScreen();
            request.accept();
    } else {
            webEngine->showNormal();
            this->showNormal();
            request.accept();
    }
}

void MainWindow::handleWebViewTitleChanged(QString title)
{
    setWindowTitle(title);

    if (notificationsTitleRegExp.exactMatch(title))
    {
        if(notificationsTitleRegExp.isEmpty() == false){
            QString capturedTitle = notificationsTitleRegExp.capturedTexts().first();
            QRegExp rgex("\\([^\\d]*(\\d+)[^\\d]*\\)");
            rgex.setMinimal(true);
            if(rgex.indexIn(capturedTitle) != -1){
                qDebug()<<rgex.capturedTexts();
                QString unreadMessageCount = rgex.capturedTexts().last();
                QString suffix = unreadMessageCount.toInt() > 1 ? tr("messages"): tr("message");
                restoreAction->setText(tr("Restore")+" | "+unreadMessageCount+" "+suffix);
            }
        }
        trayIcon->setIcon(trayIconUnread);
        setWindowIcon(trayIconUnread);
    }
    else
    {
        trayIcon->setIcon(trayIconRead);
        setWindowIcon(trayIconRead);
    }
}

void MainWindow::handleLoadStarted()
{
    statusBar->show();
}

void MainWindow::handleLoadFinished(bool loaded)
{
    statusBar->hide();
    if(loaded){
        //check if page has loaded correctly
        checkLoadedCorrectly();
        updatePageTheme();
    }
}

void MainWindow::checkLoadedCorrectly()
{
    if(webEngine && webEngine->page())
    {
        //test 1 based on the class name of body of the page
        webEngine->page()->runJavaScript("document.querySelector('body').className",[this](const QVariant &result)
        {
            if(result.toString().contains("page-version",Qt::CaseInsensitive))
            {
                qWarning()<<"Test 1 found"<<result.toString();
                loadingQuirk("test1");
            }else if(webEngine->title().contains("Error",Qt::CaseInsensitive))
            {
                utils::delete_cache(webEngine->page()->profile()->cachePath());
                utils::delete_cache(webEngine->page()->profile()->persistentStoragePath());
                settings.setValue("useragent",defaultUserAgentStr);
                utils * util = new utils(this);
                util->DisplayExceptionErrorDialog("test1 handleWebViewTitleChanged(title) title: Error, Resetting UA, Quiting!\nUA: "+settings.value("useragent","DefaultUA").toString());

                quitAction->trigger();
            }else{
                qWarning()<<"Test 1 Loaded correctly value:"<<result.toString();
            }
        });

//        //test #2 based on the content of landing-title class of page
//        webEngine->page()->runJavaScript(
//            "document.getElementsByClassName('landing-title')[0].innerText",
//            [this](const QVariant &result){
//                qWarning()<<"Test #1 Loaded correctly value:"<<result.toString();
//                if(result.toString().contains("WhatsApp works with",Qt::CaseInsensitive)){
//                    loadingQuirk("test2");
//                }else if(webEngine->title().contains("Error",Qt::CaseInsensitive)){
//                    utils::delete_cache(webEngine->page()->profile()->cachePath());
//                    utils::delete_cache(webEngine->page()->profile()->persistentStoragePath());
//                    settings.setValue("useragent",defaultUserAgentStr);
//                    utils * util = new utils(this);
//                    util->DisplayExceptionErrorDialog("handleWebViewTitleChanged(title) title: Error, Resetting UA, Quiting!\nUA: "+settings.value("useragent","DefaultUA").toString());

//                    quitAction->trigger();
//                }
//            }
//        );
    }
}

void MainWindow::loadingQuirk(QString test)
{
    //contains ug message apply quirk
    if(correctlyLoaderRetries > -1){
        qWarning()<<test<<"checkLoadedCorrectly()/loadingQuirk()/doReload()"<<correctlyLoaderRetries;
        doReload();
        correctlyLoaderRetries--;
    }else{
        utils::delete_cache(webEngine->page()->profile()->cachePath());
        utils::delete_cache(webEngine->page()->profile()->persistentStoragePath());
        settings.setValue("useragent",defaultUserAgentStr);
        utils * util = new utils(this);
        util->DisplayExceptionErrorDialog(test+" checkLoadedCorrectly()/loadingQuirk() reload retries 0, Resetting UA, Quiting!\nUA: "+settings.value("useragent","DefaultUA").toString());

        quitAction->trigger();
    }
}


void MainWindow::handleLoadProgress(int progress)
{
    statusBar->showMessage("Loading "+QString::number(progress)+"%");
    if (progress >= 50)
    {
        statusBar->hide();
    }
}

//unused direct method to download file without having entry in download manager
void MainWindow::handleDownloadRequested(QWebEngineDownloadItem *download)
{
    QFileDialog dialog;
    dialog.setParent(this);
    dialog.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
    dialog.setFileMode(QFileDialog::FileMode::AnyFile);
    QString suggestedFileName = QUrl(download->path()).fileName();
    dialog.selectFile(suggestedFileName);

    if (dialog.exec() && dialog.selectedFiles().size() > 0)
    {
        download->setPath(dialog.selectedFiles().at(0));
        download->accept();
    }
}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    Q_UNUSED(reason);
    if (isVisible()) {
        hide();
    } else {
        showNormal();
    }
}

void MainWindow::messageClicked()
{
    if (isVisible()) {
        hide();
    } else {
        showNormal();
    }
}

void MainWindow::doAppReload()
{

    if(this->webEngine->page()){
        this->webEngine->page()->disconnect();
    }
    createWebPage(false);
}

void MainWindow::doReload()
{
    this->webEngine->triggerPageAction(QWebEnginePage::ReloadAndBypassCache, false);
}

void MainWindow::toggleMute(const bool checked)
{
    this->webEngine->page()->setAudioMuted(checked);
}

// get value of page theme when page is loaded
QString MainWindow::getPageTheme()
{
    static QString theme = "web"; //implies light
    if(webEngine && webEngine->page())
    {
        webEngine->page()->runJavaScript(
            "document.querySelector('body').className;",
            [](const QVariant &result){
                theme = result.toString();
            }
        );
    }
    return theme;
}
