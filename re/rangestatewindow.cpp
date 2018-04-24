#include "rangestatewindow.h"
#include "ui_rangestatewindow.h"
#include "mainwindow.h"
#include "utility.h"
#include "helpwindow.h"

RangeStateWindow::RangeStateWindow(const QVector<CANFrame> *frames, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RangeStateWindow)
{
    ui->setupUi(this);

    modelFrames = frames;

    ui->graphSignal->xAxis->setRange(0, 8);
    ui->graphSignal->yAxis->setRange(-10, 265); //run range a bit outside possible number so they aren't plotted in a hard to see place
    ui->graphSignal->xAxis->setVisible(false);
    ui->graphSignal->yAxis->setVisible(false);

    //ui->graphSignal->axisRect()->setupFullAxesBox();

    ui->cbSignalMode->addItem(tr("Big Endian"));
    ui->cbSignalMode->addItem(tr("Little Endian"));
    ui->cbSignalMode->addItem(tr("Try Both"));

    ui->cbSignedMode->addItem(tr("Signed Value"));
    ui->cbSignedMode->addItem(tr("Unsigned Value"));
    ui->cbSignedMode->addItem(tr("Try Both"));

//lambda expressions used here because these are tiny functions that don't have any reason to be full named functions
//unfortunately the fact that valueChanged is overloaded makes the syntax here HORRIBLE. That sucks.
    connect(ui->spinMaxSigSize, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [=](int newVal)
            {
                if (newVal < ui->spinMinSigSize->value()) ui->spinMinSigSize->setValue(newVal);
            });

    connect(ui->spinMinSigSize, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [=](int newVal)
            {
                if (newVal > ui->spinMaxSigSize->value()) ui->spinMaxSigSize->setValue(newVal);
            });

    connect(ui->btnAllFilter, &QAbstractButton::clicked,
            [=]()
            {
                for (int i = 0; i < ui->listFilter->count(); i++)
                {
                    QListWidgetItem *item = ui->listFilter->item(i);
                    item->setCheckState(Qt::Checked);
                    idFilters[Utility::ParseStringToNum(item->text())] = true;
                }
            });

    connect(ui->btnNoneFilter, &QAbstractButton::clicked,
            [=]()
            {
                for (int i = 0; i < ui->listFilter->count(); i++)
                {
                    QListWidgetItem *item = ui->listFilter->item(i);
                    item->setCheckState(Qt::Unchecked);
                    idFilters[Utility::ParseStringToNum(item->text())] = false;
                }
            });

    connect(ui->listFilter, &QListWidget::itemChanged,
            [=](QListWidgetItem *item)
            {
                bool isChecked = false;
                int id = Utility::ParseStringToNum(item->text());
                if (item->checkState() == Qt::Checked) isChecked = true;
                idFilters[id] = isChecked;
            });

    connect(ui->btnRecalc, &QAbstractButton::clicked, this, &RangeStateWindow::recalcButton);
    connect(MainWindow::getReference(), SIGNAL(framesUpdated(int)), this, SLOT(updatedFrames(int)));
    connect(ui->listCandidates, &QListWidget::currentRowChanged, this, &RangeStateWindow::clickedSignalList);
}

RangeStateWindow::~RangeStateWindow()
{
    delete ui;
}

void RangeStateWindow::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    readSettings();

    refreshFilterList();

    installEventFilter(this);
}

void RangeStateWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    removeEventFilter(this);
    writeSettings();
}

bool RangeStateWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_F1:
            HelpWindow::getRef()->showHelp("rangestate.html");
            break;
        }
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
    return false;
}

void RangeStateWindow::readSettings()
{
    QSettings settings;
    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        resize(settings.value("RangeStateView/WindowSize", QSize(765, 615)).toSize());
        move(settings.value("RangeStateView/WindowPos", QPoint(50, 50)).toPoint());
    }
}

