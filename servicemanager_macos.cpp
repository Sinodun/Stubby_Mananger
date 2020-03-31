#include <QDebug>
#include <QRegularExpression>

#include "servicemanager_macos.h"
#include "runtask_macos.h"


ServiceMgrMacos::ServiceMgrMacos(MainWindow *parent) :
    ServiceMgr(parent),
    m_checkerProcess(0),
    m_checkerProcess_output("")
{
    qInfo("Creating Macos service mgr");

    m_checkerProcess = new RunHelperTaskMacos("list", QString(), QString(), this, parent);


    connect(m_checkerProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(on_checkerProcess_finished(int, QProcess::ExitStatus)));
    connect(m_checkerProcess, SIGNAL(readyReadStandardError()), this, SLOT(on_checkerProcess_readyReadStderr()));
    connect(m_checkerProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(on_checkerProcess_readyReadStdout()));
    connect(m_checkerProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(on_checkerProcess_errorOccurred(QProcess::ProcessError)));

}

ServiceMgrMacos::~ServiceMgrMacos()
{
}

int ServiceMgrMacos::getState() {
    qInfo("gettting macos state");
    m_checkerProcess_output = "";
    m_checkerProcess->start();
    return 0;
}


void ServiceMgrMacos::on_checkerProcess_finished(int exitCode, QProcess::ExitStatus exitStatus)
{

    if (exitStatus == QProcess::NormalExit) {
        /* search for "PID" = integer in the text */
        QRegularExpression pidRegEx("\"PID\" = \\d+;");
        QRegularExpressionMatch match;
        qInfo("%s", m_checkerProcess_output.toLatin1().data());
        match = pidRegEx.match(m_checkerProcess_output);
        if (match.hasMatch()) {
            m_serviceState = Running;
            emit serviceStateChanged(m_serviceState);
        } else {
            m_serviceState = Stopped;
            emit serviceStateChanged(m_serviceState);
        }
    } else {
        qInfo("Checker process failed");
        m_serviceState = Unknown;
        emit serviceStateChanged(m_serviceState);
    }
}

void ServiceMgrMacos::on_checkerProcess_readyReadStderr()
{

}

void ServiceMgrMacos::on_checkerProcess_readyReadStdout()
{
    m_checkerProcess_output = m_checkerProcess_output + QString::fromLatin1(m_checkerProcess->readAllStandardOutput().data());
}

void ServiceMgrMacos::on_checkerProcess_errorOccurred(QProcess::ProcessError)
{

}
