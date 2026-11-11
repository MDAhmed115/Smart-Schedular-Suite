#pragma once

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QMarginsF>
#include <QMenu>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPen>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "engine/TournamentEngine.h"

// Third module of the Smart Scheduler Suite. Self-contained: own engine,
// own state, own grid. It shares nothing with the Timetable / Event pages
// except the application style sheet.
class TournamentPage final : public QWidget {
    Tournament t_;
    bool created_ = false;
    int selected_ = -1;

    QComboBox* modeBox = nullptr;
    QComboBox* legsBox = nullptr;
    QSpinBox* groupSpin = nullptr;
    QCheckBox* thirdCheck = nullptr;
    QPlainTextEdit* teamsEdit = nullptr;

    QLabel* banner = nullptr;
    QLabel* teamsMetric = nullptr;
    QLabel* matchesMetric = nullptr;
    QLabel* playedMetric = nullptr;
    QLabel* phaseMetric = nullptr;
    QLabel* statusLabel = nullptr;
    QLabel* gridLabel = nullptr;

    QLabel* selLabel = nullptr;
    QSpinBox* homeGoals = nullptr;
    QSpinBox* awayGoals = nullptr;
    QSpinBox* homePen = nullptr;
    QSpinBox* awayPen = nullptr;
    QLabel* penLabel = nullptr;
    QPushButton* saveBtn = nullptr;

    QTabWidget* viewTabs = nullptr;
    QComboBox* stageFilter = nullptr;
    QTableWidget* fixtureTable = nullptr;
    QComboBox* tableBox = nullptr;
    QTableWidget* standTable = nullptr;

    // ---- drawing models ---------------------------------------------------
    enum class GridPart { All, Cross, Bracket };

    struct BoxModel {
        QString top, bottom, topScore, bottomScore;
        bool topMuted = true, bottomMuted = true;
        int winnerSide = -1;       // 0 = top, 1 = bottom
        bool pending = false;      // both teams known, result missing
        bool isFinal = false;
    };
    struct BracketModel {
        std::vector<QString> titles;
        std::vector<std::vector<BoxModel>> rounds;
        bool hasThird = false;
        BoxModel third;
        QString note;
    };
    struct TableSpec {
        QString title;
        QStringList headers;
        std::vector<QStringList> rows;
        std::vector<double> weights;
        int shade = 0;
    };

    // ---- helpers ---------------------------------------------------------
    static QString q(const std::string& s) { return QString::fromStdString(s); }
    static QString groupName(int g) { return QString("Group %1").arg(QChar('A' + g)); }
    static QColor ink() { return QColor("#000000"); }
    static QColor muted() { return QColor("#525252"); }
    static QColor accent() { return QColor("#DC2626"); }
    static QColor hair() { return QColor("#D4D4D4"); }

    static QFont uiFont(int px, bool bold = false) {
        QFont f;
        f.setFamilies({"Helvetica Neue", "Helvetica", "Arial"});
        f.setPixelSize(px);
        f.setBold(bold);
        return f;
    }

    static QLabel* mutedLabel(const QString& text) {
        auto* l = new QLabel(text);
        l->setObjectName("muted");
        l->setWordWrap(true);
        return l;
    }
    static QFrame* panel() {
        auto* f = new QFrame;
        f->setObjectName("panel");
        return f;
    }
    static QLabel* heading(const QString& text) {
        auto* l = new QLabel(text);
        l->setStyleSheet("font-size:16px;font-weight:600;color:#000000;");
        return l;
    }
    static void setupTable(QTableWidget* table, const QStringList& headers) {
        table->setColumnCount(headers.size());
        table->setHorizontalHeaderLabels(headers);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->verticalHeader()->setVisible(false);
        table->setAlternatingRowColors(true);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setShowGrid(false);
        table->setMinimumHeight(240);
    }
    QWidget* metricCard(const QString& label, QLabel* value, const QString& detail) {
        auto* f = panel();
        f->setStyleSheet("QFrame#panel { background:#F5F5F5; border:none; border-top:2px solid #000000; border-radius:0; }");
        auto* l = new QVBoxLayout(f);
        l->setContentsMargins(16, 12, 16, 12);
        auto* cap = new QLabel(label);
        cap->setObjectName("eyebrow");
        value->setObjectName("metric");
        l->addWidget(cap);
        l->addWidget(value);
        l->addWidget(mutedLabel(detail));
        return f;
    }

    QString teamLabel(int team) const { return team < 0 ? QString("TBD") : q(t_.teamName(team)); }

    QString scoreText(const TMatch& m) const {
        if (!m.played || m.bye) return "-";
        QString s = QString("%1 - %2").arg(m.homeGoals).arg(m.awayGoals);
        if (m.penalties) s += QString("  (%1 - %2 p)").arg(m.homePen).arg(m.awayPen);
        return s;
    }

