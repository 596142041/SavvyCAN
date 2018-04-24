#include "isotp_interpreterwindow.h"
#include "ui_isotp_interpreterwindow.h"
#include "mainwindow.h"
#include "helpwindow.h"

ISOTP_InterpreterWindow::ISOTP_InterpreterWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ISOTP_InterpreterWindow)
{
    ui->setupUi(this);
    modelFrames = frames;

    decoder = new ISOTP_HANDLER;
    udsDecoder = new UDS_HANDLER;

    decoder->setReception(true);
    decoder->setProcessAll(true);

    udsDecoder->setReception(false);

    connect(MainWindow::getReference(), &MainWindow::framesUpdated, this, &ISOTP_InterpreterWindow::updatedFrames);
    connect(MainWindow::getReference(), &MainWindow::framesUpdated, decoder, &ISOTP_HANDLER::updatedFrames);
    connect(decoder, &ISOTP_HANDLER::newISOMessage, this, &ISOTP_InterpreterWindow::newISOMessage);
    connect(udsDecoder, &UDS_HANDLER::newUDSMessage, this, &ISOTP_InterpreterWindow::newUDSMessage);
    connect(ui->listFilter, &QListWidget::itemChanged, this, &ISOTP_InterpreterWindow::listFilterItemChanged);
    connect(ui->btnAll, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::filterAll);
    connect(ui->btnNone, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::filterNone);
    connect(ui->btnCaptured, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::interpretCapturedFrames);

    connect(ui->tableIsoFrames, &QTableWidget::itemSelectionChanged, this, &ISOTP_InterpreterWindow::showDetailView);
    connect(ui->btnClearList, &QPushButton::clicked, this, &ISOTP_InterpreterWindow::clearList);
    connect(ui->cbUseExtendedAddressing, SIGNAL(toggled(bool)), this, SLOT(useExtendedAddressing(bool)));

    QStringList headers;
    headers << "Timestamp" << "ID" << "Bus" << "Dir" << "Length" << "Data";
    ui->tableIsoFrames->setColumnCount(6);
    ui->tableIsoFrames->setColumnWidth(0, 100);
    ui->tableIsoFrames->setColumnWidth(1, 50);
    ui->tableIsoFrames->setColumnWidth(2, 50);
    ui->tableIsoFrames->setColumnWidth(3, 50);
    ui->tableIsoFrames->setColumnWidth(4, 75);
    ui->tableIsoFrames->setColumnWidth(5, 200);
    ui->tableIsoFrames->setHorizontalHeaderLabels(headers);
    QHeaderView *HorzHdr = ui->tableIsoFrames->horizontalHeader();
    HorzHdr->setStretchLastSection(true);
    connect(HorzHdr, SIGNAL(sectionClicked(int)), this, SLOT(headerClicked(int)));

    decoder->setReception(true);
    decoder->setFlowCtrl(false);
    decoder->setProcessAll(true);
}

ISOTP_InterpreterWindow::~ISOTP_InterpreterWindow()
{
    delete decoder;
    delete ui;
}

void ISOTP_InterpreterWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    readSettings();
    decoder->updatedFrames(-2);
    installEventFilter(this);
}

void ISOTP_InterpreterWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    removeEventFilter(this);
    writeSettings();
}

bool ISOTP_InterpreterWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("isotp_decoder.html");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void ISOTP_InterpreterWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("ISODecodeWindow/WindowSize", this->size()).toSize());
        move(settings.value("ISODecodeWindow/WindowPos", QPoint(50, 50)).toPoint());
    }
}

void ISOTP_InterpreterWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("ISODecodeWindow/WindowSize", size());
        settings.setValue("ISODecodeWindow/WindowPos", pos());
    }
}

//erase current list then repopulate as if all the previously captured frames just came in again.
void ISOTP_InterpreterWindow::interpretCapturedFrames()
{
    clearList();
    decoder->rapidFrames(NULL, *modelFrames);
}

void ISOTP_InterpreterWindow::listFilterItemChanged(QListWidgetItem *item)
{
    if (item)
    {
        int id = item->text().toInt(NULL, 16);
        bool state = item->checkState();
        //qDebug() << id << "*" << state;
        idFilters[id] = state;
    }
}

void ISOTP_InterpreterWindow::filterAll()
{
    for (int i = 0 ; i < ui->listFilter->count(); i++)
    {
        ui->listFilter->item(i)->setCheckState(Qt::Checked);
        idFilters[ui->listFilter->item(1)->text().toInt(NULL, 16)] = true;
    }
}

void ISOTP_InterpreterWindow::filterNone()
{
    for (int i = 0 ; i < ui->listFilter->count(); i++)
    {
        ui->listFilter->item(i)->setCheckState(Qt::Unchecked);
        idFilters[ui->listFilter->item(1)->text().toInt(NULL, 16)] = false;
    }
}

