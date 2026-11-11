#pragma once

#include <QAbstractItemView>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
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
#include <QLineEdit>
#include <QMarginsF>
#include <QMenu>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTextCharFormat>
#include <QTime>
#include <QTimeEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "engine/StudyPlanner.h"

// Defined in main.cpp (shared text-fitting helper used by the timetable grid).
void drawFitted(QPainter& p, const QRectF& r, const QString& title, const QString& sub,
                const QString& time, const QColor& color, int basePx, double padX, double padY);

// Snapshot of the other modules, filled by MainWindow every time the planner needs it.
struct StudyLinked {
    std::vector<StudyBusy> busy;                         // classes + university events + external events
    std::vector<std::pair<QString, QString>> courses;    // code, name from the Timetable module
    bool hasTimetable = false;
    int uniEvents = 0;
    int externalEvents = 0;
};

class StudyPlannerPage final : public QWidget {
    struct Entry {
        QString code, name;
        QDate exam;
        int difficulty = 5;
        int examStart = 9 * 60;
        int examEnd = 12 * 60;
    };

    struct DetailRow {
        long long jd = 0;
        int start = 0;
        QString date, day, time, type, title, note;
        bool accent = false;
    };

    std::vector<Entry> entries_;
    std::function<StudyLinked()> link_;
    StudyPlan plan_;
    StudyPrefs prefs_;
    std::vector<StudyCourse> planCourses_;
    StudyLinked linked_;
    bool hasPlan_ = false;
    int editing_ = -1;                    // index of the entry being edited, -1 = adding
    std::vector<long long> weekStarts_;   // Julian day of each Monday shown

    QLineEdit *code = nullptr, *name = nullptr;
    QDateEdit* examDate = nullptr;
    QTimeEdit *examStartEdit = nullptr, *examEndEdit = nullptr;
    QSpinBox* difficulty = nullptr;
    QTimeEdit *freeStart = nullptr, *latestEnd = nullptr, *dinnerStart = nullptr;
    QCheckBox* pastMidnight = nullptr;
    QSpinBox *breakAfter = nullptr, *focus = nullptr, *shortBreak = nullptr;
    QSpinBox *weekdayMax = nullptr, *freeDayMax = nullptr, *dinnerLen = nullptr;
    QSpinBox *examBefore = nullptr, *examAfter = nullptr;
    QComboBox* restDay = nullptr;
    QCheckBox* gapsCheck = nullptr;
    QPushButton *addBtn = nullptr, *cancelEditBtn = nullptr;

    QTableWidget* table = nullptr;
    QLabel *coursesMetric = nullptr, *hoursMetric = nullptr, *examsMetric = nullptr, *nextMetric = nullptr;
    QLabel *banner = nullptr, *statusLabel = nullptr, *gridLabel = nullptr;
    QComboBox* weekBox = nullptr;
    QTextBrowser* textView = nullptr;

