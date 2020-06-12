#include <QDebug>
#include <QMessageBox>
#include <QCloseEvent>
#include <QPixmap>
#include <QPainter>
#include <QTimer>

#include "mainwindow.h"
#include "ui_mainwindow.h"

class CirclePixmap: public QPixmap {
public:
    CirclePixmap(QColor col);
};

CirclePixmap::CirclePixmap(QColor col)
    :QPixmap(15,15) {

    fill(QColor(255, 0, 0, 0));
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QBrush brush(col);
    p.setBrush(brush);
    p.drawEllipse(0, 0, 15, 15);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_serviceState(ServiceMgr::Unknown)
    , m_networkState(NetworkMgr::Unknown)
    , updateState(Probe)
{
    ui->setupUi(this);

    ui->main_tab->setFocus();
#ifdef Q_OS_MAC
    QFont f = ui->tabWidget->font();
    f.setPointSize(14);
    ui->tabWidget->setFont(f);
    ui->statusOutput->setFont(f);
#endif

    // TODO - add a 'clear status messages' button to the GUI
    statusMsg("Stubby Manager Started.");
    ui->runningStatus->setText("Checking status...");
    updateState = Init;

    // Set up system tray
    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    trayIconMenu = new QMenu(this);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setIcon(QIcon(":/images/stubby@245x145.png"));
    trayIcon->show();

    // Set up circle icons
    greenPixmap = new CirclePixmap(Qt::green);
    yellowPixmap = new CirclePixmap(Qt::yellow);
    redPixmap = new CirclePixmap(Qt::red);
    greyPixmap = new CirclePixmap(Qt::lightGray);
    ui->serviceStatus->setPixmap(*greyPixmap);
    ui->networkStatus->setPixmap(*greyPixmap);
    ui->connectStatus->setPixmap(*greyPixmap);
    ui->stubbyStatus->setPixmap(*greyPixmap);

    // Set up service state
    m_serviceMgr = ServiceMgr::factory(this);
    if (!m_serviceMgr) {
        qFatal("Could not initialise Service Mgr");
        abort();
    }
    connect(m_serviceMgr, SIGNAL(serviceStateChanged(ServiceMgr::ServiceState)), this, SLOT(on_serviceStateChanged(ServiceMgr::ServiceState)));
    m_serviceMgr->getState();

    // Set up network manager
    m_networkMgr = NetworkMgr::factory(this);
    if (!m_networkMgr) {
        qFatal("Could not initialise Service Mgr");
        abort();
    }
    connect(m_networkMgr, SIGNAL(networkStateChanged(NetworkMgr::NetworkState)), this, SLOT(on_networkStateChanged(NetworkMgr::NetworkState)));
    m_networkMgr->getState();

    // Discover network and profile

    // Update connection settings display

    //Create a timer
    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, QOverload<>::of(&MainWindow::timerExpired));
    probeTimer = new QTimer(this);
    connect(probeTimer, &QTimer::timeout, this, QOverload<>::of(&MainWindow::probeTimerExpired));
    probeTimer->start(60000);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_networkMgr;
    delete m_serviceMgr;
    delete timer;
    delete probeTimer;
    delete quitAction;
    delete trayIcon;
    delete trayIconMenu;
    delete greenPixmap;
    delete yellowPixmap;
    delete redPixmap;
    delete greyPixmap;
}

void MainWindow::timerExpired() {
    if (updateState == StartStop || updateState == Restart) {
        statusMsg("Stubby timed out trying to complete an action");
        updateMainTab();
        updateState = None;
    }
}

void MainWindow::probeTimerExpired() {
    if (updateState != None)
        return;
    statusMsg("Probing State");
    updateState = Probe;
    m_serviceMgr->getState();
}

void MainWindow::closeEvent(QCloseEvent *event) {

#ifdef Q_OS_OSX
    if (!event->spontaneous() || !isVisible()) {
        return;
    }
#endif

    if (trayIcon->isVisible()) {
// TODO: Restore system tray
// Comment this out whilst in development....
//        QMessageBox::information(this, tr("Systray"),
//                                 tr("Stubby Manager will keep running in the system tray. "
//                                    "**We recommend you keep Stubby Manager running to "
//                                    "properly monitor your system.**"
//                                    "To terminate Stubby Manager, "
//                                    "choose <b>Quit</b> in the context menu "
//                                    "of the system tray entry (this won't stop Stubby itself!)."));
        hide();
        event->ignore();
    }
}

// crude mechanism to output status changes to GUI...
// TODO - have manager class connected to a slot for this and derive other classes from that
void MainWindow::statusMsg(QString statusMsg) {
    ui->statusOutput->moveCursor (QTextCursor::End);
    ui->statusOutput->insertPlainText (statusMsg);
    ui->statusOutput->insertPlainText ("\n");
    ui->statusOutput->moveCursor (QTextCursor::End);
}


/*
 * Slots functions
 */

void MainWindow::on_onOffSlider_stateChanged()
{
    if (updateState != None)
        return;
    statusMsg("");
    // Currently we handle the service status first and based on the result of that action we later update the system DNS settings
    updateState = StartStop;
    bool value = ui->onOffSlider->isChecked();
    if (value == true) {
        ui->runningStatus->setText("Stubby starting...");
        if (m_serviceState != ServiceMgr::Running && m_serviceState != ServiceMgr::Starting)
            m_serviceMgr->start();
        else if (m_networkState != NetworkMgr::Localhost)
            m_networkMgr->setLocalhost();
    }
    else {
        if (m_serviceState != ServiceMgr::Stopped && m_serviceState != ServiceMgr::Stopping) {
            ui->runningStatus->setText("Stubby stopping...");
            m_serviceMgr->stop();
        }
        else if (m_networkState != NetworkMgr::NotLocalhost)
             m_networkMgr->unsetLocalhost();
    }
    timer->start(20000);
}