    TMode currentMode() const {
        switch (modeBox->currentIndex()) {
            case 1: return TMode::KnockoutGroups;
            case 2: return TMode::KnockoutDirect;
            default: return TMode::RoundRobinPlayoffs;
        }
    }

public:
    explicit TournamentPage(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumWidth(0);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(16);

        // ---- title row ----
        auto* titleRow = new QHBoxLayout;
        auto* titleCol = new QVBoxLayout;
        auto* eyebrow = new QLabel("Competitions");
        eyebrow->setObjectName("eyebrow");
        auto* title = new QLabel("Tournament planner");
        title->setObjectName("pageTitle");
        titleCol->addWidget(eyebrow);
        titleCol->addWidget(title);
        titleRow->addLayout(titleCol);
        titleRow->addStretch();

        auto* exportBtn = new QToolButton;
        exportBtn->setObjectName("exportBtn");
        exportBtn->setText("Export");
        exportBtn->setMinimumHeight(40);
        exportBtn->setCursor(Qt::PointingHandCursor);
        exportBtn->setPopupMode(QToolButton::InstantPopup);
        auto* exportMenu = new QMenu(exportBtn);
        exportMenu->addAction("PDF  (all views)", this, [this] { exportAs("pdf"); });
        exportMenu->addAction("Image  (JPG, current view)", this, [this] { exportAs("jpg"); });
        exportMenu->addSeparator();
        exportMenu->addAction("Spreadsheet  (CSV, fixtures)", this, [this] { exportAs("csv"); });
        exportBtn->setMenu(exportMenu);
        titleRow->addWidget(exportBtn);
        layout->addLayout(titleRow);

        // ---- banner ----
        auto* bannerPanel = panel();
        auto* bl = new QVBoxLayout(bannerPanel);
        bl->setContentsMargins(16, 12, 16, 12);
        banner = new QLabel("Create a tournament to begin.");
        banner->setStyleSheet("font-size:16px;font-weight:600;color:#000000;");
        banner->setWordWrap(true);
        bl->addWidget(banner);
        layout->addWidget(bannerPanel);

        // ---- metrics ----
        auto* metrics = new QHBoxLayout;
        metrics->setSpacing(16);
        teamsMetric = new QLabel("0");
        matchesMetric = new QLabel("0");
        playedMetric = new QLabel("0");
        phaseMetric = new QLabel("-");
        metrics->addWidget(metricCard("Teams", teamsMetric, "Teams in the tournament"));
        metrics->addWidget(metricCard("Matches", matchesMetric, "Matches to play so far"));
        metrics->addWidget(metricCard("Played", playedMetric, "Results entered"));
        metrics->addWidget(metricCard("Stage", phaseMetric, "Current stage"));
        layout->addLayout(metrics);

        // ---- left column ----
        auto* split = new QSplitter(Qt::Horizontal);
        split->setChildrenCollapsible(false);
        split->setMinimumHeight(560);

        auto* left = new QWidget;
        auto* ll = new QVBoxLayout(left);
        ll->setContentsMargins(0, 0, 8, 0);
        ll->setSpacing(12);

        auto* setup = new QGroupBox("Tournament setup");
        auto* sg = new QGridLayout(setup);
        sg->setVerticalSpacing(8);
        modeBox = new QComboBox;
        modeBox->addItems({"Round robin + semi-finals + final",
                           "Knockout: groups, then Round of 32",
                           "Knockout: straight to Round of 32"});
        legsBox = new QComboBox;
        legsBox->addItems({"Once (single round robin)", "Twice (home and away)"});
        groupSpin = new QSpinBox;
        groupSpin->setRange(11, 16);
        groupSpin->setValue(12);
        groupSpin->setEnabled(false);
        thirdCheck = new QCheckBox("Play a third-place match");
        thirdCheck->setChecked(true);
        teamsEdit = new QPlainTextEdit;
        teamsEdit->setPlaceholderText("One team per line");
        teamsEdit->setMinimumHeight(170);
        auto* loadBtn = new QPushButton("Load teams (CSV)");
        auto* sampleBtn = new QPushButton("Sample teams");
        auto* createBtn = new QPushButton("Create tournament");
        createBtn->setObjectName("primary");

        sg->addWidget(mutedLabel("Format"), 0, 0);
        sg->addWidget(modeBox, 1, 0);
        sg->addWidget(mutedLabel("Times each pair of teams meets"), 2, 0);
        sg->addWidget(legsBox, 3, 0);
        sg->addWidget(mutedLabel("Number of groups (World Cup 2026 = 12)"), 4, 0);
        sg->addWidget(groupSpin, 5, 0);
        sg->addWidget(thirdCheck, 6, 0);
        sg->addWidget(mutedLabel("Teams (list order = seeding / draw order)"), 7, 0);
        sg->addWidget(teamsEdit, 8, 0);
        auto* btnRow = new QHBoxLayout;
        btnRow->addWidget(loadBtn);
        btnRow->addWidget(sampleBtn);
        sg->addLayout(btnRow, 9, 0);
        sg->addWidget(createBtn, 10, 0);
        ll->addWidget(setup);

        auto* entry = new QGroupBox("Enter result");
        auto* eg = new QGridLayout(entry);
        eg->setVerticalSpacing(8);
        selLabel = mutedLabel("Select a match in the Fixtures tab.");
        homeGoals = new QSpinBox;
        awayGoals = new QSpinBox;
        homeGoals->setRange(0, 99);
        awayGoals->setRange(0, 99);
        homePen = new QSpinBox;
        awayPen = new QSpinBox;
        homePen->setRange(0, 99);
        awayPen->setRange(0, 99);
        penLabel = mutedLabel("Draw in a knockout match: enter the penalty shoot-out score");
        saveBtn = new QPushButton("Save result");
        saveBtn->setObjectName("primary");
        auto* simBtn = new QPushButton("Simulate all remaining");
        eg->addWidget(selLabel, 0, 0, 1, 2);
        eg->addWidget(mutedLabel("Home goals"), 1, 0);
        eg->addWidget(mutedLabel("Away goals"), 1, 1);
        eg->addWidget(homeGoals, 2, 0);
        eg->addWidget(awayGoals, 2, 1);
        eg->addWidget(penLabel, 3, 0, 1, 2);
        eg->addWidget(homePen, 4, 0);
        eg->addWidget(awayPen, 4, 1);
        eg->addWidget(saveBtn, 5, 0, 1, 2);
        eg->addWidget(simBtn, 6, 0, 1, 2);
        ll->addWidget(entry);
        ll->addStretch();

        auto* leftScroll = new QScrollArea;
        leftScroll->setWidgetResizable(true);
        leftScroll->setFrameShape(QFrame::NoFrame);
        leftScroll->setWidget(left);

        // ---- right column ----
        auto* right = panel();
        auto* rl = new QVBoxLayout(right);
        viewTabs = new QTabWidget;

        auto* gridScroll = new QScrollArea;
        gridScroll->setWidgetResizable(false);
        gridScroll->setFrameShape(QFrame::NoFrame);
        gridLabel = new QLabel;
        gridLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        gridScroll->setWidget(gridLabel);
        viewTabs->addTab(gridScroll, "Grid / bracket");

        auto* fixPage = new QWidget;
        auto* fl = new QVBoxLayout(fixPage);
        fl->setContentsMargins(0, 8, 0, 0);
        stageFilter = new QComboBox;
        stageFilter->addItem("All stages", -1);
        const TStage stages[] = {TStage::League, TStage::Group, TStage::R32, TStage::R16, TStage::QF,
                                 TStage::SF, TStage::ThirdPlace, TStage::Final};
        for (TStage s : stages) stageFilter->addItem(Tournament::stageName(s), static_cast<int>(s));
        fixtureTable = new QTableWidget;
        setupTable(fixtureTable, {"#", "Stage", "Group / matchday", "Home", "Away", "Score", "Status"});
        fl->addWidget(stageFilter);
        fl->addWidget(fixtureTable, 1);
        viewTabs->addTab(fixPage, "Fixtures");

        auto* standPage = new QWidget;
        auto* stl = new QVBoxLayout(standPage);
        stl->setContentsMargins(0, 8, 0, 0);
        tableBox = new QComboBox;
        standTable = new QTableWidget;
        setupTable(standTable, {"Pos", "Team", "P", "W", "D", "L", "GF", "GA", "GD", "Pts"});
        stl->addWidget(tableBox);
        stl->addWidget(mutedLabel("Shaded rows advance to the next stage."));
        stl->addWidget(standTable, 1);
        viewTabs->addTab(standPage, "Standings");

        rl->addWidget(heading("Tournament"));
        rl->addWidget(viewTabs, 1);

        split->addWidget(leftScroll);
        split->addWidget(right);
        split->setStretchFactor(0, 4);
        split->setStretchFactor(1, 6);
        layout->addWidget(split, 1);

        statusLabel = mutedLabel("Choose a format, add teams and press Create tournament.");
        layout->addWidget(statusLabel);

        // ---- wiring ----
        connect(modeBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
            legsBox->setEnabled(i != 2);
            groupSpin->setEnabled(i == 1);
        });
        connect(createBtn, &QPushButton::clicked, this, [this] { createTournament(); });
        connect(loadBtn, &QPushButton::clicked, this, [this] { loadTeamsCsv(); });
        connect(sampleBtn, &QPushButton::clicked, this, [this] { fillSampleTeams(); });
        connect(saveBtn, &QPushButton::clicked, this, [this] { saveResult(); });
        connect(simBtn, &QPushButton::clicked, this, [this] { simulateAll(); });
        connect(stageFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { applyFilter(); });
        connect(tableBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { refreshStandings(); });
        connect(fixtureTable, &QTableWidget::currentCellChanged, this,
                [this](int row, int, int, int) { selectMatch(row); });
        auto penToggle = [this](int) { updatePenVisibility(); };
        connect(homeGoals, qOverload<int>(&QSpinBox::valueChanged), this, penToggle);
        connect(awayGoals, qOverload<int>(&QSpinBox::valueChanged), this, penToggle);