    // ---- helpers ---------------------------------------------------------
    static QString q(const std::string& s) { return QString::fromStdString(s); }
    static QLabel* mutedLabel(const QString& t) {
        auto* l = new QLabel(t);
        l->setObjectName("muted");
        l->setWordWrap(true);
        return l;
    }
    static QFrame* panel() {
        auto* f = new QFrame;
        f->setObjectName("panel");
        return f;
    }
    static QLabel* heading(const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet("font-size:16px;font-weight:600;color:#000000;");
        return l;
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
    static QFont uiFont(int px, bool bold = false) {
        QFont f;
        f.setFamilies({"Helvetica Neue", "Helvetica", "Arial"});
        f.setPixelSize(px);
        f.setBold(bold);
        return f;
    }
    // Minutes can pass 24:00 when studying continues after midnight: 1560 -> "02:00 +1".
    static QString timeText(int m) {
        const int mm = m % 1440;
        QString s = QString("%1:%2").arg(mm / 60, 2, 10, QChar('0')).arg(mm % 60, 2, 10, QChar('0'));
        if (m >= 1440) s += " +1";
        return s;
    }
    static QString axisTime(int m) {
        const int mm = m % 1440;
        return QString("%1:%2").arg(mm / 60, 2, 10, QChar('0')).arg(mm % 60, 2, 10, QChar('0'));
    }
    static QString dur(int m) {
        if (m < 60) return QString::number(m) + " min";
        return m % 60 ? QString("%1 h %2 min").arg(m / 60).arg(m % 60) : QString("%1 h").arg(m / 60);
    }
    static QString dayShort(int d) {
        static const QStringList n = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        return n.value(d, "?");
    }
    static QDate dateOf(long long jd) { return QDate::fromJulianDay(jd); }
    static QColor courseColor(int i) {
        static const QList<QColor> base = {QColor("#000000"), QColor("#595959"), QColor("#262626"), QColor("#737373"),
                                           QColor("#404040"), QColor("#8C8C8C"), QColor("#1F1F1F"), QColor("#666666")};
        return base[i % base.size()];
    }
    static QString kindTag(BlockKind k) {
        switch (k) {
            case BlockKind::ExamEve: return "Exam eve";
            case BlockKind::ExamDay: return "Exam day";
            case BlockKind::Revision: return "Revision";
            default: return "Study";
        }
    }
    static QString busyName(BusyKind k) {
        return k == BusyKind::Class ? "Class" : k == BusyKind::UniEvent ? "University event" : "External event";
    }
    int nowMinutes() const { return QTime::currentTime().hour() * 60 + QTime::currentTime().minute(); }
    static int mins(const QTimeEdit* t) { return t->time().hour() * 60 + t->time().minute(); }
    static QTime toTime(int m) { return QTime(std::min(m, 1439) / 60, std::min(m, 1439) % 60); }

public:
    explicit StudyPlannerPage(std::function<StudyLinked()> link, QWidget* parent = nullptr)
        : QWidget(parent), link_(std::move(link)) {
        setMinimumWidth(0);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(20);

        // ---- title row ----
        auto* titleRow = new QHBoxLayout;
        auto* titleCol = new QVBoxLayout;
        auto* eyebrow = new QLabel("Exam preparation");
        eyebrow->setObjectName("eyebrow");
        auto* title = new QLabel("Study planner");
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
        auto* menu = new QMenu(exportBtn);
        menu->addAction("PDF  (grid, timetable, events, details)", this, [this] { exportAs("pdf"); });
        menu->addAction("Image  (JPG, shown week)", this, [this] { exportAs("jpg"); });
        menu->addAction("Spreadsheet  (CSV, everything)", this, [this] { exportAs("csv"); });
        exportBtn->setMenu(menu);
        auto* generate = new QPushButton("Generate study plan");
        generate->setObjectName("primary");
        generate->setMinimumHeight(40);
        generate->setCursor(Qt::PointingHandCursor);
        titleRow->addWidget(exportBtn);
        titleRow->addSpacing(8);
        titleRow->addWidget(generate);
        layout->addLayout(titleRow);

        // ---- banner ----
        auto* bannerPanel = panel();
        auto* bl = new QVBoxLayout(bannerPanel);
        bl->setContentsMargins(20, 14, 20, 14);
        banner = new QLabel("Add your courses with exam dates, times and difficulty, then press Generate.");
        banner->setStyleSheet("font-size:14px;font-weight:600;color:#000000;");
        banner->setWordWrap(true);
        bl->addWidget(banner);
        layout->addWidget(bannerPanel);

        // ---- metrics ----
        auto* metrics = new QHBoxLayout;
        metrics->setSpacing(16);
        coursesMetric = new QLabel("0");
        hoursMetric = new QLabel("-");
        examsMetric = new QLabel("-");
        nextMetric = new QLabel("-");
        metrics->addWidget(metricCard("Courses", coursesMetric, "Courses with an exam"));
        metrics->addWidget(metricCard("Study time", hoursMetric, "Planned before the last exam"));
        metrics->addWidget(metricCard("Study days", examsMetric, "Days with study blocks"));
        metrics->addWidget(metricCard("Next exam", nextMetric, "Days until the first exam"));
        layout->addLayout(metrics);

        auto* split = new QSplitter(Qt::Horizontal);
        split->setChildrenCollapsible(false);
        split->setMinimumHeight(720);

        // ---- left column ----
        auto* left = new QWidget;
        auto* ll = new QVBoxLayout(left);
        ll->setContentsMargins(0, 0, 16, 0);
        ll->setSpacing(16);

        auto* form = new QGroupBox("Add course and exam");
        auto* fg = new QGridLayout(form);
        fg->setHorizontalSpacing(16);
        fg->setVerticalSpacing(10);
        code = new QLineEdit;
        code->setPlaceholderText("e.g. CS2001");
        name = new QLineEdit;
        name->setPlaceholderText("Data Structures");
        examDate = new QDateEdit(QDate::currentDate().addDays(14));
        examDate->setCalendarPopup(true);
        examDate->setDisplayFormat("d MMM yyyy");
        examDate->setMinimumDate(QDate::currentDate().addDays(1));
        styleCalendar();
        difficulty = new QSpinBox;
        difficulty->setRange(1, 10);
        difficulty->setValue(5);
        difficulty->setSuffix(" / 10");
        examStartEdit = new QTimeEdit(QTime(9, 0));
        examStartEdit->setDisplayFormat("HH:mm");
        examEndEdit = new QTimeEdit(QTime(12, 0));
        examEndEdit->setDisplayFormat("HH:mm");
        addBtn = new QPushButton("Add course");
        addBtn->setObjectName("primary");
        cancelEditBtn = new QPushButton("Cancel edit");
        cancelEditBtn->setObjectName("quiet");
        cancelEditBtn->setVisible(false);
        auto* importBtn = new QPushButton("Import courses from Timetable");
        fg->addWidget(mutedLabel("Course code"), 0, 0);
        fg->addWidget(mutedLabel("Course name"), 0, 1);
        fg->addWidget(code, 1, 0);
        fg->addWidget(name, 1, 1);
        fg->addWidget(mutedLabel("Exam date"), 2, 0);
        fg->addWidget(mutedLabel("Difficulty (10 = hardest)"), 2, 1);
        fg->addWidget(examDate, 3, 0);
        fg->addWidget(difficulty, 3, 1);
        fg->addWidget(mutedLabel("Exam starts"), 4, 0);
        fg->addWidget(mutedLabel("Exam ends"), 4, 1);
        fg->addWidget(examStartEdit, 5, 0);
        fg->addWidget(examEndEdit, 5, 1);
        fg->addWidget(addBtn, 6, 0, 1, 2);
        fg->addWidget(cancelEditBtn, 7, 0, 1, 2);
        fg->addWidget(importBtn, 8, 0, 1, 2);
        fg->addWidget(mutedLabel("Select a course in the list and press Edit (or double-click it) to change its exam date, time or difficulty. "
                                 "There are no classes on an exam day, but you still study before and after the exam."), 9, 0, 1, 2);
        ll->addWidget(form);

        auto* habits = new QGroupBox("Study habits");
        auto* hg = new QGridLayout(habits);
        hg->setHorizontalSpacing(16);
        hg->setVerticalSpacing(10);
        freeStart = new QTimeEdit(QTime(9, 0));
        freeStart->setDisplayFormat("HH:mm");
        latestEnd = new QTimeEdit(QTime(22, 30));
        latestEnd->setDisplayFormat("HH:mm");
        pastMidnight = new QCheckBox("Continue studying after midnight (into the next day)");
        dinnerStart = new QTimeEdit(QTime(20, 0));
        dinnerStart->setDisplayFormat("HH:mm");
        breakAfter = new QSpinBox;
        breakAfter->setRange(0, 240);
        breakAfter->setSingleStep(15);
        breakAfter->setValue(60);
        breakAfter->setSuffix(" min");
        focus = new QSpinBox;
        focus->setRange(25, 120);
        focus->setSingleStep(5);
        focus->setValue(50);
        focus->setSuffix(" min");
        shortBreak = new QSpinBox;
        shortBreak->setRange(5, 30);
        shortBreak->setSingleStep(5);
        shortBreak->setValue(10);
        shortBreak->setSuffix(" min");
        weekdayMax = new QSpinBox;
        weekdayMax->setRange(60, 600);
        weekdayMax->setSingleStep(30);
        weekdayMax->setValue(240);
        weekdayMax->setSuffix(" min");
        freeDayMax = new QSpinBox;
        freeDayMax->setRange(60, 720);
        freeDayMax->setSingleStep(30);
        freeDayMax->setValue(420);
        freeDayMax->setSuffix(" min");
        dinnerLen = new QSpinBox;
        dinnerLen->setRange(30, 120);
        dinnerLen->setSingleStep(15);
        dinnerLen->setValue(60);
        dinnerLen->setSuffix(" min");
        examBefore = new QSpinBox;
        examBefore->setRange(0, 180);
        examBefore->setSingleStep(15);
        examBefore->setValue(30);
        examBefore->setSuffix(" min");
        examAfter = new QSpinBox;
        examAfter->setRange(0, 240);
        examAfter->setSingleStep(15);
        examAfter->setValue(45);
        examAfter->setSuffix(" min");
        restDay = new QComboBox;
        restDay->addItems({"No rest day", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"});
        gapsCheck = new QCheckBox("Use long free periods between classes");
        gapsCheck->setChecked(true);
        int r = 0;
        auto row = [&](const QString& label, QWidget* w) {
            hg->addWidget(mutedLabel(label), r, 0);
            hg->addWidget(w, r, 1);
            ++r;
        };
        row("Rest after the last class", breakAfter);
        row("Study starts on free days", freeStart);
        row("Stop studying at", latestEnd);
        hg->addWidget(pastMidnight, r++, 0, 1, 2);
        row("One study block", focus);
        row("Short break", shortBreak);
        row("Max study, class days", weekdayMax);
        row("Max study, free days", freeDayMax);
        row("Stop before an exam", examBefore);
        row("Rest after an exam", examAfter);
        row("Dinner at", dinnerStart);
        row("Dinner length", dinnerLen);
        row("Rest day", restDay);
        hg->addWidget(gapsCheck, r++, 0, 1, 2);
        hg->addWidget(mutedLabel("Study starts after your last class or university event plus the rest time, "
                                 "works around external events and meals, and never fills the whole day. "
                                 "On an exam day there are no classes: you study before the exam (its own course first) "
                                 "and after it (for the next exam). With the midnight option on, \"Stop studying at\" "
                                 "is a time on the next day (e.g. 02:00 means 2 am of the following day)."), r, 0, 1, 2);
        ll->addWidget(habits);
        ll->addStretch();

        auto* leftScroll = new QScrollArea;
        leftScroll->setWidgetResizable(true);
        leftScroll->setFrameShape(QFrame::NoFrame);
        leftScroll->setMinimumWidth(400);
        leftScroll->setWidget(left);

        // ---- right column ----
        auto* right = new QWidget;
        auto* rl = new QVBoxLayout(right);
        rl->setContentsMargins(16, 0, 0, 0);
        rl->setSpacing(16);

        auto* listPanel = panel();
        auto* lpl = new QVBoxLayout(listPanel);
        lpl->setContentsMargins(20, 18, 20, 18);
        lpl->setSpacing(10);
        lpl->addWidget(heading("Exam list"));
        lpl->addWidget(mutedLabel("Each course with its exam date, time and difficulty"));
        table = new QTableWidget;
        table->setColumnCount(7);
        table->setHorizontalHeaderLabels({"Course", "Name", "Exam", "Time", "Difficulty", "Days left", "Planned"});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->verticalHeader()->setVisible(false);
        table->verticalHeader()->setDefaultSectionSize(40);
        table->setAlternatingRowColors(true);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setShowGrid(false);
        table->setMinimumHeight(190);
        lpl->addWidget(table);
        auto* btnRow = new QHBoxLayout;
        auto* editBtn = new QPushButton("Edit selected");
        editBtn->setObjectName("quiet");
        auto* removeBtn = new QPushButton("Remove selected");
        removeBtn->setObjectName("quiet");
        auto* clearBtn = new QPushButton("Clear all");
        clearBtn->setObjectName("quiet");
        btnRow->addStretch();
        btnRow->addWidget(clearBtn);
        btnRow->addWidget(removeBtn);
        btnRow->addWidget(editBtn);
        lpl->addLayout(btnRow);
        rl->addWidget(listPanel, 1);

        auto* resultPanel = panel();
        auto* rpl = new QVBoxLayout(resultPanel);
        rpl->setContentsMargins(20, 18, 20, 18);
        rpl->setSpacing(10);
        rpl->addWidget(heading("Study schedule"));
        rpl->addWidget(mutedLabel("Grey = classes, red = university events, dark red = external events, dark blocks = study, red outline = exam."));
        auto* tabsView = new QTabWidget;
        auto* gridPage = new QWidget;
        auto* gpl = new QVBoxLayout(gridPage);
        gpl->setContentsMargins(0, 10, 0, 0);
        gpl->setSpacing(10);
        weekBox = new QComboBox;
        auto* gridScroll = new QScrollArea;
        gridScroll->setWidgetResizable(false);
        gridScroll->setFrameShape(QFrame::NoFrame);
        gridLabel = new QLabel;
        gridLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        gridScroll->setWidget(gridLabel);
        gpl->addWidget(weekBox);
        gpl->addWidget(gridScroll, 1);
        textView = new QTextBrowser;
        textView->setMinimumHeight(240);
        textView->setHtml(placeholder("Your study plan will appear here", "Add courses and press Generate."));
        tabsView->addTab(gridPage, "Grid view");
        tabsView->addTab(textView, "Day plan");
        rpl->addWidget(tabsView, 1);
        rl->addWidget(resultPanel, 3);

        split->addWidget(leftScroll);
        split->addWidget(right);
        split->setStretchFactor(0, 4);
        split->setStretchFactor(1, 6);
        layout->addWidget(split, 1);

        statusLabel = mutedLabel("The planner reads your timetable, university events and external events.");
        layout->addWidget(statusLabel);

        connect(addBtn, &QPushButton::clicked, this, [this] { addCourse(); });
        connect(cancelEditBtn, &QPushButton::clicked, this, [this] { endEdit(); });
        connect(code, &QLineEdit::returnPressed, this, [this] { addCourse(); });
        connect(importBtn, &QPushButton::clicked, this, [this] { importCourses(); });
        connect(editBtn, &QPushButton::clicked, this, [this] { editSelected(); });
        connect(table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editSelected(); });
        connect(removeBtn, &QPushButton::clicked, this, [this] { removeSelected(); });
        connect(clearBtn, &QPushButton::clicked, this, [this] {
            endEdit();
            entries_.clear();
            invalidate();
        });
        connect(generate, &QPushButton::clicked, this, [this] { generatePlan(); });
        connect(weekBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { renderGrid(); });
        connect(pastMidnight, &QCheckBox::toggled, this, [this](bool on) {
            if (on) {
                latestEnd->setTimeRange(QTime(0, 0), QTime(8, 0));
                latestEnd->setTime(QTime(2, 0));
            } else {
                latestEnd->setTimeRange(QTime(0, 0), QTime(23, 59));
                latestEnd->setTime(QTime(22, 30));
            }
        });
        refreshTable();
    }

private:
    static QString placeholder(const QString& head, const QString& body) {
        return "<div style='color:#525252;padding:24px;text-align:center'><h2 style='color:#000000'>" + head +
               "</h2><p>" + body + "</p></div>";
    }

    // Makes the date popup readable regardless of the system (dark) theme.
    void styleCalendar() {
        QCalendarWidget* cal = examDate->calendarWidget();
        if (!cal) return;
        cal->setStyleSheet(
            "QCalendarWidget { background:#FFFFFF; color:#000000; border:1px solid #000000; }"
            "QCalendarWidget QWidget#qt_calendar_navigationbar { background:#000000; }"
            "QCalendarWidget QToolButton { color:#FFFFFF; background:transparent; border:none; padding:4px 8px; font-weight:600; }"
            "QCalendarWidget QToolButton:hover { background:#DC2626; }"
            "QCalendarWidget QSpinBox { color:#FFFFFF; background:#000000; border:none; selection-background-color:#DC2626; selection-color:#FFFFFF; }"
            "QCalendarWidget QMenu { background:#FFFFFF; color:#000000; }"
            "QCalendarWidget QAbstractItemView { background:#FFFFFF; color:#000000; "
            "selection-background-color:#000000; selection-color:#FFFFFF; outline:0; }"
            "QCalendarWidget QAbstractItemView:disabled { color:#A3A3A3; }");
        cal->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
        cal->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
        QTextCharFormat normal, weekend, header;
        normal.setForeground(QColor("#000000"));
        weekend.setForeground(QColor("#DC2626"));
        for (int d = 1; d <= 7; ++d) {
            const Qt::DayOfWeek dow = static_cast<Qt::DayOfWeek>(d);
            cal->setWeekdayTextFormat(dow, (d >= 6) ? weekend : normal);
        }
        header.setForeground(QColor("#000000"));
        header.setBackground(QColor("#F5F5F5"));
        cal->setHeaderTextFormat(header);
    }

    // ---- inputs -----------------------------------------------------------
    void endEdit() {
        editing_ = -1;
        addBtn->setText("Add course");
        cancelEditBtn->setVisible(false);
        examDate->setMinimumDate(QDate::currentDate().addDays(1));
        code->clear();
        name->clear();
        difficulty->setValue(5);
        examDate->setDate(QDate::currentDate().addDays(14));
        examStartEdit->setTime(QTime(9, 0));
        examEndEdit->setTime(QTime(12, 0));
    }

    void editSelected() {
        const int row = table->currentRow();
        if (row < 0 || row >= int(entries_.size())) {
            QMessageBox::information(this, "Select a course", "Select a course in the exam list first.");
            return;
        }
        const Entry& e = entries_[static_cast<std::size_t>(row)];
        editing_ = row;
        examDate->setMinimumDate(std::min(QDate::currentDate().addDays(1), e.exam));
        code->setText(e.code);
        name->setText(e.name);
        examDate->setDate(e.exam);
        difficulty->setValue(e.difficulty);
        examStartEdit->setTime(toTime(e.examStart));
        examEndEdit->setTime(toTime(e.examEnd));
        addBtn->setText("Save changes");
        cancelEditBtn->setVisible(true);
        statusLabel->setText("Editing " + e.code + ". Change the exam date, time or difficulty, then press Save changes.");
        code->setFocus();
    }

    void addCourse() {
        const QString c = code->text().trimmed().toUpper(), n = name->text().trimmed();
        if (c.isEmpty() || n.isEmpty()) {
            QMessageBox::warning(this, "Missing information", "Enter the course code and name.");
            return;
        }
        for (std::size_t i = 0; i < entries_.size(); ++i)
            if (entries_[i].code == c && int(i) != editing_) {
                QMessageBox::warning(this, "Duplicate course", "This course was already added.");
                return;
            }
        if (examDate->date() <= QDate::currentDate()) {
            QMessageBox::warning(this, "Exam date", "Choose an exam date after today.");
            return;
        }
        const int es = mins(examStartEdit), ee = mins(examEndEdit);
        if (ee <= es) {
            QMessageBox::warning(this, "Exam time", "The exam must end after it starts.");
            return;
        }
        Entry entry;
        entry.code = c;
        entry.name = n;
        entry.exam = examDate->date();
        entry.difficulty = difficulty->value();
        entry.examStart = es;
        entry.examEnd = ee;
        if (editing_ >= 0 && editing_ < int(entries_.size())) {
            entries_[static_cast<std::size_t>(editing_)] = entry;
            endEdit();
            statusLabel->setText("Course updated. Press Generate to rebuild the study plan.");
        } else {
            entries_.push_back(entry);
            code->clear();
            name->clear();
            code->setFocus();
        }
        invalidate();
    }

    void importCourses() {
        const StudyLinked l = link_ ? link_() : StudyLinked();
        int added = 0;
        for (const auto& c : l.courses) {
            bool have = false;
            for (const Entry& e : entries_) have = have || e.code == c.first;
            if (have) continue;
            Entry e;
            e.code = c.first;
            e.name = c.second;
            e.exam = QDate::currentDate().addDays(14);
            entries_.push_back(e);
            ++added;
        }
        if (l.courses.empty())
            QMessageBox::information(this, "Nothing to import", "Add courses in the Timetable page first.");
        else
            statusLabel->setText(QString("%1 course(s) imported with a placeholder exam date and time. Select each course and "
                                         "press Edit to set its real exam date, time and difficulty.").arg(added));
        invalidate();
    }

    void removeSelected() {
        const int row = table->currentRow();
        if (row < 0 || row >= int(entries_.size())) return;
        if (editing_ == row) endEdit();
        else if (editing_ > row) --editing_;
        entries_.erase(entries_.begin() + row);
        invalidate();
    }

    void invalidate() {
        hasPlan_ = false;
        gridLabel->clear();
        gridLabel->resize(0, 0);
        weekBox->blockSignals(true);
        weekBox->clear();
        weekBox->blockSignals(false);
        textView->setHtml(placeholder("Your study plan will appear here", "Add courses and press Generate."));
        hoursMetric->setText("-");
        examsMetric->setText("-");
        refreshTable();
    }

    void refreshTable() {
        table->setRowCount(0);
        QDate firstExam;
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const Entry& e = entries_[i];
            const int r = table->rowCount();
            table->insertRow(r);
            QString planned = "-";
            if (hasPlan_ && i < plan_.plannedMin.size()) planned = dur(plan_.plannedMin[i]);
            const QStringList vals = {e.code, e.name, e.exam.toString("ddd d MMM yyyy"),
                                      timeText(e.examStart) + " - " + timeText(e.examEnd),
                                      QString("%1 / 10").arg(e.difficulty),
                                      QString::number(QDate::currentDate().daysTo(e.exam)), planned};
            for (int c = 0; c < vals.size(); ++c) table->setItem(r, c, new QTableWidgetItem(vals[c]));
            if (!firstExam.isValid() || e.exam < firstExam) firstExam = e.exam;
        }
        coursesMetric->setText(QString::number(entries_.size()));
        nextMetric->setText(firstExam.isValid() ? QString("%1 d").arg(QDate::currentDate().daysTo(firstExam)) : "-");
    }

    StudyPrefs readPrefs() const {
        StudyPrefs p;
        p.freeDayStartMin = mins(freeStart);
        // After-midnight mode: the end time belongs to the next calendar day.
        p.latestEndMin = pastMidnight->isChecked() ? 1440 + mins(latestEnd) : mins(latestEnd);
        p.breakAfterUniMin = breakAfter->value();
        p.focusMin = focus->value();
        p.shortBreakMin = shortBreak->value();
        p.longBreakMin = shortBreak->value() * 2;
        p.dinnerStartMin = mins(dinnerStart);
        p.dinnerLenMin = dinnerLen->value();
        p.maxWeekdayStudyMin = weekdayMax->value();
        p.maxFreeDayStudyMin = freeDayMax->value();
        p.examBufferBeforeMin = examBefore->value();
        p.breakAfterExamMin = examAfter->value();
        p.restWeekday = restDay->currentIndex() - 1;
        p.useCampusGaps = gapsCheck->isChecked();
        return p;
    }

    // ---- planning ---------------------------------------------------------
    void generatePlan() {
        if (entries_.empty()) {
            QMessageBox::information(this, "No courses", "Add at least one course with an exam date first.");
            return;
        }
        linked_ = link_ ? link_() : StudyLinked();
        prefs_ = readPrefs();
        planCourses_.clear();
        const QDate today = QDate::currentDate();
        for (const Entry& e : entries_) {
            StudyCourse c;
            c.code = e.code.toStdString();
            c.name = e.name.toStdString();
            c.difficulty = e.difficulty;
            c.examJd = e.exam.toJulianDay();
            c.examStartMin = e.examStart;
            c.examEndMin = e.examEnd;
            planCourses_.push_back(c);
        }
        plan_ = StudyPlanner::plan(planCourses_, linked_.busy, prefs_, today.toJulianDay(), nowMinutes());
        if (!plan_.ok) {
            hasPlan_ = false;
            banner->setText(q(plan_.message));
            QMessageBox::warning(this, "Could not build a plan", q(plan_.message));
            return;
        }
        hasPlan_ = true;

        weekStarts_.clear();
        const long long first = plan_.startJd - (((plan_.startJd % 7) + 7) % 7);
        const long long last = plan_.startJd + static_cast<long long>(plan_.days.size()) - 1;
        for (long long w = first; w <= last; w += 7) weekStarts_.push_back(w);
        weekBox->blockSignals(true);
        weekBox->clear();
        for (long long w : weekStarts_)
            weekBox->addItem(QString("Week of %1 - %2").arg(dateOf(w).toString("d MMM"), dateOf(w + 6).toString("d MMM yyyy")));
        weekBox->blockSignals(false);

        int studyDays = 0, total = 0;
        for (const StudyDay& d : plan_.days) {
            if (!d.blocks.empty()) ++studyDays;
            total += d.studyMinutes();
        }
        hoursMetric->setText(QString("%1 h").arg(total / 60.0, 0, 'f', 1));
        examsMetric->setText(QString::number(studyDays));
        QString msg = q(plan_.message);
        if (!linked_.hasTimetable) msg += "  No timetable generated yet, so every day is treated as free of classes.";
        if (!plan_.warnings.empty()) msg += "  Notes: " + QString::number(plan_.warnings.size());
        banner->setText(msg);
        statusLabel->setText(QString("Linked: %1 class entries, %2 university event(s), %3 external event(s).")
                                 .arg(int(linked_.busy.size()) - linked_.uniEvents - linked_.externalEvents)
                                 .arg(linked_.uniEvents).arg(linked_.externalEvents));
        refreshTable();
        renderGrid();
        renderText();
    }

    // ---- grid -------------------------------------------------------------
    const StudyDay* dayFor(long long jd) const {
        const long long i = jd - plan_.startJd;
        return (i >= 0 && i < static_cast<long long>(plan_.days.size())) ? &plan_.days[static_cast<std::size_t>(i)] : nullptr;
    }

    void paintWeek(QPainter& p, const QRectF& area, long long mondayJd) const {
        // The grid follows the study start / end times; it only grows when a
        // class, event, exam or study block would otherwise be cut off.
        int start = (prefs_.freeDayStartMin / 60) * 60;
        int end = ((prefs_.latestEndMin + 59) / 60) * 60;
        for (int d = 0; d < 7; ++d) {
            const StudyDay* day = dayFor(mondayJd + d);
            if (!day) continue;
            for (const StudyBusy& b : day->busy) { start = std::min(start, (b.startMin / 60) * 60); end = std::max(end, ((b.endMin + 59) / 60) * 60); }
            for (const StudyBlock& b : day->blocks) { start = std::min(start, (b.startMin / 60) * 60); end = std::max(end, ((b.endMin + 59) / 60) * 60); }
            for (const StudyExam& x : day->exams) { start = std::min(start, (x.startMin / 60) * 60); end = std::max(end, ((x.endMin + 59) / 60) * 60); }
        }
        if (end - start < 120) end = start + 120;
        const int total = end - start;
        const double u = std::min(area.width() / 100.0, area.height() / 66.0);

        p.save();
        const int basePx = std::max(9, int(std::round(u * 1.3)));
        p.setFont(uiFont(basePx));
        const double frameW = std::max(1.0, u * 0.14);
        p.setPen(QPen(Qt::black, frameW));
        p.setBrush(Qt::white);
        p.drawRect(area.adjusted(frameW / 2, frameW / 2, -frameW / 2, -frameW / 2));

        const double pad = u * 1.2, timeW = u * 6, headH = u * 6;
        const QRectF grid(area.left() + pad + timeW, area.top() + pad + headH,
                          area.width() - 2 * pad - timeW, area.height() - 2 * pad - headH);
        const double colW = grid.width() / 7.0;
        auto Y = [&](int m) { return grid.top() + (m - start) * grid.height() / double(total); };

        for (int d = 0; d < 7; ++d) {
            const QRectF col(grid.left() + d * colW, grid.top(), colW, grid.height());
            const StudyDay* day = dayFor(mondayJd + d);
            if (!day) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#F3F3F3"));
                p.drawRect(col);
            } else if (day->restDay) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#EFEFEF"));
                p.drawRect(col);
                p.setPen(QColor("#737373"));
                p.setFont(uiFont(basePx));
                p.drawText(col, Qt::AlignCenter, "Rest day");
            }
            p.setPen(Qt::black);
            p.setFont(uiFont(std::max(8, int(u * 1.5)), true));
            p.drawText(QRectF(col.left(), area.top() + pad, colW, headH * 0.5), Qt::AlignCenter,
                       dayShort(d) + " " + dateOf(mondayJd + d).toString("d MMM"));
            if (day && !day->examCourses.empty()) {
                QStringList codes;
                for (int c : day->examCourses) codes << q(planCourses_[static_cast<std::size_t>(c)].code);
                p.setPen(QColor("#DC2626"));
                p.setFont(uiFont(std::max(8, int(u * 1.2)), true));
                p.drawText(QRectF(col.left(), area.top() + pad + headH * 0.5, colW, headH * 0.5), Qt::AlignCenter,
                           "EXAM " + codes.join(", "));
            }
        }

