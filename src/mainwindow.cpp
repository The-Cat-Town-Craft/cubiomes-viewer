#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "gotodialog.h"
#include "quadlistdialog.h"
#include "aboutdialog.h"
#include "protobasedialog.h"
#include "filterdialog.h"
#include "extgendialog.h"

#include "quad.h"
#include "cutil.h"

#include <QIntValidator>
#include <QMetaType>
#include <QMessageBox>
#include <QtDebug>
#include <QDataStream>
#include <QMenu>
#include <QClipboard>
#include <QFont>
#include <QFileDialog>
#include <QTextStream>
#include <QSettings>
#include <QTreeWidget>
#include <QDateTime>
#include <QStandardPaths>
#include <QDebug>
#include <QFile>
#include <QTranslator>

// Keep the extended generator settings in global scope, but we mainly need
// them in this file. (Pass through via pointer elsewhere.)
static ExtGenSettings g_extgen;

extern "C"
int getStructureConfig_override(int stype, int mc, StructureConfig *sconf)
{
    if U(mc == INT_MAX) // to check if override is enabled in cubiomes
        mc = 0;
    int ok = getStructureConfig(stype, mc, sconf);
    if (ok && g_extgen.saltOverride)
    {
        uint64_t salt = g_extgen.salts[stype];
        if (salt <= MASK48)
            sconf->salt = salt;
    }
    return ok;
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , prevdir(".")
    , protodialog()
{
    if(QLocale::system().language() == QLocale::Chinese){
        auto translator = new QTranslator;
        translator->load(":/languages/zh_CN.qm");
        qApp->installTranslator(translator);
    }
    ui->setupUi(this);

    QCoreApplication::setApplicationName("cubiomes-viewer");

    formCond = new FormConditions(this);
    ui->collapseConstraints->init(tr("Conditions"), formCond, false);
    connect(formCond, &FormConditions::changed, this, &MainWindow::onConditionsChanged);
    ui->collapseConstraints->setInfo(
        tr("Help: Conditions"),
        tr("The search conditions define the properties by which potential seeds "
        "are filtered."
        "\n\n"
        "Conditions can reference each other to produce relative positionial "
        "dependencies (indicated with the ID in square brackets [XY]). "
        "The conditions will be checked in the same order they are listed, "
        "so make sure that references are not broken. Conditions can be reordered "
        "by dragging the list items.")
    );

    formGen48 = new FormGen48(this);
    ui->collapseGen48->init(tr("Seed generator (48-bit)"), formGen48, false);
    connect(formGen48, &FormGen48::changed, this, &MainWindow::onGen48Changed);
    ui->collapseGen48->setInfo(
        tr("Help: Seed generator"),
        tr("<html><head/><body><p>"
        "For some searches, the 48-bit structure seed candidates can be "
        "generated without searching, which can vastly reduce the search space "
        "that has to be checked."
        "</p><p>"
        "The generator mode <b>Auto</b> is recommended for general use, which "
        "automatically selects suitable options based on the conditions list."
        "</p><p>"
        "The <b>Quad-feature</b> mode produces candidates for "
        "quad&#8209;structures that have a uniform distribution of "
        "region&#8209;size=32 and chunk&#8209;gap=8, such as swamp huts."
        "</p><p>"
        "A perfect <b>Quad-monument</b> structure constellation does not "
        "actually exist, but some extremely rare structure seed bases get close, "
        "with over 90&#37; of the area within 128 blocks. The generator uses a "
        "precomputed list of these seed bases."
        "</p><p>"
        "Using a <b>Seed list</b> you can provide a custom set of 48-bit "
        "candidates. Optionally, a salt value can be added and the seeds can "
        "be region transposed."
        "</p></body></html>")
    );

    formControl = new FormSearchControl(this);
    ui->collapseControl->init(tr("Matching seeds"), formControl, false);
    connect(formControl, &FormSearchControl::selectedSeedChanged, this, &MainWindow::onSelectedSeedChanged);
    connect(formControl, &FormSearchControl::searchStatusChanged, this, &MainWindow::onSearchStatusChanged);
    ui->collapseControl->setInfo(
        tr("Help: Matching seeds"),
        tr("<html><head/><body><p>"
        "The list of seeds acts as a buffer onto which suitable seeds are added "
        "when they are found. You can also copy the seed list, or paste seeds "
        "into the list. Selecting a seed will open it in the map view."
        "</p></body></html>")
    );

    this->update();

    //ui->frameMap->layout()->addWidget(ui->toolBar);
    //ui->toolBar->setContentsMargins(0, 0, 0, 0);

    QAction *toorigin = new QAction(QIcon(":/icons/origin.png"), tr("Goto origin"), this);
    connect(toorigin, &QAction::triggered, [=](){ this->mapGoto(0,0,16); });
    ui->toolBar->addAction(toorigin);
    ui->toolBar->addSeparator();

    dimactions[0] = addMapAction(-1, "overworld", tr("Overworld"));
    dimactions[1] = addMapAction(-1, "nether", tr("Nether"));
    dimactions[2] = addMapAction(-1, "the_end", tr("End"));
    dimgroup = new QActionGroup(this);

    for (int i = 0; i < 3; i++)
    {
        connect(dimactions[i], &QAction::triggered, [=](){ this->updateMapSeed(); });
        ui->toolBar->addAction(dimactions[i]);
        dimgroup->addAction(dimactions[i]);
    }
    dimactions[0]->setChecked(true);
    ui->toolBar->addSeparator();

    saction.resize(STRUCT_NUM);
    addMapAction(D_GRID, "grid", tr("Show grid"));
    addMapAction(D_SLIME, "slime", tr("Show slime chunks"));
    addMapAction(D_SPAWN, "spawn", tr("Show world spawn"));
    addMapAction(D_STRONGHOLD, "stronghold", tr("Show strongholds"));
    addMapAction(D_VILLAGE, "village", tr("Show villages"));
    addMapAction(D_MINESHAFT, "mineshaft", tr("Show abandoned mineshafts"));
    addMapAction(D_DESERT, "desert", tr("Show desert pyramid"));
    addMapAction(D_JUNGLE, "jungle", tr("Show jungle temples"));
    addMapAction(D_HUT, "hut", tr("Show swamp huts"));
    addMapAction(D_MONUMENT, "monument", tr("Show ocean monuments"));
    addMapAction(D_IGLOO, "igloo", tr("Show igloos"));
    addMapAction(D_MANSION, "mansion", tr("Show woodland mansions"));
    addMapAction(D_RUINS, "ruins", tr("Show ocean ruins"));
    addMapAction(D_SHIPWRECK, "shipwreck", tr("Show shipwrecks"));
    addMapAction(D_TREASURE, "treasure", tr("Show buried treasures"));
    addMapAction(D_OUTPOST, "outpost", tr("Show illager outposts"));
    addMapAction(D_PORTAL, "portal", tr("Show ruined portals"));
    ui->toolBar->addSeparator();
    addMapAction(D_FORTESS, "fortress", tr("Show nether fortresses"));
    addMapAction(D_BASTION, "bastion", tr("Show bastions"));
    ui->toolBar->addSeparator();
    addMapAction(D_ENDCITY, "endcity", tr("Show end cities"));
    addMapAction(D_GATEWAY, "gateway", tr("Show end gateways"));

    saction[D_GRID]->setChecked(true);

    protodialog = new ProtoBaseDialog(this);

    ui->splitterMap->setSizes(QList<int>({6000, 10000}));
    ui->splitterSearch->setSizes(QList<int>({800, 1200, 2000}));

    qRegisterMetaType< int64_t >("int64_t");
    qRegisterMetaType< uint64_t >("uint64_t");
    qRegisterMetaType< QVector<uint64_t> >("QVector<uint64_t>");
    qRegisterMetaType< Config >("Config");

    QIntValidator *intval = new QIntValidator(this);
    ui->lineRadius->setValidator(intval);
    ui->lineEditX1->setValidator(intval);
    ui->lineEditZ1->setValidator(intval);
    ui->lineEditX2->setValidator(intval);
    ui->lineEditZ2->setValidator(intval);
    on_cboxArea_toggled(false);

    formCond->updateSensitivity();

    connect(&autosaveTimer, &QTimer::timeout, this, &MainWindow::onAutosaveTimeout);

    loadSettings();
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

QAction *MainWindow::addMapAction(int sopt, const char *iconpath, QString tip)
{
    QIcon icon;
    QString inam = QString(":icons/") + iconpath;
    icon.addPixmap(QPixmap(inam + ".png"), QIcon::Normal, QIcon::On);
    icon.addPixmap(QPixmap(inam + "_d.png"), QIcon::Normal, QIcon::Off);
    QAction *action = new QAction(icon, tip, this);
    action->setCheckable(true);
    ui->toolBar->addAction(action);
    if (sopt >= 0)
    {
        action->connect(action, &QAction::toggled, [=](bool state){ this->onActionMapToggled(sopt, state); });
        saction[sopt] = action;
    }
    return action;
}


MapView* MainWindow::getMapView()
{
    return ui->mapView;
}

bool MainWindow::getSeed(WorldInfo *wi, bool applyrand)
{
    bool ok = true;
    const std::string& mcs = ui->comboBoxMC->currentText().toStdString();
    wi->mc = str2mc(mcs.c_str());
    if (wi->mc < 0)
    {
        wi->mc = MC_NEWEST;
        qDebug() << "Unknown MC version: " << wi->mc;
        ok = false;
    }

    int v = str2seed(ui->seedEdit->text(), &wi->seed);
    if (applyrand && v == S_RANDOM)
    {
        ui->seedEdit->setText(QString::asprintf("%" PRId64, (int64_t)wi->seed));
    }

    wi->large = g_extgen.largeBiomes;

    return ok;
}

bool MainWindow::setSeed(WorldInfo wi, int dim)
{
    const char *mcstr = mc2str(wi.mc);
    if (!mcstr)
    {
        qDebug() << "Unknown MC version: " << wi.mc;
        return false;
    }

    if (dim == INT_MAX)
        dim = getDim();

    g_extgen.largeBiomes = wi.large;

    ui->comboBoxMC->setCurrentText(mcstr);
    ui->seedEdit->setText(QString::asprintf("%" PRId64, (int64_t)wi.seed));
    ui->mapView->setSeed(wi, dim);
    return true;
}

int MainWindow::getDim()
{
    QAction *active = dimgroup->checkedAction();
    if (active == dimactions[1])
        return -1; // nether
    if (active == dimactions[2])
        return +1; // end
    return 0;
}

void MainWindow::saveSettings()
{
    QSettings settings("cubiomes-viewer", "cubiomes-viewer");
    settings.setValue("mainwindow/size", size());
    settings.setValue("mainwindow/pos", pos());
    settings.setValue("mainwindow/prevdir", prevdir);
    settings.setValue("config/restoreSession", config.restoreSession);
    settings.setValue("config/showBBoxes", config.showBBoxes);
    settings.setValue("config/autosaveCycle", config.autosaveCycle);
    settings.setValue("config/smoothMotion", config.smoothMotion);
    settings.setValue("config/uistyle", config.uistyle);
    settings.setValue("config/seedsPerItem", config.seedsPerItem);
    settings.setValue("config/queueSize", config.queueSize);
    settings.setValue("config/maxMatching", config.maxMatching);

    settings.setValue("world/largeBiomes", g_extgen.largeBiomes);
    settings.setValue("world/saltOverride", g_extgen.saltOverride);
    for (int st = 0; st < FEATURE_NUM; st++)
    {
        uint64_t salt = g_extgen.salts[st];
        if (salt <= MASK48)
            settings.setValue(QString("world/salt_") + struct2str(st), (qulonglong)salt);
    }

    WorldInfo wi;
    getSeed(&wi, false);
    settings.setValue("map/mc", wi.mc);
    settings.setValue("map/large", wi.large);
    settings.setValue("map/seed", (qlonglong)wi.seed);
    settings.setValue("map/dim", getDim());
    settings.setValue("map/x", ui->mapView->getX());
    settings.setValue("map/z", ui->mapView->getZ());
    settings.setValue("map/scale", ui->mapView->getScale());
    for (int stype = 0; stype < STRUCT_NUM; stype++)
    {
        QString s = QString("map/show_") + mapopt2str(stype);
        settings.setValue(s, ui->mapView->getShow(stype));
    }
    if (config.restoreSession)
    {
        QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        saveProgress(path + "/session.save", true);
    }
}

void MainWindow::loadSettings()
{
    QSettings settings("cubiomes-viewer", "cubiomes-viewer");
    resize(settings.value("mainwindow/size", size()).toSize());
    move(settings.value("mainwindow/pos", pos()).toPoint());
    prevdir = settings.value("mainwindow/prevdir", pos()).toString();
    config.smoothMotion = settings.value("config/smoothMotion", config.smoothMotion).toBool();
    config.showBBoxes = settings.value("config/showBBoxes", config.showBBoxes).toBool();
    config.restoreSession = settings.value("config/restoreSession", config.restoreSession).toBool();
    config.autosaveCycle = settings.value("config/autosaveCycle", config.autosaveCycle).toInt();
    config.uistyle = settings.value("config/uistyle", config.uistyle).toInt();
    config.seedsPerItem = settings.value("config/seedsPerItem", config.seedsPerItem).toInt();
    config.queueSize = settings.value("config/queueSize", config.queueSize).toInt();
    config.maxMatching = settings.value("config/maxMatching", config.maxMatching).toInt();

    ui->mapView->setShowBB(config.showBBoxes);
    ui->mapView->setSmoothMotion(config.smoothMotion);
    onStyleChanged(config.uistyle);

    g_extgen.largeBiomes = settings.value("world/largeBiomes", g_extgen.largeBiomes).toBool();
    g_extgen.saltOverride = settings.value("world/saltOverride", g_extgen.saltOverride).toBool();
    for (int st = 0; st < FEATURE_NUM; st++)
    {
        QVariant v = QVariant::fromValue(~(qulonglong)0);
        g_extgen.salts[st] = settings.value(QString("world/salt_") + struct2str(st), v).toULongLong();
    }

    int dim = settings.value("map/dim", getDim()).toInt();
    if (dim == -1)
        dimactions[1]->setChecked(true);
    else if (dim == +1)
        dimactions[2]->setChecked(true);
    else
        dimactions[0]->setChecked(true);

    WorldInfo wi;
    getSeed(&wi, true);
    wi.mc = settings.value("map/mc", wi.mc).toInt();
    wi.large = settings.value("map/large", wi.large).toBool();
    wi.seed = (uint64_t) settings.value("map/seed", QVariant::fromValue((qlonglong)wi.seed)).toLongLong();
    setSeed(wi);

    qreal x = ui->mapView->getX();
    qreal z = ui->mapView->getZ();
    qreal scale = ui->mapView->getScale();

    x = settings.value("map/x", x).toDouble();
    z = settings.value("map/z", z).toDouble();
    scale = settings.value("map/scale", scale).toDouble();

    for (int sopt = 0; sopt < STRUCT_NUM; sopt++)
    {
        bool show = ui->mapView->getShow(sopt);
        QString soptstr = QString("map/show_") + mapopt2str(sopt);
        show = settings.value(soptstr, show).toBool();
        if (saction[sopt])
            saction[sopt]->setChecked(show);
        ui->mapView->setShow(sopt, show);
    }
    mapGoto(x, z, scale);

    if (config.restoreSession)
    {
        QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        loadProgress(path + "/session.save", true);
    }

    if (config.autosaveCycle > 0)
    {
        autosaveTimer.setInterval(config.autosaveCycle * 60000);
        autosaveTimer.start();
    }
    else
    {
        autosaveTimer.stop();
    }
}

bool MainWindow::saveProgress(QString fnam, bool quiet)
{
    QFile file(fnam);

    if (!file.open(QIODevice::WriteOnly))
    {
        if (!quiet)
            warning("Warning", "Failed to open file.");
        return false;
    }

    SearchConfig searchconf = formControl->getSearchConfig();
    Gen48Settings gen48 = formGen48->getSettings(false);
    QVector<Condition> condvec = formCond->getConditions();
    QVector<uint64_t> results = formControl->getResults();

    WorldInfo wi;
    getSeed(&wi);

    QTextStream stream(&file);
    stream << "#Version:  " << VERS_MAJOR << "." << VERS_MINOR << "." << VERS_PATCH << "\n";
    stream << "#Time:     " << QDateTime::currentDateTime().toString() << "\n";
    // MC version of the session should take priority over the one in the settings
    stream << "#MC:       " << mc2str(wi.mc) << "\n";

    stream << "#Search:   " << searchconf.searchtype << "\n";
    if (!searchconf.slist64path.isEmpty())
        stream << "#List64:   " << searchconf.slist64path.replace("\n", "") << "\n";
    stream << "#Progress: " << searchconf.startseed << "\n";
    stream << "#Threads:  " << searchconf.threads << "\n";
    stream << "#ResStop:  " << (int)searchconf.stoponres << "\n";

    stream << "#Mode48:   " << gen48.mode << "\n";
    if (!gen48.slist48path.isEmpty())
        stream << "#List48:   " << gen48.slist48path.replace("\n", "") << "\n";
    stream << "#HutQual:  " << gen48.qual << "\n";
    stream << "#MonArea:  " << gen48.qmarea << "\n";
    if (gen48.salt != 0)
        stream << "#Salt:     " << gen48.salt << "\n";
    if (gen48.listsalt != 0)
        stream << "#LSalt:    " << gen48.listsalt << "\n";
    if (gen48.manualarea)
    {
        stream << "#Gen48X1:  " << gen48.x1 << "\n";
        stream << "#Gen48Z1:  " << gen48.z1 << "\n";
        stream << "#Gen48X2:  " << gen48.x2 << "\n";
        stream << "#Gen48Z2:  " << gen48.z2 << "\n";
    }
    if (searchconf.smin != 0)
        stream << "#SMin:     " << searchconf.smin << "\n";
    if (searchconf.smax != ~(uint64_t)0)
        stream << "#SMax:     " << searchconf.smax << "\n";

    for (Condition &c : condvec)
        stream << "#Cond: " << QByteArray((const char*) &c, sizeof(Condition)).toHex() << "\n";

    for (uint64_t s : results)
        stream << QString::asprintf("%" PRId64 "\n", (int64_t)s);

    return true;
}

bool MainWindow::loadProgress(QString fnam, bool quiet)
{
    QFile file(fnam);

    if (!file.open(QIODevice::ReadOnly))
    {
        if (!quiet)
            warning("Warning", "Failed to open file.");
        return false;
    }

    int major = 0, minor = 0, patch = 0;
    SearchConfig searchconf = formControl->getSearchConfig();
    Gen48Settings gen48 = formGen48->getSettings(false);
    QVector<Condition> condvec;
    QVector<uint64_t> seeds;

    char buf[4096];
    int tmp;
    WorldInfo wi;
    getSeed(&wi, true);

    QTextStream stream(&file);
    QString line;
    line = stream.readLine();
    if (sscanf(line.toLatin1().data(), "#Version: %d.%d.%d", &major, &minor, &patch) != 3)
        return false;
    if (cmpVers(major, minor, patch) > 0 && !quiet)
        warning("Warning", "Progress file was created with a newer version.");

    while (stream.status() == QTextStream::Ok)
    {
        line = stream.readLine();
        QByteArray ba = line.toLatin1();
        const char *p = ba.data();

        if (line.isEmpty())
            break;

        if (line.startsWith("#Time:")) continue;
        else if (sscanf(p, "#MC:       %8[^\n]", buf) == 1)                     { wi.mc = str2mc(buf); if (wi.mc < 0) return false; }
        // SearchConfig
        else if (sscanf(p, "#Search:   %d", &searchconf.searchtype) == 1)       {}
        else if (sscanf(p, "#Progress: %" PRId64, &searchconf.startseed) == 1)  {}
        else if (sscanf(p, "#Threads:  %d", &searchconf.threads) == 1)          {}
        else if (sscanf(p, "#ResStop:  %d", &tmp) == 1)                         { searchconf.stoponres = tmp; }
        else if (line.startsWith("#List64:   "))                                { searchconf.slist64path = line.mid(11).trimmed(); }
        // Gen48Settings
        else if (sscanf(p, "#Mode48:   %d", &gen48.mode) == 1)                  {}
        else if (sscanf(p, "#HutQual:  %d", &gen48.qual) == 1)                  {}
        else if (sscanf(p, "#MonArea:  %d", &gen48.qmarea) == 1)                {}
        else if (sscanf(p, "#Salt:     %" PRIu64, &gen48.salt) == 1)            {}
        else if (sscanf(p, "#LSalt:    %" PRIu64, &gen48.listsalt) == 1)        {}
        else if (sscanf(p, "#Gen48X1:  %d", &gen48.x1) == 1)                    { gen48.manualarea = true; }
        else if (sscanf(p, "#Gen48Z1:  %d", &gen48.z1) == 1)                    { gen48.manualarea = true; }
        else if (sscanf(p, "#Gen48X2:  %d", &gen48.x2) == 1)                    { gen48.manualarea = true; }
        else if (sscanf(p, "#Gen48Z2:  %d", &gen48.z2) == 1)                    { gen48.manualarea = true; }
        else if (line.startsWith("#List48:   "))                                { gen48.slist48path = line.mid(11).trimmed(); }
        else if (sscanf(p, "#SMin:     %" PRIu64, &searchconf.smin) == 1)       {}
        else if (sscanf(p, "#SMax:     %" PRIu64, &searchconf.smax) == 1)       {}
        // Conditions
        else if (line.startsWith("#Cond:"))
        {
            QString hex = line.mid(6).trimmed();
            QByteArray ba = QByteArray::fromHex(QByteArray(hex.toLatin1().data()));
            if (ba.size() == sizeof(Condition))
            {
                Condition c = *(Condition*) ba.data();
                condvec.push_back(c);
            }
            else return false;
        }
        else
        {
            uint64_t s;
            if (sscanf(line.toLatin1().data(), "%" PRId64, (int64_t*)&s) == 1)
                seeds.push_back(s);
            else return false;
        }
    }

    setSeed(wi);

    formControl->on_buttonClear_clicked();
    formControl->searchResultsAdd(seeds, false);
    formControl->setSearchConfig(searchconf, quiet);

    formGen48->setSettings(gen48, quiet);

    formCond->on_buttonRemoveAll_clicked();
    for (Condition &c : condvec)
    {
        QListWidgetItem *item = new QListWidgetItem();
        formCond->addItemCondition(item, c);
    }

    return true;
}


void MainWindow::updateMapSeed()
{
    WorldInfo wi;
    if (getSeed(&wi))
        setSeed(wi);
}


void MainWindow::warning(QString title, QString text)
{
    QMessageBox::warning(this, title, text, QMessageBox::Ok);
}

void MainWindow::mapGoto(qreal x, qreal z, qreal scale)
{
    ui->mapView->setView(x, z, scale);
}

void MainWindow::openProtobaseMsg(QString path)
{
    protodialog->setPath(path);
    protodialog->show();
}

void MainWindow::closeProtobaseMsg()
{
    if (protodialog->closeOnDone())
        protodialog->close();
}

void MainWindow::on_comboBoxMC_currentIndexChanged(int)
{
    updateMapSeed();
    update();
}

void MainWindow::on_seedEdit_editingFinished()
{
    updateMapSeed();
    update();
}

void MainWindow::on_seedEdit_textChanged(const QString &a)
{
    uint64_t s;
    int v = str2seed(a, &s);
    switch (v)
    {
        case 0: ui->labelSeedType->setText(tr("(text)")); break;
        case 1: ui->labelSeedType->setText(tr("(numeric)")); break;
        case 2: ui->labelSeedType->setText(tr("(random)")); break;
    }
}

void MainWindow::on_actionSave_triggered()
{
    QString fnam = QFileDialog::getSaveFileName(this, tr("Save progress"), prevdir, tr("Text files (*.txt);;Any files (*)"));
    if (!fnam.isEmpty())
    {
        QFileInfo finfo(fnam);
        prevdir = finfo.absolutePath();
        saveProgress(fnam);
    }
}

void MainWindow::on_actionLoad_triggered()
{
    if (formControl->isbusy())
    {
        warning("Warning", "Cannot load progress: search is still active.");
        return;
    }
    QString fnam = QFileDialog::getOpenFileName(this, "Load progress", prevdir, "Text files (*.txt);;Any files (*)");
    if (!fnam.isEmpty())
    {
        QFileInfo finfo(fnam);
        prevdir = finfo.absolutePath();
        if (!loadProgress(fnam))
            warning("Warning", "Failed to parse progress file.");
    }
}

void MainWindow::on_actionQuit_triggered()
{
    close();
}

void MainWindow::on_actionPreferences_triggered()
{
    ConfigDialog *dialog = new ConfigDialog(this, &config);
    int status = dialog->exec();
    if (status == QDialog::Accepted)
    {
        Config oldConfig = config;
        config = dialog->getSettings();
        ui->mapView->setShowBB(config.showBBoxes);
        ui->mapView->setSmoothMotion(config.smoothMotion);
        if (oldConfig.uistyle != config.uistyle)
            onStyleChanged(config.uistyle);

        if (config.autosaveCycle)
        {
            autosaveTimer.setInterval(config.autosaveCycle * 60000);
            autosaveTimer.start();
        }
        else
        {
            autosaveTimer.stop();
        }
    }
}

void MainWindow::on_actionGo_to_triggered()
{
    GotoDialog *dialog = new GotoDialog(this, ui->mapView->getX(), ui->mapView->getZ(), ui->mapView->getScale());
    dialog->show();
}

void MainWindow::on_actionScan_seed_for_Quad_Huts_triggered()
{
    QuadListDialog *dialog = new QuadListDialog(this);
    dialog->show();
}

void MainWindow::on_actionOpen_shadow_seed_triggered()
{
    WorldInfo wi;
    if (getSeed(&wi))
    {
        wi.seed = getShadow(wi.seed);
        setSeed(wi);
    }
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog *dialog = new AboutDialog(this);
    dialog->show();
}

void MainWindow::on_actionCopy_triggered()
{
    formControl->copyResults();
}

void MainWindow::on_actionPaste_triggered()
{
    formControl->pasteResults();
}

void MainWindow::on_actionAddShadow_triggered()
{
    QVector<uint64_t> results = formControl->getResults();
    QVector<uint64_t> shadows;
    shadows.reserve(results.size());
    for (uint64_t s : results)
        shadows.push_back( getShadow(s) );
    formControl->searchResultsAdd(shadows, false);
}

void MainWindow::on_actionExtGen_triggered()
{
    ExtGenDialog *dialog = new ExtGenDialog(this, &g_extgen);
    int status = dialog->exec();
    if (status == QDialog::Accepted)
    {
        g_extgen = dialog->getSettings();
        // invalidate the map world, forcing an update
        getMapView()->deleteWorld();
        updateMapSeed();
        update();
    }
}

void MainWindow::on_mapView_customContextMenuRequested(const QPoint &pos)
{
    QMenu menu(this);
    menu.addAction("Copy coordinates", this, &MainWindow::copyCoord);
    menu.addAction("Go to coordinates...", this, &MainWindow::on_actionGo_to_triggered);
    menu.exec(ui->mapView->mapToGlobal(pos));
}

void MainWindow::on_cboxArea_toggled(bool checked)
{
    ui->labelSquareArea->setEnabled(!checked);
    ui->lineRadius->setEnabled(!checked);
    ui->labelX1->setEnabled(checked);
    ui->labelZ1->setEnabled(checked);
    ui->labelX2->setEnabled(checked);
    ui->labelZ2->setEnabled(checked);
    ui->lineEditX1->setEnabled(checked);
    ui->lineEditZ1->setEnabled(checked);
    ui->lineEditX2->setEnabled(checked);
    ui->lineEditZ2->setEnabled(checked);
}

void MainWindow::on_lineRadius_editingFinished()
{
    on_buttonAnalysis_clicked();
}

void MainWindow::on_buttonFromVisible_clicked()
{
    qreal uiw = ui->mapView->width() * ui->mapView->getScale();
    qreal uih = ui->mapView->height() * ui->mapView->getScale();
    int bx0 = (int) floor(ui->mapView->getX() - uiw/2);
    int bz0 = (int) floor(ui->mapView->getZ() - uih/2);
    int bx1 = (int) ceil(ui->mapView->getX() + uiw/2);
    int bz1 = (int) ceil(ui->mapView->getZ() + uih/2);

    ui->cboxArea->setChecked(true);
    ui->lineEditX1->setText( QString::number(bx0) );
    ui->lineEditZ1->setText( QString::number(bz0) );
    ui->lineEditX2->setText( QString::number(bx1) );
    ui->lineEditZ2->setText( QString::number(bz1) );
}

void MainWindow::on_buttonAnalysis_clicked()
{
    int x1, z1, x2, z2;

    if (ui->lineRadius->isEnabled())
    {
        int d = ui->lineRadius->text().toInt();
        x1 = (-d) >> 1;
        z1 = (-d) >> 1;
        x2 = (d) >> 1;
        z2 = (d) >> 1;
    }
    else
    {
        x1 = ui->lineEditX1->text().toInt();
        z1 = ui->lineEditZ1->text().toInt();
        x2 = ui->lineEditX2->text().toInt();
        z2 = ui->lineEditZ2->text().toInt();
    }
    if (x2 < x1 || z2 < z1)
    {
        warning(tr("Warning"), tr("Invalid area for analysis"));
        return;
    }
    if ((uint64_t)(x2 - x1) * (uint64_t)(z2 - z1) > 100000000LL)
    {
        QString msg = tr(
                    "Area for analysis is very large (%d, %d).\n"
                    "The analysis might take a while. Do you want to continue?").replace(x2-x1+1, z2-z1+1);
        int button = QMessageBox::warning(this, tr("Warning"), msg, QMessageBox::Cancel, QMessageBox::Yes);
        if (button != QMessageBox::Yes)
            return;
    }

    bool everything = ui->radioEverything->isChecked();

    WorldInfo wi;
    if (!getSeed(&wi))
        return;

    ui->buttonAnalysis->setEnabled(false);
    update();
    QApplication::processEvents();

    const int step = 512;
    long idcnt[256] = {0};
    int dim = getDim();

    LayerStack g;
    setupGeneratorLargeBiomes(&g, wi.mc, wi.large);
    applySeed(&g, wi.seed);
    int *ids = allocCache(g.entry_1, step, step);

    for (int x = x1; x <= x2; x += step)
    {
        for (int z = z1; z <= z2; z += step)
        {
            int w = x2-x+1 < step ? x2-x+1 : step;
            int h = z2-z+1 < step ? z2-z+1 : step;

            if (everything || dim == 0)
            {
                genArea(g.entry_1, ids, x, z, w, h);
                for (int i = 0; i < w*h; i++)
                    idcnt[ ids[i] & 0xff ]++;
            }
            if (everything || dim == -1)
            {
                genNetherScaled(wi.mc, wi.seed, 1, ids, x, z, w, h, 0, 0);
                for (int i = 0; i < w*h; i++)
                    idcnt[ ids[i] & 0xff ]++;
            }
            if (everything || dim == +1)
            {
                genEndScaled(wi.mc, wi.seed, 1, ids, x, z, w, h);
                for (int i = 0; i < w*h; i++)
                    idcnt[ ids[i] & 0xff ]++;
            }
        }
    }

    int bcnt = 0;
    for (int i = 0; i < 256; i++)
        bcnt += !!idcnt[i];

    free(ids);
    ids = NULL;

    QTreeWidget *tree = ui->treeAnalysis;
    while (tree->topLevelItemCount() > 0)
        delete tree->takeTopLevelItem(0);

    QTreeWidgetItem* item_cat;
    item_cat = new QTreeWidgetItem(tree);
    item_cat->setText(0, "biomes");
    item_cat->setData(1, Qt::DisplayRole, QVariant::fromValue(bcnt));

    for (int id = 0; id < 256; id++)
    {
        long cnt = idcnt[id];
        if (cnt <= 0)
            continue;
        const char *s;
        if (!(s = biome2str(id)))
            continue;
        QTreeWidgetItem* item = new QTreeWidgetItem(item_cat, QTreeWidgetItem::UserType + id);
        item->setText(0, s);
        item->setData(1, Qt::DisplayRole, QVariant::fromValue(cnt));
    }

    //tree->insertTopLevelItem(0, item_cat);

    std::vector<VarPos> st;
    for (int sopt = D_DESERT; sopt < D_SPAWN; sopt++)
    {
        int sdim = 0;
        if (sopt == D_FORTESS || sopt == D_BASTION || sopt == D_PORTALN)
            sdim = -1;
        if (sopt == D_ENDCITY || sopt == D_GATEWAY)
            sdim = 1;
        if (!everything)
        {
            if (!getMapView()->getShow(sopt))
                continue;
            if (sdim != dim)
                continue;
        }
        int stype = mapopt2stype(sopt);
        st.clear();
        StructureConfig sconf;
        if (!getStructureConfig_override(stype, wi.mc, &sconf))
            continue;
        getStructs(&st, sconf, wi, sdim, x1, z1, x2, z2);
        if (st.empty())
            continue;

        item_cat = new QTreeWidgetItem(tree);
        const char *s = struct2str(stype);
        item_cat->setText(0, s);
        item_cat->setData(1, Qt::DisplayRole, QVariant::fromValue(st.size()));

        for (size_t i = 0; i < st.size(); i++)
        {
            VarPos vp = st[i];
            QTreeWidgetItem* item = new QTreeWidgetItem(item_cat);
            item->setData(0, Qt::UserRole, QVariant::fromValue(vp.p));
            item->setText(0, QString::asprintf("%d,\t%d", vp.p.x, vp.p.z));
            if (vp.variant)
            {
                if (stype == Village)
                    item->setText(1, "abandoned");
            }
        }
        //tree->insertTopLevelItem(stype, item_cat);
    }

    if (everything || (dim == 0 && getMapView()->getShow(D_SPAWN)))
    {
        Pos pos = getSpawn(wi.mc, &g, NULL, wi.seed);
        if (pos.x >= x1 && pos.x <= x2 && pos.z >= z1 && pos.z <= z2)
        {
            item_cat = new QTreeWidgetItem(tree);
            item_cat->setText(0, "spawn");
            item_cat->setData(1, Qt::DisplayRole, QVariant::fromValue(1));
            QTreeWidgetItem* item = new QTreeWidgetItem(item_cat);
            item->setData(0, Qt::UserRole, QVariant::fromValue(pos));
            item->setText(0, QString::asprintf("%d,\t%d", pos.x, pos.z));
        }
    }

    if (everything || (dim == 0 && getMapView()->getShow(D_STRONGHOLD)))
    {
        StrongholdIter sh;
        initFirstStronghold(&sh, wi.mc, wi.seed);
        std::vector<Pos> shp;
        while (nextStronghold(&sh, &g, NULL) > 0)
        {
            Pos pos = sh.pos;
            if (pos.x >= x1 && pos.x <= x2 && pos.z >= z1 && pos.z <= z2)
                shp.push_back(pos);
        }

        if (!shp.empty())
        {
            item_cat = new QTreeWidgetItem(tree);
            item_cat->setText(0, "stronghold");
            item_cat->setData(1, Qt::DisplayRole, QVariant::fromValue(shp.size()));
            for (Pos pos : shp)
            {
                QTreeWidgetItem* item = new QTreeWidgetItem(item_cat);
                item->setData(0, Qt::UserRole, QVariant::fromValue(pos));
                item->setText(0, QString::asprintf("%d,\t%d", pos.x, pos.z));
            }
        }
    }

    ui->buttonExport->setEnabled(true);
    ui->buttonAnalysis->setEnabled(true);
}

void MainWindow::on_buttonExport_clicked()
{
    QString fnam = QFileDialog::getSaveFileName(this, tr("Export analysis"), prevdir,
        tr("Text files (*.txt *csv);;Any files (*)"));
    if (fnam.isEmpty())
        return;

    QFileInfo finfo(fnam);
    QFile file(fnam);
    prevdir = finfo.absolutePath();

    if (!file.open(QIODevice::WriteOnly))
    {
        warning(tr("Warning"), tr("Failed to open file."));
        return;
    }

    QTextStream stream(&file);

    QTreeWidgetItemIterator it(ui->treeAnalysis);
    for (; *it; ++it)
    {
        QTreeWidgetItem *item = *it;
        if (item->type() >= QTreeWidgetItem::UserType)
            stream << QString::number(item->type() - QTreeWidgetItem::UserType) << ", ";
        stream << item->text(0).replace('\t', ' ');
        if (!item->text(1).isEmpty())
            stream << ", " << item->text(1);
        stream << "\n";
    }
}

void MainWindow::on_treeAnalysis_itemDoubleClicked(QTreeWidgetItem *item)
{
    QVariant dat = item->data(0, Qt::UserRole);
    if (dat.isValid())
    {
        Pos p = qvariant_cast<Pos>(dat);
        ui->mapView->setView(p.x+0.5, p.z+0.5);
    }
}

void MainWindow::on_actionSearch_seed_list_triggered()
{
    formControl->setSearchMode(SEARCH_LIST);
}

void MainWindow::on_actionSearch_full_seed_space_triggered()
{
    formControl->setSearchMode(SEARCH_BLOCKS);
}

void MainWindow::onAutosaveTimeout()
{
    if (config.autosaveCycle)
    {
        QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        saveProgress(path + "/session.save", true);
    }
}

void MainWindow::onActionMapToggled(int sopt, bool show)
{
    if (sopt == D_PORTAL) // overworld porals should also control nether
        ui->mapView->setShow(D_PORTALN, show);
    ui->mapView->setShow(sopt, show);
}

void MainWindow::onConditionsChanged()
{
    QVector<Condition> conds = formCond->getConditions();
    formGen48->updateAutoConditions(conds);
}

void MainWindow::onGen48Changed()
{
    formGen48->updateCount();
    formControl->searchProgressReset();
}

void MainWindow::onSelectedSeedChanged(uint64_t seed)
{
    ui->seedEdit->setText(QString::asprintf("%" PRId64, (int64_t)seed));
    on_seedEdit_editingFinished();
}

void MainWindow::onSearchStatusChanged(bool running)
{
    formGen48->setEnabled(!running);
}

void MainWindow::onStyleChanged(int style)
{
    if (style == STYLE_DARK)
    {
        QFile file(":dark.qss");
        file.open(QFile::ReadOnly);
        QString st = file.readAll();
        qApp->setStyleSheet(st);
    }
    else
    {
        qApp->setStyleSheet("");
    }
}

void MainWindow::copyCoord()
{
    Pos p = ui->mapView->getActivePos();
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(QString::asprintf("%d, %d", p.x, p.z));
}