void RangeStateWindow::writeSettings()
{
    QSettings settings;

    if (settings.value("Main/SaveRestorePositions", false).toBool())
    {
        settings.setValue("RangeStateView/WindowSize", size());
        settings.setValue("RangeStateView/WindowPos", pos());
    }
}

void RangeStateWindow::updatedFrames(int numFrames)
{
    CANFrame thisFrame;
    if (numFrames == -1) //all frames deleted. We don't need to do a thing on this window but erase everything in the filters section
    {
        ui->listFilter->clear();
        idFilters.clear();
    }
    else if (numFrames == -2) //all new set of frames. Reset
    {
        refreshFilterList();
    }
    else //just got some new frames. See if we need to update the filters list. Otherwise nothing to do - no recalc happens until the button is pressed
    {
        if (numFrames > modelFrames->count()) return;
        for (int i = modelFrames->count() - numFrames; i < modelFrames->count(); i++)
        {
            thisFrame = modelFrames->at(i);
            if (!idFilters.contains(thisFrame.ID))
            {
                idFilters.insert(thisFrame.ID, true);
                QListWidgetItem* listItem = new QListWidgetItem(Utility::formatCANID(thisFrame.ID, thisFrame.extended), ui->listFilter);
                listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable); // set checkable flag
                listItem->setCheckState(Qt::Checked); //default all filters to be set active
            }
        }
    }
}

void RangeStateWindow::refreshFilterList()
{
    int id;

    idFilters.clear();
    ui->listFilter->clear();

    for (int i = 0; i < modelFrames->length(); i++)
    {
        id = modelFrames->at(i).ID;
        if (!idFilters.contains(id))
        {
            idFilters.insert(id, true);
            QListWidgetItem* listItem;
            if (id < 0x800) listItem = new QListWidgetItem(Utility::formatCANID(id, false), ui->listFilter);
                else listItem = new QListWidgetItem(Utility::formatCANID(id, true), ui->listFilter);
            listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable); // set checkable flag
            listItem->setCheckState(Qt::Checked); //default all filters to be set active
        }
    }

    ui->listFilter->sortItems();
}

void RangeStateWindow::recalcButton()
{
    QHash<int, bool>::iterator iter;
    uint32_t id;

    ui->listCandidates->clear();
    foundSignals.clear();

    for (iter = idFilters.begin(); iter != idFilters.end(); ++iter)
    {
        if (iter.value() == true)
        {
            qDebug() << "Processing for ID: " << iter.key();
            //so, we're supposed to process this frame ID. We'll need to create a frame cache for it
            frameCache.clear();
            frameCache.reserve(modelFrames->count()); //block allocate more than enough space
            id = iter.key();
            for (int j = 0; j < modelFrames->count(); j++)
            {
                if (modelFrames->at(j).ID == id) frameCache.append(modelFrames->at(j));
            }
            //now we've got a list with all the same ID. Time to send it off for processing
            signalsFactory();
        }
    }

    qDebug() << "Found " << foundSignals.count() << " signals total.";
}

/*
 * Uses the settings exposed to the user to generate a set of candidate signals that should be checked.
 * The user could specify signal sizes, granularity, endian type and we generate all the permutations from there
 * Should process from max to min and stop when a valid signal is found (at least as an option) to declutter a bit.
 * Mostly what we're interested in is the largest signal that matches
*/
void RangeStateWindow::signalsFactory()
{
    int minSig = ui->spinMinSigSize->value();
    int maxSig = ui->spinMaxSigSize->value();
    int granularity = ui->spinGranularity->value();
    int sigType = ui->cbSignalMode->currentIndex() + 1;
    int signedType = ui->cbSignedMode->currentIndex() + 1;
    int maxBits = frameCache.at(0).len * 8;
    int sens = ui->slideSensitivity->value();

    for (int sigSize = maxSig; sigSize >= minSig; sigSize--)
    {
        for (int startBit = 0; startBit < maxBits; startBit += granularity)
        {
            if (sigType & 1)
            {
                if (signedType & 1) processSignal(startBit, sigSize, sens, true, true);
                if (signedType & 2) processSignal(startBit, sigSize, sens, true, false);
            }
            if (sigType & 2)
            {
                if (signedType & 1) processSignal(startBit, sigSize, sens, false, true);
                if (signedType & 2) processSignal(startBit, sigSize, sens, false, false);
            }
            //have to try both types even with 8 bit and smaller signals
            //because they could cross byte boundaries. Could check whether they
            //do and not try both types if it is impossible.
        }
    }
}