        p.setFont(uiFont(basePx));
        for (int t = ((start + 59) / 60) * 60; t <= end; t += 60) {
            const double y = Y(t);
            p.setPen(QPen(QColor("#D4D4D4"), 1));
            p.drawLine(QPointF(grid.left(), y), QPointF(grid.right(), y));
            p.setPen(t >= 1440 ? QColor("#DC2626") : QColor("#525252"));
            p.drawText(QRectF(area.left() + pad, y - u * 1.2, timeW - u * 0.6, u * 2.4), Qt::AlignRight | Qt::AlignVCenter, axisTime(t));
        }
        p.setPen(QPen(QColor("#D4D4D4"), 1));
        for (int d = 0; d <= 7; ++d)
            p.drawLine(QPointF(grid.left() + d * colW, grid.top()), QPointF(grid.left() + d * colW, grid.bottom()));
        if (end > 1440 && start < 1440) {   // midnight marker
            p.setPen(QPen(QColor("#DC2626"), std::max(1.0, u * 0.12), Qt::DashLine));
            p.drawLine(QPointF(grid.left(), Y(1440)), QPointF(grid.right(), Y(1440)));
        }
        p.setPen(QPen(Qt::black, std::max(1.5, u * 0.2)));
        p.drawLine(QPointF(grid.left(), grid.top()), QPointF(grid.right(), grid.top()));