void MainWindow::on_restartButton_clicked() {
    statusMsg("");
    // Currently we handle the service status first and based on the result of that action we later update the system DNS settings
    updateState = Restart;
    m_serviceMgr->restart();
    ui->runningStatus->setText("Stubby restarting...");
    timer->start(20000);
}

void MainWindow::on_serviceStateChanged(ServiceMgr::ServiceState state) {

    qDebug("Stubby Service state changed from %s to %s ", getServiceStateString(m_serviceState).toLatin1().data(), getServiceStateString(state).toLatin1().data());
    m_serviceState = state;
    switch (m_serviceState) {
        case ServiceMgr::Running:
            ui->serviceStatus->setPixmap(*greenPixmap);
            break;
        case ServiceMgr::Stopped:
            ui->serviceStatus->setPixmap(*greyPixmap);
            break;
        case ServiceMgr::Error:
            ui->serviceStatus->setPixmap(*redPixmap);
            if (updateState == StartStop || updateState == Restart) {
                updateMainTab();
                updateState = None;
            }
            return;
        default:
            ui->serviceStatus->setPixmap(*yellowPixmap);
            break;
    }

    if (updateState == StartStop) {
        if (m_serviceState == ServiceMgr::Running) {
            m_networkMgr->setLocalhost();
        }
        else if (m_serviceState != ServiceMgr::Running && m_serviceState != ServiceMgr::Starting) {
            m_networkMgr->unsetLocalhost();
        }
    } else if (updateState == Restart)
        m_networkMgr->setLocalhost();
    else if (updateState == Probe)
        m_networkMgr->getState();

    updateMainTab();
}

void MainWindow::on_networkStateChanged(NetworkMgr::NetworkState state) {
    qDebug("Network DNS state changed from %s to %s ", getNetworkStateString(m_networkState).toLatin1().data(), getNetworkStateString(state).toLatin1().data());
    m_networkState = state;
    if (m_networkState == NetworkMgr::Localhost)  ui->networkStatus->setPixmap(*greenPixmap);
    else if (m_networkState == NetworkMgr::NotLocalhost)  ui->networkStatus->setPixmap(*greyPixmap);
    else ui->networkStatus->setPixmap(*yellowPixmap);
    updateMainTab();
    updateState = None;
}

/*
 * Private functions
 */

QString MainWindow::getServiceStateString(const ServiceMgr::ServiceState state)
{
    switch (state) {
        case ServiceMgr::Stopped    : return "Not running";
        case ServiceMgr::Starting   : return "Starting";
        case ServiceMgr::Stopping   : return "Stopping";
        case ServiceMgr::Running    : return "Running";
        case ServiceMgr::Error      : return "Error determining state";
        case ServiceMgr::Unknown :
        default : return "Unknown";
    }
}

QString MainWindow::getNetworkStateString(const NetworkMgr::NetworkState state)
{
    switch (state) {
        case NetworkMgr::Localhost    : return "Localhost";
        case NetworkMgr::NotLocalhost : return "Not Localhost";
        //case NetworkMgr::Error      : return "Error determining state";
        case NetworkMgr::Unknown :
        default : return "Unknown";
    }
}


void MainWindow::updateMainTab() {

    qDebug ("Updating state with service %s and network %s ", getServiceStateString(m_serviceState).toLatin1().data(), getNetworkStateString(m_networkState).toLatin1().data());
    if (m_serviceState == ServiceMgr::Running &&
        m_networkState == NetworkMgr::Localhost) {
        ui->runningStatus->setText(getServiceStateString(m_serviceState));
        ui->stubbyStatus->setPixmap(*greenPixmap);
        ui->onOffSlider->setChecked(true);
    }
    else if (m_serviceState == ServiceMgr::Stopped &&
             m_networkState == NetworkMgr::NotLocalhost) {
        ui->runningStatus->setText(getServiceStateString(m_serviceState));
        ui->stubbyStatus->setPixmap(*greyPixmap);
        ui->onOffSlider->setChecked(false);
    }
    else if ((m_serviceState == ServiceMgr::Running &&
              m_networkState == NetworkMgr::NotLocalhost) ||
             (m_serviceState == ServiceMgr::Stopped &&
              m_networkState == NetworkMgr::Localhost)) {
        if (updateState == StartStop || updateState == Restart)
            ui->runningStatus->setText("Updating...");
        else
            ui->runningStatus->setText("Partly running...");
        ui->stubbyStatus->setPixmap(*yellowPixmap);
    }
    else if ((m_serviceState == ServiceMgr::Unknown &&
             m_networkState == NetworkMgr::Unknown) ||
             m_serviceState == ServiceMgr::Error ) {
             //m_networkState == NetworkMgr::Unknown) {
        ui->runningStatus->setText(getServiceStateString(m_serviceState));
        ui->stubbyStatus->setPixmap(*redPixmap);
        ui->onOffSlider->setChecked(false);
    }
    else {
        ui->runningStatus->setText("Updating...");
        ui->stubbyStatus->setPixmap(*yellowPixmap);
    }
    statusMsg("State Updated");

}