/*
 * Given the signal we generate the relevant data and figure out whether this signal seems to be a smooth range signal
*/
bool RangeStateWindow::processSignal(int startBit, int bitLength, int sensitivity, bool bigEndian, bool isSigned)
{
    qDebug() << "";
    qDebug() << "S:" << startBit << " B:" << bitLength << " Sens:" << sensitivity << " Big E:" << bigEndian << " Signed: " << isSigned;

    QVector<int> scaledVals;
    QVector<int> diff1;
    QVector<int> diff2;
    int64_t valu;
    int64_t highestValue = -1000000000000LL;
    int64_t lowestValue = 1000000000000LL;
    double multiplier;
    int numFrames = frameCache.count();

    scaledVals.reserve(frameCache.count());
    diff1.reserve(frameCache.count() - 1);
    diff2.reserve(frameCache.count() - 2);

    int i;

    for (i = 0; i < numFrames; i++)
    {
        valu = Utility::processIntegerSignal(frameCache.at(i).data, startBit, bitLength, !bigEndian, isSigned);
        if (valu < lowestValue) lowestValue = valu;
        if (valu > highestValue) highestValue = valu;
    }

    qDebug() << "Min: " << lowestValue << " Max: " << highestValue;

    if (lowestValue == highestValue) return false; //a signal that never changes is worthless and not a range signal

    //multiplier is calculated such that the range of the signal is scaled down to the value of sensitivity.
    //so, if we have a sensitivity of 50 then the range is scaled so that it is 50
    //this has the effect of smoothing the data a bit by completely removing the fine detail. It's irrelevant to
    //an algorithm like this anyway and just gets in the way.
    //Also, we're going to offset by lowest value so that everything is based at a value of 0 as the low end.
    //All of this drastically modifies the actual signal but that's OK, we're looking for patterns here not
    //being faithful to the signal (what a dirty cheater)
    int range = highestValue - lowestValue;
    multiplier = (double)sensitivity / (double)range;

    for (i = 0; i < numFrames; i++) scaledVals.append((int)((Utility::processIntegerSignal(frameCache.at(i).data, startBit, bitLength, !bigEndian, isSigned) - lowestValue) * multiplier));

    for (i = 1; i < numFrames; i++)
    {
        valu = scaledVals[i-1] - scaledVals[i];
        diff1.append(valu);
    }

    for (i = 1; i < diff1.count(); i++)
    {
        valu = diff1[i-1] - diff1[i];
        diff2.append(valu);
    }

    //now the differences are all stored so let's go through and see if they seem to suggest a ramping sort of signal or not.
    //for a first test lets let through any signal where the acceleration values are all much smaller than the signal range
    bool isGood = true;
    int comparisonValue = Utility::Lerp(sensitivity / 5, sensitivity / 40, (sensitivity - 10) / 240.0);
    qDebug() << "range: " << sensitivity << " comparisonvalue: " << comparisonValue;
    int overValues = 0;
    for (i = 0; i < diff2.count(); i++)
    {
        if (abs(diff1[i]) > comparisonValue) overValues++;
    }
    int maxOvers = Utility::Lerp(4, numFrames / 50.0, 1.0 - ((sensitivity -10) / 240.0));
    qDebug() << "Max Overs: " << maxOvers;
    if (overValues > maxOvers) isGood = false;
    qDebug() << "Is this signal good: " << isGood << " Num overs: " << overValues;
    if (isGood)
    {
        //createGraph(scaledVals);
        QString temp;
        temp = "ID: " + QString::number(frameCache.at(0).ID, 16) + " startBit: " + QString::number(startBit) + "  len: " + QString::number(bitLength);
        int64_t foundSig;
        foundSig = frameCache.at(0).ID;
        foundSig += (int64_t)startBit << 32;
        foundSig += (int64_t)bitLength << 40;

        if (isSigned)
        {
            temp += " Signed";
            foundSig += (int64_t)1 << 48;
        }
        else
        {
            temp += " Unsigned";
        }

        if (bigEndian)
        {
            temp += " BigEndian";
            foundSig += (int64_t)1 << 49;
        }
        else
        {
            temp += " LittleEndian";
        }

        ui->listCandidates->addItem(temp);
        foundSignals.append(foundSig);
    }
    return isGood;
}