void ISOTP_InterpreterWindow::clearList()
{
    qDebug() << "Clearing the table";
    ui->tableIsoFrames->clearContents();
    ui->tableIsoFrames->model()->removeRows(0, ui->tableIsoFrames->rowCount());
    messages.clear();
    //idFilters.clear();
}

void ISOTP_InterpreterWindow::useExtendedAddressing(bool checked)
{
    decoder->setExtendedAddressing(checked);
    this->interpretCapturedFrames();
}

void ISOTP_InterpreterWindow::updatedFrames(int numFrames)
{
    if (numFrames == -1) //all frames deleted. Kill the display
    {
        clearList();
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        clearList();
    }
    else //just got some new frames. See if they are relevant.
    {
    }
}

void ISOTP_InterpreterWindow::headerClicked(int logicalIndex)
{
    ui->tableIsoFrames->sortByColumn(logicalIndex);
}

void ISOTP_InterpreterWindow::showDetailView()
{
    QString buildString;
    ISOTP_MESSAGE *msg;
    int rowNum = ui->tableIsoFrames->currentRow();

    ui->txtFrameDetails->clear();
    if (rowNum == -1) return;

    msg = &messages[rowNum];

    if (msg->len != msg->data.length())
    {
        buildString.append("Message didn't have the correct number of bytes.\rExpected "
                           + QString::number(msg->len) + " got "
                           + QString::number(msg->data.length()) + "\r\r");
    }

    buildString.append(tr("Raw Payload: "));
    for (int i = 0; i < messages[rowNum].data.count(); i++)
    {
        buildString.append(Utility::formatNumber(messages[rowNum].data[i]));
        buildString.append(" ");
    }
    buildString.append("\r\r");

    ui->txtFrameDetails->setPlainText(buildString);

    //pass this frame to the UDS decoder to see if it feels it could be a UDS related message
    udsDecoder->gotISOTPFrame(messages[rowNum]);
}

void ISOTP_InterpreterWindow::newUDSMessage(UDS_MESSAGE msg)
{
    //qDebug() << "Got UDS message in ISOTP Interpreter";
    QString buildText;

    buildText = ui->txtFrameDetails->toPlainText();

    /*
    buildText.append("UDS Message:\n");
    if (msg.isErrorReply)
    {
        buildText.append("Error reply for service " + udsDecoder->getServiceShortDesc(msg.service));
        buildText.append("\nError Desc: " + udsDecoder->getNegativeResponseShort(msg.subFunc));
    }
    else
    {
        if (msg.service < 0x3F || (msg.service > 0x7F && msg.service < 0xAF))
            buildText.append("Request for service " + udsDecoder->getServiceShortDesc(msg.service) + " Sub Func: " + QString::number(msg.subFunc));
        else
            buildText.append("Response on service " + udsDecoder->getServiceShortDesc(msg.service - 0x40) + " Sub Func: " + QString::number(msg.subFunc));
    }*/

    //Much more detailed analysis than the code above. You'll like it.
    buildText.append(udsDecoder->getDetailedMessageAnalysis(msg));

    ui->txtFrameDetails->setPlainText(buildText);
}

void ISOTP_InterpreterWindow::newISOMessage(ISOTP_MESSAGE msg)
{
    int rowNum;
    QString tempString;

    if ((msg.len != msg.data.count()) && !ui->cbShowIncomplete->isChecked()) return;

    if (idFilters.find(msg.ID) == idFilters.end())
    {
        idFilters.insert(msg.ID, true);

        QListWidgetItem* listItem = new QListWidgetItem(Utility::formatCANID(msg.ID, msg.extended), ui->listFilter);
        listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable); // set checkable flag
        listItem->setCheckState(Qt::Checked);
    }
    if (!idFilters[msg.ID]) return;
    messages.append(msg);

    rowNum = ui->tableIsoFrames->rowCount();
    ui->tableIsoFrames->insertRow(rowNum);

    ui->tableIsoFrames->setItem(rowNum, 0, new QTableWidgetItem(Utility::formatTimestamp(msg.timestamp)));
    ui->tableIsoFrames->setItem(rowNum, 1, new QTableWidgetItem(QString::number(msg.ID, 16)));
    ui->tableIsoFrames->setItem(rowNum, 2, new QTableWidgetItem(QString::number(msg.bus)));
    if (msg.isReceived) ui->tableIsoFrames->setItem(rowNum, 3, new QTableWidgetItem("Rx"));
    else ui->tableIsoFrames->setItem(rowNum, 3, new QTableWidgetItem("Tx"));
    ui->tableIsoFrames->setItem(rowNum, 4, new QTableWidgetItem(QString::number(msg.len)));

    for (int i = 0; i < msg.data.count(); i++)
    {
        tempString.append(Utility::formatNumber(msg.data[i]));
        tempString.append(" ");
    }
    ui->tableIsoFrames->setItem(rowNum, 5, new QTableWidgetItem(tempString));

}


