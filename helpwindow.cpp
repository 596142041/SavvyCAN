#include <QDebug>
#include "helpwindow.h"
#include "ui_helpwindow.h"

HelpWindow* HelpWindow::self = 0;

HelpWindow::HelpWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HelpWindow)
{
    ui->setupUi(this);

    m_helpEngine = new QHelpEngineCore("SavvyCAN.qhc", this);
    if (!m_helpEngine->setupData()) {
        delete m_helpEngine;
        m_helpEngine = 0;
        qDebug() << "Could not load help file!";
    }

    readSettings();
}

HelpWindow::~HelpWindow()
{
    writeSettings();
    delete ui;
}

void HelpWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    writeSettings();
}

void HelpWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("HelpViewer/WindowSize", QSize(600, 700)).toSize());
        move(settings.value("HelpViewer/WindowPos", QPoint(50, 50)).toPoint());
    }
}

void HelpWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("HelpViewer/WindowSize", size());
        settings.setValue("HelpViewer/WindowPos", pos());
    }
}

HelpWindow* HelpWindow::getRef()
{
    if (!self)
    {
        self = new HelpWindow();
    }

    return self;
}

void HelpWindow::showHelp(QString help)
{
    if (m_helpEngine) {
        qDebug() << "Searching for " << help;
        QByteArray helpData = m_helpEngine->fileData(QUrl("qthelp://org.sphinx.savvycan.181/doc/" + help));
        ui->textHelp->setText(helpData);
    }
    else
    {
        qDebug() << "Help engine not loaded!";
    }

    readSettings();
    self->show();
}
/*

QVariant HelpBrowser::loadResource(int type, const QUrl &name)
{
QByteArray ba;
if (type < 4 && m_helpEngine) {
    QUrl url(name);
    if (name.isRelative())
        url = source().resolved(url);
    ba = m_helpEngine->fileData(url);
}
return ba;
}
*/