//graphs the vector such that the X axis is just the index into the vector and Y is perfectly graphed within the window
void RangeStateWindow::createGraph(QVector<int> values)
{

    int tempVal;
    float minval=1000000, maxval = -100000;

    int numEntries = values.count();

    ui->graphSignal->clearGraphs();

    QVector<double> x(numEntries), y(numEntries);

    for (int j = 0; j < numEntries; j++)
    {
        tempVal = values[j];

        x[j] = j;

        y[j] = tempVal;
        if (y[j] < minval) minval = y[j];
        if (y[j] > maxval) maxval = y[j];
    }

    qDebug() << "YMin: " << minval << " YMax: " << maxval;

    double ymin, ymax;

    ymin = minval;

    if (ymin < 0) ymin *= 1.2;
    else ymin *= 0.8;

    ymax = maxval;
    if (ymax < 0) ymax *= 0.8;
    else ymax *= 1.2;

    if (fabs(ymin) < 0.01) ymin -= (ymax / 60.0);
    if (fabs(ymax) < 0.01) ymax -= (ymin / 60.0);

    qDebug() << "YFm: " << ymin << " YFM: " << ymax;

    ui->graphSignal->addGraph();
    ui->graphSignal->graph()->setName("Graph");
    ui->graphSignal->graph()->setData(x,y);
    ui->graphSignal->graph()->setLineStyle(QCPGraph::lsLine); //connect points with lines
    QPen graphPen;
    graphPen.setColor(Qt::black);
    graphPen.setWidth(1);
    ui->graphSignal->graph()->setPen(graphPen);
    ui->graphSignal->xAxis->setRange(0, numEntries);
    ui->graphSignal->yAxis->setRange(ymin, ymax);
    ui->graphSignal->replot();
}

void RangeStateWindow::clickedSignalList(int idx)
{
    if (idx == -1) return; //just in case...

    uint32_t id, startBit, bitLength;
    bool isSigned = false, isBigEndian = false;
    int64_t valu;

    valu = foundSignals.at(idx);
    id = valu & 0xFFFFFFFFULL;
    startBit = (valu >> 32) & 0xFF;
    bitLength = (valu >> 40) & 0xFF;
    if (valu & (1LL << 48)) isSigned = true;
    if (valu & (1LL << 49)) isBigEndian = true;

    qDebug() << "I:" << id << " sb:" << startBit << " len:" << bitLength << " signed:" << isSigned << " big:" << isBigEndian;

    frameCache.clear();
    frameCache.reserve(modelFrames->count()); //block allocate more than enough space

    for (int j = 0; j < modelFrames->count(); j++)
    {
        if (modelFrames->at(j).ID == id) frameCache.append(modelFrames->at(j));
    }

    int numFrames = frameCache.count();
    QVector<int> values;
    values.reserve(numFrames);
    for (int i = 0; i < numFrames; i++) values.append((int)((Utility::processIntegerSignal(frameCache.at(i).data, startBit, bitLength, !isBigEndian, isSigned))));
    createGraph(values);
}