        for (int d = 0; d < 7; ++d) {
            const StudyDay* day = dayFor(mondayJd + d);
            if (!day) continue;
            const double x = grid.left() + d * colW + u * 0.35, w = colW - u * 0.7;

            auto meal = [&](int s, int len, const QString& label) {
                if (s < start || s + len > end) return;
                const QRectF r(x, Y(s) + 1, w, Y(s + len) - Y(s) - 2);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#F5F5F5"));
                p.drawRect(r);
                drawFitted(p, r, label, QString(), timeText(s) + " - " + timeText(s + len), QColor("#737373"), basePx, u * 0.9, u * 0.4);
            };
            meal(prefs_.dinnerStartMin, prefs_.dinnerLenMin, "Dinner");
            if (day->uniEndMin < 0) meal(prefs_.lunchStartMin, prefs_.lunchLenMin, "Lunch");

            for (const StudyBusy& b : day->busy) {
                const QRectF r(x, Y(b.startMin) + 1, w, Y(b.endMin) - Y(b.startMin) - 2);
                QColor fill = QColor("#E5E5E5"), ink = QColor("#525252");
                if (b.kind == BusyKind::UniEvent) { fill = QColor("#DC2626"); ink = Qt::white; }
                if (b.kind == BusyKind::ExternalEvent) { fill = QColor("#991B1B"); ink = Qt::white; }
                p.setPen(Qt::NoPen);
                p.setBrush(fill);
                p.drawRect(r);
                drawFitted(p, r, q(b.label), b.kind == BusyKind::ExternalEvent ? "External" : QString(),
                           timeText(b.startMin) + " - " + timeText(b.endMin), ink, basePx, u * 0.9, u * 0.55);
            }
            for (const StudyBlock& b : day->blocks) {
                const QRectF r(x, Y(b.startMin) + 1, w, Y(b.endMin) - Y(b.startMin) - 2);
                const QColor fill = courseColor(b.course);
                const bool marked = b.kind == BlockKind::ExamEve || b.kind == BlockKind::ExamDay;
                p.setPen(marked ? QPen(QColor("#DC2626"), std::max(1.5, u * 0.2)) : Qt::NoPen);
                p.setBrush(fill);
                p.drawRect(r);
                drawFitted(p, r, q(planCourses_[static_cast<std::size_t>(b.course)].code), kindTag(b.kind),
                           timeText(b.startMin) + " - " + timeText(b.endMin),
                           fill.lightness() > 127 ? QColor("#000000") : QColor("#FFFFFF"), basePx, u * 0.9, u * 0.55);
            }
            for (const StudyExam& x2 : day->exams) {
                const StudyCourse& sc = planCourses_[static_cast<std::size_t>(x2.course)];
                const QRectF r(x, Y(x2.startMin) + 1, w, Y(x2.endMin) - Y(x2.startMin) - 2);
                p.setPen(QPen(QColor("#DC2626"), std::max(2.5, u * 0.3)));
                p.setBrush(QColor("#FEF2F2"));
                p.drawRect(r);
                drawFitted(p, r, "EXAM " + q(sc.code), q(sc.name),
                           timeText(x2.startMin) + " - " + timeText(x2.endMin),
                           QColor("#DC2626"), basePx, u * 0.9, u * 0.55);
            }
        }
        p.restore();
    }

    void paintLegend(QPainter& p, const QRectF& area) const {
        const double u = area.width() / 100.0;
        p.save();
        p.setFont(uiFont(std::max(8, int(u * 1.2))));
        double x = area.left(), y = area.top();
        const double rowH = u * 2.4;
        auto item = [&](const QColor& color, const QString& label, bool outline = false) {
            const double w = u * 3.2 + p.fontMetrics().horizontalAdvance(label) + u * 1.5;
            if (x + w > area.right()) { x = area.left(); y += rowH; }
            if (y + rowH > area.bottom() + rowH) return;
            p.setPen(QPen(outline ? QColor("#DC2626") : QColor("#000000"), outline ? 2 : 1));
            p.setBrush(color);
            p.drawRect(QRectF(x, y + u * 0.3, u * 1.8, u * 1.5));
            p.setPen(Qt::black);
            p.drawText(QRectF(x + u * 2.5, y, w, rowH), Qt::AlignLeft | Qt::AlignVCenter, label);
            x += w;
        };
        item(QColor("#E5E5E5"), "Classes");
        if (linked_.uniEvents > 0) item(QColor("#DC2626"), "University events");
        if (linked_.externalEvents > 0) item(QColor("#991B1B"), "External events");
        item(QColor("#F5F5F5"), "Meals");
        item(QColor("#FEF2F2"), "Exam", true);
        item(Qt::white, "Exam eve / exam day study", true);
        for (std::size_t i = 0; i < planCourses_.size(); ++i)
            item(courseColor(int(i)), q(planCourses_[i].code) + "  " + q(planCourses_[i].name));
        p.restore();
    }

    // Grid + legend in one rectangle.
    void paintWeekSheet(QPainter& p, const QRectF& area, long long mondayJd) const {
        const double legendH = std::max(40.0, area.height() * 0.085);
        paintWeek(p, QRectF(area.left(), area.top(), area.width(), area.height() - legendH), mondayJd);
        paintLegend(p, QRectF(area.left(), area.bottom() - legendH + 8, area.width(), legendH - 8));
    }

    QImage weekImage(long long mondayJd, int w, int h) const {
        QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        paintWeekSheet(p, QRectF(0, 0, w, h), mondayJd);
        p.end();
        return img;
    }

    void renderGrid() {
        if (!hasPlan_ || weekBox->currentIndex() < 0 || weekBox->currentIndex() >= int(weekStarts_.size())) return;
        const QImage img = weekImage(weekStarts_[static_cast<std::size_t>(weekBox->currentIndex())], 1500, 980);
        gridLabel->setPixmap(QPixmap::fromImage(img));
        gridLabel->resize(img.size());
    }

    // ---- text view --------------------------------------------------------
    void renderText() {
        QString h = "<div style='color:#000000'>";
        h += "<div style='font-weight:600'>" + q(plan_.message).toHtmlEscaped() + "</div>";
        for (const std::string& w : plan_.warnings)
            h += "<div style='color:#DC2626;margin-top:4px'><b>Note:</b> " + q(w).toHtmlEscaped() + "</div>";

        h += "<table width='100%' cellspacing='0' cellpadding='8' style='margin-top:8px'>"
             "<tr bgcolor='#000000'><th align='left' style='color:#FFFFFF'>Course</th>"
             "<th align='left' style='color:#FFFFFF'>Exam</th><th align='left' style='color:#FFFFFF'>Difficulty</th>"
             "<th align='left' style='color:#FFFFFF'>Target</th><th align='left' style='color:#FFFFFF'>Planned</th></tr>";
        for (std::size_t i = 0; i < entries_.size(); ++i)
            h += QString("<tr bgcolor='%1'><td><b>%2</b> %3</td><td>%4 %5</td><td>%6 / 10</td><td>%7</td><td>%8</td></tr>")
                     .arg(i % 2 ? "#F5F5F5" : "#FFFFFF", entries_[i].code.toHtmlEscaped(), entries_[i].name.toHtmlEscaped(),
                          entries_[i].exam.toString("ddd d MMM"),
                          timeText(entries_[i].examStart) + " - " + timeText(entries_[i].examEnd),
                          QString::number(entries_[i].difficulty),
                          dur(plan_.neededMin[i]), dur(plan_.plannedMin[i]));
        h += "</table>";

        for (const StudyDay& d : plan_.days) {
            const QDate date = dateOf(d.jd);
            h += "<div style='background-color:#F5F5F5;padding:12px;margin-top:8px'>";
            h += "<b style='font-size:15px'>" + date.toString("dddd d MMMM yyyy") + "</b>";
            if (d.studyMinutes() > 0) h += " <span style='color:#525252'>- " + dur(d.studyMinutes()) + " of study</span>";
            h += "<br>";
            for (const StudyExam& ex : d.exams)
                h += "<div style='color:#DC2626;margin-top:6px'><b>EXAM " + timeText(ex.startMin) + " - " + timeText(ex.endMin) + ": " +
                     q(planCourses_[static_cast<std::size_t>(ex.course)].code).toHtmlEscaped() + " " +
                     q(planCourses_[static_cast<std::size_t>(ex.course)].name).toHtmlEscaped() + "</b></div>";
            if (!d.exams.empty())
                h += "<div style='color:#525252'>No classes today. Study before the exam and again after it.</div>";
            if (d.restDay) { h += "<div style='color:#525252;margin-top:6px'>Rest day</div></div>"; continue; }

            for (const StudyBusy& b : d.busy) {
                h += "<div style='color:#525252;margin-top:4px'>" + timeText(b.startMin) + " - " + timeText(b.endMin) +
                     " &nbsp; " + busyName(b.kind) + ": " + q(b.label).toHtmlEscaped() + "</div>";
            }
            if (d.uniEndMin >= 0)
                h += "<div style='margin-top:4px'><b>University ends " + timeText(d.uniEndMin) + "</b> - "
                     + dur(prefs_.breakAfterUniMin) + " rest, study from "
                     + (d.studyFromMin >= 0 ? timeText(std::max(d.studyFromMin, d.uniEndMin + prefs_.breakAfterUniMin)) : QString("-"))
                     + ".</div>";
            if (d.blocks.empty()) h += "<div style='color:#525252;margin-top:6px'>No study planned.</div>";
            for (const StudyBlock& b : d.blocks)
                h += "<div style='margin-top:6px'><span style='background-color:" + courseColor(b.course).name() +
                     "'>&nbsp;&nbsp;&nbsp;</span> <b>" + timeText(b.startMin) + " - " + timeText(b.endMin) + "</b> &nbsp; <b>" +
                     q(planCourses_[static_cast<std::size_t>(b.course)].code).toHtmlEscaped() + "</b> &nbsp; <span style='color:#525252'>" +
                     q(planCourses_[static_cast<std::size_t>(b.course)].name).toHtmlEscaped() + " (" + kindTag(b.kind) +
                     (b.onCampus ? ", between classes" : "") + ")</span></div>";
            h += "</div>";
        }
        h += "</div>";
        textView->setHtml(h);
    }

    // ---- export -----------------------------------------------------------
    QString askPath(const QString& n, const QString& filter, const QString& suffix) {
        QString path = QFileDialog::getSaveFileName(this, "Export", QDir::homePath() + "/" + n, filter);
        if (path.isEmpty()) return QString();
        if (!path.endsWith("." + suffix, Qt::CaseInsensitive)) path += "." + suffix;
        return path;
    }

    static QString csvCell(QString s) {
        s.replace("\"", "\"\"");
        return "\"" + s + "\"";
    }

    // Every class, event, study block and exam in date order (used by PDF details).
    std::vector<DetailRow> detailRows() const {
        std::vector<DetailRow> rows;
        for (const StudyDay& d : plan_.days) {
            const QString date = dateOf(d.jd).toString("d MMM yyyy"), dn = dayShort(d.weekday);
            for (const StudyBusy& b : d.busy) {
                DetailRow r;
                r.jd = d.jd; r.start = b.startMin; r.date = date; r.day = dn;
                r.time = timeText(b.startMin) + " - " + timeText(b.endMin);
                r.type = busyName(b.kind); r.title = q(b.label);
                r.accent = b.kind != BusyKind::Class;
                rows.push_back(r);
            }
            for (const StudyBlock& b : d.blocks) {
                const StudyCourse& c = planCourses_[static_cast<std::size_t>(b.course)];
                DetailRow r;
                r.jd = d.jd; r.start = b.startMin; r.date = date; r.day = dn;
                r.time = timeText(b.startMin) + " - " + timeText(b.endMin);
                r.type = kindTag(b.kind); r.title = q(c.code) + "  " + q(c.name);
                r.note = b.onCampus ? "Between classes" : QString();
                rows.push_back(r);
            }
            for (const StudyExam& ex : d.exams) {
                const StudyCourse& sc = planCourses_[static_cast<std::size_t>(ex.course)];
                DetailRow r;
                r.jd = d.jd; r.start = ex.startMin; r.date = date; r.day = dn;
                r.time = timeText(ex.startMin) + " - " + timeText(ex.endMin);
                r.type = "Exam"; r.title = q(sc.code) + "  " + q(sc.name);
                r.note = "No classes; study before and after"; r.accent = true;
                rows.push_back(r);
            }
        }
        std::stable_sort(rows.begin(), rows.end(), [](const DetailRow& a, const DetailRow& b) {
            return a.jd != b.jd ? a.jd < b.jd : a.start < b.start;
        });
        return rows;
    }

    void drawDetails(QPainter& p, QPdfWriter& w, const QRectF& page) const {
        const std::vector<DetailRow> rows = detailRows();
        const double u = page.width() / 100.0;
        const double rowH = u * 2.7, pad = u * 0.7;
        const QStringList heads = {"Date", "Day", "Time", "Type", "Course / item", "Note"};
        const double cut[7] = {0.0, 0.13, 0.20, 0.38, 0.54, 0.86, 1.0};
        const QFont reg = uiFont(std::max(8, int(u * 1.2))), bold = uiFont(std::max(8, int(u * 1.2)), true);

        auto cell = [&](double y, int c, const QString& text) {
            const QRectF r(page.left() + page.width() * cut[c] + pad, y, page.width() * (cut[c + 1] - cut[c]) - 2 * pad, rowH);
            p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(text, Qt::ElideRight, int(r.width())));
        };
        auto header = [&](double y) {
            p.setPen(Qt::NoPen);
            p.setBrush(Qt::black);
            p.drawRect(QRectF(page.left(), y, page.width(), rowH));
            p.setFont(bold);
            p.setPen(Qt::white);
            for (int c = 0; c < 6; ++c) cell(y, c, heads[c]);
        };

        p.setFont(uiFont(int(u * 2.4), true));
        p.setPen(Qt::black);
        p.drawText(QRectF(page.left(), page.top(), page.width(), u * 3.4), Qt::AlignLeft | Qt::AlignVCenter,
                   "Schedule details - classes, events, study blocks and exams");
        double y = page.top() + u * 4.4;
        header(y);
        y += rowH;
        int n = 0;
        for (const DetailRow& r : rows) {
            if (y + rowH > page.bottom()) {
                w.newPage();
                y = page.top();
                header(y);
                y += rowH;
                n = 0;
            }
            p.setPen(Qt::NoPen);
            p.setBrush(n % 2 ? QColor("#F5F5F5") : QColor("#FFFFFF"));
            p.drawRect(QRectF(page.left(), y, page.width(), rowH));
            p.setPen(QPen(QColor("#D4D4D4"), 1));
            p.drawLine(QPointF(page.left(), y + rowH), QPointF(page.right(), y + rowH));
            p.setFont(reg);
            p.setPen(r.accent ? QColor("#DC2626") : QColor("#000000"));
            cell(y, 0, r.date); cell(y, 1, r.day); cell(y, 2, r.time);
            cell(y, 3, r.type); cell(y, 4, r.title); cell(y, 5, r.note);
            y += rowH;
            ++n;
        }
        p.setPen(QPen(Qt::black, 2));
        p.drawLine(QPointF(page.left(), y), QPointF(page.right(), y));
    }

    void exportAs(const QString& kind) {
        if (!hasPlan_) {
            QMessageBox::information(this, "Nothing to export", "Generate a study plan first.");
            return;
        }
        bool ok = false;
        QString path;
        if (kind == "pdf") {
            path = askPath("study-plan.pdf", "PDF document (*.pdf)", "pdf");
            if (path.isEmpty()) return;
            QPdfWriter w(path);
            w.setPageSize(QPageSize(QPageSize::A4));
            w.setPageOrientation(QPageLayout::Landscape);
            w.setResolution(150);
            w.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);
            w.setTitle("Study Plan");
            w.setCreator("Smart Scheduler Suite");
            QPainter p(&w);
            ok = p.isActive();
            if (ok) {
                p.setRenderHint(QPainter::Antialiasing);
                p.setRenderHint(QPainter::TextAntialiasing);
                const QRectF page(0, 0, w.width(), w.height());
                for (std::size_t i = 0; i < weekStarts_.size(); ++i) {
                    if (i) w.newPage();
                    p.setFont(uiFont(30, true));
                    p.setPen(Qt::black);
                    p.drawText(QRectF(page.left(), page.top(), page.width(), 44), Qt::AlignLeft | Qt::AlignVCenter,
                               "Study Plan - week of " + dateOf(weekStarts_[i]).toString("d MMM yyyy"));
                    paintWeekSheet(p, QRectF(page.left(), page.top() + 54, page.width(), page.height() - 54), weekStarts_[i]);
                }
                w.newPage();
                drawDetails(p, w, page);
                p.end();
            }
        } else if (kind == "jpg") {
            path = askPath("study-plan.jpg", "JPEG image (*.jpg)", "jpg");
            if (path.isEmpty()) return;
            const int idx = std::max(0, weekBox->currentIndex());
            ok = weekImage(weekStarts_[static_cast<std::size_t>(idx)], 3300, 2200).convertToFormat(QImage::Format_RGB32).save(path, "JPG", 95);
        } else {
            path = askPath("study-plan.csv", "CSV spreadsheet (*.csv)", "csv");
            if (path.isEmpty()) return;
            QString out = "Date,Day,Start,End,Duration (min),Type,Course,Name,Note\r\n";
            for (const StudyDay& d : plan_.days) {
                const QString date = dateOf(d.jd).toString("yyyy-MM-dd"), dn = dayShort(d.weekday);
                for (const StudyBusy& b : d.busy)
                    out += csvCell(date) + "," + csvCell(dn) + "," + csvCell(timeText(b.startMin)) + "," + csvCell(timeText(b.endMin)) + "," +
                           QString::number(b.endMin - b.startMin) + "," + csvCell(busyName(b.kind)) +
                           ",," + csvCell(q(b.label)) + ",\r\n";
                for (const StudyBlock& b : d.blocks) {
                    const StudyCourse& c = planCourses_[static_cast<std::size_t>(b.course)];
                    out += csvCell(date) + "," + csvCell(dn) + "," + csvCell(timeText(b.startMin)) + "," + csvCell(timeText(b.endMin)) + "," +
                           QString::number(b.endMin - b.startMin) + "," + csvCell(kindTag(b.kind)) + "," + csvCell(q(c.code)) + "," +
                           csvCell(q(c.name)) + ",\r\n";
                }
                for (const StudyExam& ex : d.exams) {
                    const StudyCourse& sc = planCourses_[static_cast<std::size_t>(ex.course)];
                    out += csvCell(date) + "," + csvCell(dn) + "," + csvCell(timeText(ex.startMin)) + "," + csvCell(timeText(ex.endMin)) + "," +
                           QString::number(ex.endMin - ex.startMin) + "," + csvCell("Exam") + "," +
                           csvCell(q(sc.code)) + "," + csvCell(q(sc.name)) + ",\r\n";
                }
            }
            QFile f(path);
            ok = f.open(QIODevice::WriteOnly | QIODevice::Truncate);
            if (ok) {
                f.write(QString(QChar(0xFEFF)).toUtf8());
                f.write(out.toUtf8());
            }
        }
        if (ok) statusLabel->setText("Exported to " + path);
        else QMessageBox::warning(this, "Export failed", "The file could not be written. Check the location and try again.");
    }
};