        updatePenVisibility();
        setEntryEnabled(false);
    }

private:
    // ---- tournament lifecycle -------------------------------------------
    void createTournament() {
        DynArray<std::string> names;
        const QStringList lines = teamsEdit->toPlainText().split('\n');
        for (const QString& line : lines) {
            const QString name = line.trimmed();
            if (!name.isEmpty()) names.push_back(name.toStdString());
        }

        TConfig cfg;
        cfg.mode = currentMode();
        cfg.legs = legsBox->currentIndex() + 1;
        cfg.thirdPlace = thirdCheck->isChecked();
        cfg.groups = groupSpin->value();

        Tournament fresh;
        std::string error;
        if (!fresh.init(names, cfg, error)) {
            QMessageBox::warning(this, "Cannot create tournament", q(error));
            return;
        }
        t_ = fresh;
        created_ = true;
        selected_ = -1;
        rebuildTableChoices();
        refreshAll();
        statusLabel->setText("Tournament created. Pick a match in Fixtures and enter its result.");
    }

    void loadTeamsCsv() {
        const QString path = QFileDialog::getOpenFileName(this, "Load teams", QString(), "CSV or text (*.csv *.txt)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, "Load failed", "The file could not be read.");
            return;
        }
        const QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
        QStringList out;
        bool first = true;
        for (const QString& raw : lines) {
            QString cell = raw.split(',').value(0).trimmed();
            cell.remove('"');
            if (cell.isEmpty()) continue;
            if (first && cell.compare("teamName", Qt::CaseInsensitive) == 0) { first = false; continue; }
            first = false;
            out << cell;
        }
        teamsEdit->setPlainText(out.join('\n'));
    }

    void fillSampleTeams() {
        const int count = currentMode() == TMode::KnockoutGroups ? 48
                        : currentMode() == TMode::KnockoutDirect ? 32 : 8;
        QStringList out;
        for (int i = 1; i <= count; ++i) out << QString("Team %1").arg(i);
        teamsEdit->setPlainText(out.join('\n'));
    }

    // ---- results ---------------------------------------------------------
    void updatePenVisibility() {
        bool show = false;
        if (created_ && selected_ >= 0 && selected_ < t_.matchCount())
            show = Tournament::isKnockout(t_.match(selected_).stage) && homeGoals->value() == awayGoals->value();
        penLabel->setVisible(show);
        homePen->setVisible(show);
        awayPen->setVisible(show);
    }

    void setEntryEnabled(bool on) {
        homeGoals->setEnabled(on);
        awayGoals->setEnabled(on);
        homePen->setEnabled(on);
        awayPen->setEnabled(on);
        saveBtn->setEnabled(on);
    }

    void selectMatch(int row) {
        if (!created_ || row < 0 || row >= t_.matchCount()) return;
        selected_ = row;
        const TMatch& m = t_.match(row);
        if (!Tournament::playable(m)) {
            selLabel->setText(m.bye ? "Bye: this team advances automatically."
                                    : "Waiting for earlier rounds to decide both teams.");
            setEntryEnabled(false);
        } else {
            selLabel->setText(QString("%1 vs %2  (%3)").arg(teamLabel(m.home), teamLabel(m.away),
                                                            Tournament::stageName(m.stage)));
            setEntryEnabled(true);
            homeGoals->setValue(m.homeGoals);
            awayGoals->setValue(m.awayGoals);
            homePen->setValue(m.homePen);
            awayPen->setValue(m.awayPen);
        }
        updatePenVisibility();
    }

    void saveResult() {
        if (!created_ || selected_ < 0) return;
        std::string error;
        if (!t_.setResult(selected_, homeGoals->value(), awayGoals->value(), homePen->value(),
                          awayPen->value(), error)) {
            QMessageBox::warning(this, "Result not saved", q(error));
            return;
        }
        refreshAll();
        statusLabel->setText("Result saved.");
    }

    void simulateAll() {
        if (!created_) return;
        auto* rng = QRandomGenerator::global();
        bool any = true;
        int guard = 0;
        while (any && guard++ < 1000) {
            any = false;
            for (int i = 0; i < t_.matchCount(); ++i) {
                const TMatch m = t_.match(i);  // copy: the list can grow
                if (m.played || !Tournament::playable(m)) continue;
                int hg = rng->bounded(0, 4), ag = rng->bounded(0, 4), hp = 0, ap = 0;
                if (Tournament::isKnockout(m.stage) && hg == ag) {
                    const bool homeWins = rng->bounded(2) == 1;
                    hp = homeWins ? 5 : 4;
                    ap = homeWins ? 4 : 5;
                }
                std::string error;
                if (t_.setResult(i, hg, ag, hp, ap, error)) any = true;
            }
        }
        refreshAll();
        statusLabel->setText("Remaining matches simulated with random scores.");
    }

    // ---- refresh ---------------------------------------------------------
    void refreshAll() {
        refreshMetrics();
        refreshFixtures();
        refreshStandings();
        renderGrid();
        if (selected_ >= 0) selectMatch(selected_);
    }

    QString phaseText() const {
        if (!created_) return "-";
        if (t_.champion() >= 0) return "Finished";
        for (int i = 0; i < t_.matchCount(); ++i) {
            const TMatch& m = t_.match(i);
            if (!m.played && !m.bye) return Tournament::stageName(m.stage);
        }
        return "-";
    }

    void refreshMetrics() {
        int total = 0, played = 0;
        for (int i = 0; i < t_.matchCount(); ++i) {
            const TMatch& m = t_.match(i);
            if (m.bye) continue;
            ++total;
            if (m.played) ++played;
        }
        teamsMetric->setText(QString::number(t_.teamCount()));
        matchesMetric->setText(QString::number(total));
        playedMetric->setText(QString::number(played));
        phaseMetric->setText(phaseText());

        if (t_.champion() >= 0) {
            QString text = "Champion: " + teamLabel(t_.champion());
            if (t_.runnerUp() >= 0) text += "     Runner-up: " + teamLabel(t_.runnerUp());
            if (t_.thirdPlaceTeam() >= 0) text += "     Third: " + teamLabel(t_.thirdPlaceTeam());
            banner->setText(text);
        } else if (!t_.playoffsBuilt() && t_.config().mode != TMode::KnockoutDirect) {
            banner->setText(QString("League stage: %1 of %2 matches played. Playoffs are drawn automatically afterwards.")
                                .arg(played).arg(total));
        } else {
            banner->setText("Current stage: " + phaseText());
        }
    }

    QStringList fixtureRow(int i) const {
        const TMatch& m = t_.match(i);
        QString where = "-";
        if (m.stage == TStage::Group) where = QString("%1 · MD %2").arg(groupName(m.group)).arg(m.round);
        else if (m.stage == TStage::League) where = QString("MD %1").arg(m.round);
        const QString status = m.bye ? "Bye" : m.played ? "Played"
                             : Tournament::playable(m) ? "Pending" : "Waiting for teams";
        return {QString::number(i + 1), Tournament::stageName(m.stage), where,
                teamLabel(m.home), m.bye ? QString("BYE") : teamLabel(m.away),
                scoreText(m), status};
    }

    void refreshFixtures() {
        fixtureTable->blockSignals(true);
        fixtureTable->setRowCount(0);
        for (int i = 0; i < t_.matchCount(); ++i) {
            const int r = fixtureTable->rowCount();
            fixtureTable->insertRow(r);
            const QStringList vals = fixtureRow(i);
            for (int c = 0; c < vals.size(); ++c) fixtureTable->setItem(r, c, new QTableWidgetItem(vals[c]));
        }
        fixtureTable->blockSignals(false);
        applyFilter();
        if (selected_ >= 0 && selected_ < fixtureTable->rowCount()) fixtureTable->selectRow(selected_);
    }

    void applyFilter() {
        const int want = stageFilter->currentData().toInt();
        for (int i = 0; i < fixtureTable->rowCount() && i < t_.matchCount(); ++i)
            fixtureTable->setRowHidden(i, want >= 0 && static_cast<int>(t_.match(i).stage) != want);
    }

    void rebuildTableChoices() {
        tableBox->blockSignals(true);
        tableBox->clear();
        if (t_.config().mode == TMode::RoundRobinPlayoffs) {
            tableBox->addItem("League table", 0);
        } else if (t_.config().mode == TMode::KnockoutGroups) {
            for (int g = 0; g < t_.groupCount(); ++g) tableBox->addItem(groupName(g), g);
            tableBox->addItem("Best third-placed teams", -2);
        }
        tableBox->blockSignals(false);
    }

    std::vector<TStanding> rowsOf(int group) const {
        const DynArray<TStanding> d = t_.standings(group);
        std::vector<TStanding> v;
        for (std::size_t i = 0; i < d.size(); ++i) v.push_back(d[i]);
        return v;
    }

    // One standings table as plain text (used by the screen and the exports).
    TableSpec standingsSpec(int gid) const {
        TableSpec s;
        s.headers = {"Pos", "Team", "P", "W", "D", "L", "GF", "GA", "GD", "Pts"};
        s.weights = {0.07, 0.31, 0.07, 0.07, 0.07, 0.07, 0.08, 0.08, 0.08, 0.10};
        std::vector<TStanding> rows;
        if (t_.config().mode == TMode::RoundRobinPlayoffs) {
            s.title = "League table";
            rows = rowsOf(0);
            s.shade = 4;
        } else if (gid == -2) {
            s.title = "Best third-placed teams";
            for (int g = 0; g < t_.groupCount(); ++g) {
                const std::vector<TStanding> r = rowsOf(g);
                if (r.size() >= 3) rows.push_back(r[2]);
            }
            std::stable_sort(rows.begin(), rows.end(), [](const TStanding& a, const TStanding& b) {
                if (a.points != b.points) return a.points > b.points;
                if (a.gd != b.gd) return a.gd > b.gd;
                return a.gf > b.gf;
            });
            s.shade = 32 - 2 * t_.groupCount();
        } else {
            s.title = groupName(gid);
            rows = rowsOf(gid);
            s.shade = 2;
        }
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const TStanding& r = rows[i];
            s.rows.push_back({QString::number(i + 1), q(t_.teamName(r.team)), QString::number(r.played),
                              QString::number(r.won), QString::number(r.drawn), QString::number(r.lost),
                              QString::number(r.gf), QString::number(r.ga), QString::number(r.gd),
                              QString::number(r.points)});
        }
        return s;
    }

    std::vector<TableSpec> allStandingsSpecs() const {
        std::vector<TableSpec> out;
        if (!created_) return out;
        if (t_.config().mode == TMode::RoundRobinPlayoffs) {
            out.push_back(standingsSpec(0));
        } else if (t_.config().mode == TMode::KnockoutGroups) {
            for (int g = 0; g < t_.groupCount(); ++g) out.push_back(standingsSpec(g));
            out.push_back(standingsSpec(-2));
        }
        return out;
    }

    TableSpec fixtureSpec(int stageWanted) const {
        TableSpec s;
        s.title = "Fixtures";
        s.headers = {"#", "Stage", "Group / matchday", "Home", "Away", "Score", "Status"};
        s.weights = {0.05, 0.15, 0.17, 0.20, 0.20, 0.13, 0.10};
        for (int i = 0; i < t_.matchCount(); ++i) {
            if (stageWanted >= 0 && static_cast<int>(t_.match(i).stage) != stageWanted) continue;
            s.rows.push_back(fixtureRow(i));
        }
        return s;
    }

    void refreshStandings() {
        standTable->setRowCount(0);
        if (!created_ || tableBox->count() == 0) return;
        const TableSpec spec = standingsSpec(tableBox->currentData().toInt());
        for (std::size_t i = 0; i < spec.rows.size(); ++i) {
            const int r = standTable->rowCount();
            standTable->insertRow(r);
            for (int c = 0; c < spec.rows[i].size(); ++c) {
                auto* item = new QTableWidgetItem(spec.rows[i][c]);
                if (static_cast<int>(i) < spec.shade) item->setBackground(QColor("#FDE8E8"));
                standTable->setItem(r, c, item);
            }
        }
    }

    // ---- export ----------------------------------------------------------
    static QString csvCell(QString s) {
        s.replace("\"", "\"\"");
        return "\"" + s + "\"";
    }

    QString askPath(const QString& name, const QString& filter, const QString& suffix) {
        QString path = QFileDialog::getSaveFileName(this, "Export", QDir::homePath() + "/" + name, filter);
        if (path.isEmpty()) return QString();
        if (!path.endsWith("." + suffix, Qt::CaseInsensitive)) path += "." + suffix;
        return path;
    }

    void exportAs(const QString& kind) {
        if (!created_) {
            QMessageBox::information(this, "Nothing to export", "Create a tournament first.");
            return;
        }
        bool ok = false;
        QString path;
        if (kind == "pdf") {
            path = askPath("tournament.pdf", "PDF document (*.pdf)", "pdf");
            if (path.isEmpty()) return;
            ok = writePdf(path);
        } else if (kind == "jpg") {
            path = askPath("tournament.jpg", "JPEG image (*.jpg)", "jpg");
            if (path.isEmpty()) return;
            QImage img = renderCurrentViewImage();
            if (img.isNull()) {
                QMessageBox::information(this, "Nothing to export", "The current view has nothing to export.");
                return;
            }
            ok = img.convertToFormat(QImage::Format_RGB32).save(path, "JPG", 95);
        } else {
            exportCsv();
            return;
        }
        if (ok) statusLabel->setText("Exported to " + path);
        else QMessageBox::warning(this, "Export failed", "The file could not be written. Check the location and try again.");
    }

    void exportCsv() {
        QString path = askPath("tournament.csv", "CSV spreadsheet (*.csv)", "csv");
        if (path.isEmpty()) return;
        QString out = "Stage,Group,Matchday,Home,Away,Home goals,Away goals,Penalties,Winner\r\n";
        for (int i = 0; i < t_.matchCount(); ++i) {
            const TMatch& m = t_.match(i);
            if (m.bye) continue;
            out += csvCell(Tournament::stageName(m.stage)) + ",";
            out += csvCell(m.stage == TStage::Group ? groupName(m.group) : QString()) + ",";
            out += csvCell(m.stage == TStage::League || m.stage == TStage::Group ? QString::number(m.round) : QString()) + ",";
            out += csvCell(teamLabel(m.home)) + "," + csvCell(teamLabel(m.away)) + ",";
            out += m.played ? QString("%1,%2,").arg(m.homeGoals).arg(m.awayGoals) : QString(",,");
            out += csvCell(m.penalties ? QString("%1-%2").arg(m.homePen).arg(m.awayPen) : QString()) + ",";
            out += csvCell(m.winner >= 0 ? teamLabel(m.winner) : QString()) + "\r\n";
        }
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(this, "Export failed", "The file could not be written.");
            return;
        }
        f.write(QString(QChar(0xFEFF)).toUtf8());
        f.write(out.toUtf8());
        statusLabel->setText("Exported to " + path);
    }

    // Draws one text table. When `w` is given the table flows over pages.
    static void drawTable(QPainter& p, const TableSpec& t, const QRectF& page, double& y,
                          QPdfWriter* w, int fontPx) {
        const double rowH = fontPx * 2.2, titleH = fontPx * 2.8, pad = fontPx * 0.6;
        const QFont reg = uiFont(fontPx), bold = uiFont(fontPx, true);

        double sum = 0;
        for (double v : t.weights) sum += v;
        std::vector<double> cut{0.0};
        for (double v : t.weights) cut.push_back(cut.back() + v / sum);

        auto cell = [&](double yy, int c, const QString& text) {
            const QRectF r(page.left() + page.width() * cut[static_cast<std::size_t>(c)] + pad, yy,
                           page.width() * (cut[static_cast<std::size_t>(c) + 1] - cut[static_cast<std::size_t>(c)]) - 2 * pad, rowH);
            p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(p.font()).elidedText(text, Qt::ElideRight, int(r.width())));
        };
        auto header = [&] {
            p.setPen(Qt::NoPen);
            p.setBrush(Qt::black);
            p.drawRect(QRectF(page.left(), y, page.width(), rowH));
            p.setFont(bold);
            p.setPen(Qt::white);
            for (int c = 0; c < t.headers.size(); ++c) cell(y, c, t.headers[c]);
            y += rowH;
        };

        if (w && y + titleH + 2 * rowH > page.bottom()) { w->newPage(); y = page.top(); }
        p.setFont(bold);
        p.setPen(Qt::black);
        p.drawText(QRectF(page.left(), y, page.width(), titleH), Qt::AlignLeft | Qt::AlignVCenter, t.title);
        y += titleH;
        header();

        for (std::size_t i = 0; i < t.rows.size(); ++i) {
            if (w && y + rowH > page.bottom()) { w->newPage(); y = page.top(); header(); }
            p.setPen(Qt::NoPen);
            if (static_cast<int>(i) < t.shade) p.setBrush(QColor("#FDE8E8"));
            else p.setBrush(i % 2 ? QColor("#F5F5F5") : QColor("#FFFFFF"));
            p.drawRect(QRectF(page.left(), y, page.width(), rowH));
            p.setPen(QPen(QColor("#D4D4D4"), 1));
            p.drawLine(QPointF(page.left(), y + rowH), QPointF(page.right(), y + rowH));
            p.setFont(reg);
            p.setPen(Qt::black);
            for (int c = 0; c < t.rows[i].size(); ++c) cell(y, c, t.rows[i][c]);
            y += rowH;
        }
        p.setPen(QPen(Qt::black, 2));
        p.drawLine(QPointF(page.left(), y), QPointF(page.right(), y));
        y += rowH;
    }

    QImage renderSpecsImage(const std::vector<TableSpec>& specs) const {
        const int fontPx = 15, margin = 30, width = 1500;
        const double rowH = fontPx * 2.2, titleH = fontPx * 2.8;
        double height = margin * 2;
        for (const TableSpec& s : specs) height += titleH + rowH * (double(s.rows.size()) + 2.0);
        QImage img(width, std::max(200, int(std::ceil(height))), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        const QRectF page(margin, margin, width - 2 * margin, img.height() - 2 * margin);
        double y = page.top();
        for (const TableSpec& s : specs) drawTable(p, s, page, y, nullptr, fontPx);
        p.end();
        return img;
    }

    // Image export = whatever tab is on screen right now.
    QImage renderCurrentViewImage() const {
        switch (viewTabs->currentIndex()) {
            case 1: return renderSpecsImage({fixtureSpec(stageFilter->currentData().toInt())});
            case 2:
                if (tableBox->count() == 0) return QImage();
                return renderSpecsImage({standingsSpec(tableBox->currentData().toInt())});
            default: return renderGridImage(GridPart::All, 1.0);
        }
    }

    // PDF export = every view: bracket, fixtures, standings.
    bool writePdf(const QString& path) const {
        QPdfWriter w(path);
        w.setPageSize(QPageSize(QPageSize::A4));
        w.setPageOrientation(QPageLayout::Landscape);
        w.setResolution(150);
        w.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);
        w.setTitle("Tournament");
        w.setCreator("Smart Scheduler Suite");

        QPainter p(&w);
        if (!p.isActive()) return false;
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF page(0, 0, w.width(), w.height());
        bool first = true;

        auto imagePage = [&](const QString& title, const QImage& img) {
            if (!first) w.newPage();
            first = false;
            p.setFont(uiFont(30, true));
            p.setPen(Qt::black);
            p.drawText(QRectF(page.left(), page.top(), page.width(), 44), Qt::AlignLeft | Qt::AlignVCenter, title);
            const QRectF avail(page.left(), page.top() + 54, page.width(), page.height() - 54);
            const double s = std::min(avail.width() / img.width(), avail.height() / img.height());
            p.drawImage(QRectF(avail.left(), avail.top(), img.width() * s, img.height() * s), img);
        };

        const QString head = "Tournament - " + banner->text();
        if (t_.config().mode == TMode::RoundRobinPlayoffs) {
            imagePage(head + "  |  League results", renderGridImage(GridPart::Cross, 2.0));
            imagePage("Bracket", renderGridImage(GridPart::Bracket, 2.0));
        } else {
            imagePage(head + "  |  Bracket", renderGridImage(GridPart::Bracket, 2.0));
        }

        w.newPage();
        double y = page.top();
        drawTable(p, fixtureSpec(-1), page, y, &w, 20);

        const std::vector<TableSpec> specs = allStandingsSpecs();
        if (!specs.empty()) {
            w.newPage();
            y = page.top();
            for (const TableSpec& s : specs) drawTable(p, s, page, y, &w, 20);
        }
        p.end();
        return true;
    }

    // ---- bracket model ---------------------------------------------------
    BoxModel boxFromMatch(const TMatch& m) const {
        BoxModel b;
        b.top = teamLabel(m.home);
        b.bottom = m.bye ? QString("BYE") : teamLabel(m.away);
        b.topMuted = m.home < 0;
        b.bottomMuted = m.bye || m.away < 0;
        b.isFinal = m.stage == TStage::Final;
        b.pending = Tournament::playable(m) && !m.played;
        if (m.played && !m.bye) {
            b.topScore = QString::number(m.homeGoals);
            b.bottomScore = QString::number(m.awayGoals);
            if (m.penalties) {
                b.topScore += QString(" (%1)").arg(m.homePen);
                b.bottomScore += QString(" (%1)").arg(m.awayPen);
            }
            if (m.winner >= 0) b.winnerSide = (m.winner == m.home) ? 0 : 1;
        }
        return b;
    }

    static BoxModel placeholderBox(const QString& top, const QString& bottom, bool isFinal = false) {
        BoxModel b;
        b.top = top;
        b.bottom = bottom;
        b.isFinal = isFinal;
        return b;
    }

    // Real bracket when it exists, otherwise a preview of what it will look like.
    BracketModel buildBracketModel() const {
        BracketModel bm;
        int thirdId = -1, lastStage = -1;
        for (int i = 0; i < t_.matchCount(); ++i) {
            const TMatch& m = t_.match(i);
            if (m.stage == TStage::League || m.stage == TStage::Group) continue;
            if (m.stage == TStage::ThirdPlace) { thirdId = i; continue; }
            if (static_cast<int>(m.stage) != lastStage) {
                bm.rounds.emplace_back();
                bm.titles.push_back(Tournament::stageName(m.stage));
                lastStage = static_cast<int>(m.stage);
            }
            bm.rounds.back().push_back(boxFromMatch(m));
        }
        if (thirdId >= 0) {
            bm.hasThird = true;
            bm.third = boxFromMatch(t_.match(thirdId));
        }
        if (!bm.rounds.empty()) return bm;

        // ---- preview (playoffs not drawn yet) ----
        const bool wantThird = t_.config().thirdPlace;
        if (t_.config().mode == TMode::RoundRobinPlayoffs) {
            bool anyPlayed = false;
            for (int i = 0; i < t_.matchCount(); ++i)
                if (t_.match(i).stage == TStage::League && t_.match(i).played) { anyPlayed = true; break; }
            QString name[4];
            const std::vector<TStanding> rows = rowsOf(0);
            for (int i = 0; i < 4; ++i) {
                if (anyPlayed && i < int(rows.size()))
                    name[i] = QString("%1. %2").arg(i + 1).arg(q(t_.teamName(rows[static_cast<std::size_t>(i)].team)));
                else
                    name[i] = QString("League place %1").arg(i + 1);
            }
            bm.titles = {"Semi-finals", "Final"};
            bm.rounds.resize(2);
            bm.rounds[0] = {placeholderBox(name[0], name[3]), placeholderBox(name[1], name[2])};
            bm.rounds[1] = {placeholderBox("Winner SF 1", "Winner SF 2", true)};
            if (wantThird) {
                bm.hasThird = true;
                bm.third = placeholderBox("Loser SF 1", "Loser SF 2");
            }
            bm.note = "Preview: the top four of the league table. It is drawn for real when the league is complete.";
        } else {
            bm.titles = {"Round of 32", "Round of 16", "Quarter-finals", "Semi-finals", "Final"};
            const int counts[] = {16, 8, 4, 2, 1};
            bm.rounds.resize(5);
            for (int r = 0; r < 5; ++r)
                for (int k = 0; k < counts[r]; ++k)
                    bm.rounds[static_cast<std::size_t>(r)].push_back(placeholderBox("TBD", "TBD", r == 4));
            if (wantThird) {
                bm.hasThird = true;
                bm.third = placeholderBox("TBD", "TBD");
            }
            bm.note = "Preview: the Round of 32 is drawn when every group match has been played.";
        }
        return bm;
    }

    // ---- grid painting ---------------------------------------------------
    void drawBox(QPainter& p, const QRectF& r, const BoxModel& m, const QFont& reg) const {
        QFont bold = reg;
        bold.setBold(true);
        p.setPen(QPen(m.isFinal ? accent() : ink(), 1.5, m.pending ? Qt::DashLine : Qt::SolidLine));
        p.setBrush(Qt::white);
        p.drawRect(r);
        const double h = r.height() / 2.0;
        for (int side = 0; side < 2; ++side) {
            const QRectF row(r.left(), r.top() + side * h, r.width(), h);
            const bool won = m.winnerSide == side;
            if (won) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#F0F0F0"));
                p.drawRect(row.adjusted(1, 1, -1, -1));
            }
            p.setFont(won ? bold : reg);
            const bool dim = side == 0 ? m.topMuted : m.bottomMuted;
            p.setPen(dim ? muted() : ink());
            const QString& name = side == 0 ? m.top : m.bottom;
            const QRectF nameRect = row.adjusted(8, 0, -48, 0);
            p.drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
                       QFontMetrics(p.font()).elidedText(name, Qt::ElideRight, int(nameRect.width())));
            const QString& sc = side == 0 ? m.topScore : m.bottomScore;
            if (!sc.isEmpty()) {
                p.setPen(ink());
                p.drawText(row.adjusted(0, 0, -8, 0), Qt::AlignVCenter | Qt::AlignRight, sc);
            }
        }
        p.setPen(QPen(hair(), 1));
        p.drawLine(QPointF(r.left(), r.top() + h), QPointF(r.right(), r.top() + h));
    }

    QImage renderGridImage(GridPart part, qreal scale) const {
        QFont reg = uiFont(13);
        QFont bold = uiFont(13, true);
        QFont head = uiFont(15, true);

        const int margin = 24, boxW = 210, boxH = 54, gapX = 64, gapY = 16, headH = 34;
        const int n = t_.teamCount();
        const bool showCross = t_.config().mode == TMode::RoundRobinPlayoffs && part != GridPart::Bracket;
        const int nameW = 170, cell = 50, cellH = 26;
        const int crossW = showCross ? nameW + cell * n : 0;
        const int crossH = showCross ? headH + cellH * (n + 1) : 0;

        const BracketModel bm = buildBracketModel();
        const bool showBracket = part != GridPart::Cross && !bm.rounds.empty();
        const double noteH = bm.note.isEmpty() ? 0.0 : 26.0;

        std::vector<std::vector<double>> cy(bm.rounds.size());
        double bracketW = 0, bracketH = 0, thirdCy = 0;
        if (showBracket) {
            const double bodyTop = headH + noteH;
            cy[0].resize(bm.rounds[0].size());
            for (std::size_t k = 0; k < bm.rounds[0].size(); ++k)
                cy[0][k] = bodyTop + k * (boxH + gapY) + boxH / 2.0;
            for (std::size_t r = 1; r < bm.rounds.size(); ++r) {
                cy[r].resize(bm.rounds[r].size());
                const std::size_t prev = cy[r - 1].size();
                for (std::size_t k = 0; k < bm.rounds[r].size(); ++k) {
                    const double a = cy[r - 1][std::min(2 * k, prev - 1)];
                    const double b = cy[r - 1][std::min(2 * k + 1, prev - 1)];
                    cy[r][k] = (a + b) / 2.0;
                }
            }
            bracketW = double(bm.rounds.size()) * (boxW + gapX) - gapX;
            bracketH = bodyTop + double(bm.rounds[0].size()) * (boxH + gapY);
            if (bm.hasThird) {
                thirdCy = cy.back()[0] + boxH + 70;
                bracketH = std::max(bracketH, thirdCy + boxH / 2.0 + gapY);
            }
        }

        const int contentW = std::max({crossW, int(std::ceil(bracketW)), 700});
        const int imgW = margin * 2 + contentW;
        const int imgH = margin * 2 + crossH + ((showCross && showBracket) ? 40 : 0) + int(std::ceil(bracketH));
        QImage img(int(std::ceil(imgW * scale)), int(std::ceil(std::max(imgH, 120) * scale)),
                   QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        {
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            p.setRenderHint(QPainter::TextAntialiasing);
            p.scale(scale, scale);
            p.setFont(reg);

            // ---- league cross table ----
            if (showCross) {
                p.setFont(head);
                p.setPen(ink());
                p.drawText(QPointF(margin, margin + 18), "League results  (row = home team, column = away team)");
                p.setFont(reg);
                std::vector<std::vector<int>> idx(static_cast<std::size_t>(n),
                                                  std::vector<int>(static_cast<std::size_t>(n), -1));
                for (int i = 0; i < t_.matchCount(); ++i) {
                    const TMatch& m = t_.match(i);
                    if (m.stage == TStage::League && m.home >= 0 && m.away >= 0)
                        idx[static_cast<std::size_t>(m.home)][static_cast<std::size_t>(m.away)] = i;
                }
                const double x0 = margin, y0 = margin + headH;
                for (int c = 0; c < n; ++c) {
                    p.setPen(ink());
                    p.setFont(bold);
                    p.drawText(QRectF(x0 + nameW + c * cell, y0, cell, cellH), Qt::AlignCenter, QString::number(c + 1));
                }
                for (int r = 0; r < n; ++r) {
                    const double y = y0 + (r + 1) * cellH;
                    p.setFont(bold);
                    p.setPen(ink());
                    const QString label = QString("%1. %2").arg(r + 1).arg(q(t_.teamName(r)));
                    p.drawText(QRectF(x0, y, nameW - 8, cellH), Qt::AlignVCenter | Qt::AlignLeft,
                               QFontMetrics(bold).elidedText(label, Qt::ElideRight, nameW - 12));
                    p.setFont(reg);
                    for (int c = 0; c < n; ++c) {
                        const QRectF cr(x0 + nameW + c * cell, y, cell, cellH);
                        p.setPen(QPen(hair(), 1));
                        p.setBrush(r == c ? QColor("#E5E5E5") : Qt::white);
                        p.drawRect(cr);
                        const int id = idx[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
                        if (id >= 0 && t_.match(id).played) {
                            p.setPen(ink());
                            p.drawText(cr, Qt::AlignCenter,
                                       QString("%1-%2").arg(t_.match(id).homeGoals).arg(t_.match(id).awayGoals));
                        }
                    }
                }
            }

            // ---- bracket ----
            if (showBracket) {
                const double ox = margin;
                const double oy = margin + crossH + (showCross ? 40 : 0);
                if (noteH > 0) {
                    p.setFont(reg);
                    p.setPen(muted());
                    p.drawText(QRectF(ox, oy + 24, bracketW + 200, 22), Qt::AlignLeft | Qt::AlignVCenter, bm.note);
                }
                for (std::size_t r = 0; r < bm.rounds.size(); ++r) {
                    const double x = ox + r * (boxW + gapX);
                    p.setFont(bold);
                    p.setPen(ink());
                    p.drawText(QRectF(x, oy, boxW, 22), Qt::AlignLeft | Qt::AlignVCenter, bm.titles[r]);
                    for (std::size_t k = 0; k < bm.rounds[r].size(); ++k) {
                        const double c = oy + cy[r][k];
                        drawBox(p, QRectF(x, c - boxH / 2.0, boxW, boxH), bm.rounds[r][k], reg);
                        if (r + 1 < bm.rounds.size()) {
                            const std::size_t nk = std::min(k / 2, cy[r + 1].size() - 1);
                            const double pc = oy + cy[r + 1][nk];
                            const double midX = x + boxW + gapX / 2.0;
                            p.setPen(QPen(ink(), 1.2));
                            p.drawLine(QPointF(x + boxW, c), QPointF(midX, c));
                            p.drawLine(QPointF(midX, c), QPointF(midX, pc));
                            p.drawLine(QPointF(midX, pc), QPointF(x + boxW + gapX, pc));
                        }
                    }
                }
                if (bm.hasThird) {
                    const double x = ox + (bm.rounds.size() - 1) * (boxW + gapX);
                    const double c = oy + thirdCy;
                    p.setFont(bold);
                    p.setPen(ink());
                    p.drawText(QRectF(x, c - boxH / 2.0 - 24, boxW, 22), Qt::AlignLeft | Qt::AlignVCenter, "Third place");
                    drawBox(p, QRectF(x, c - boxH / 2.0, boxW, boxH), bm.third, reg);
                }
            }
        }
        return img;
    }

    void renderGrid() {
        if (!created_) {
            gridLabel->clear();
            gridLabel->resize(0, 0);
            return;
        }
        const QImage img = renderGridImage(GridPart::All, 1.0);
        gridLabel->setPixmap(QPixmap::fromImage(img));
        gridLabel->resize(img.size());
    }
};