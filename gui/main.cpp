#include <QApplication>
#include <QIcon>
#include <QSplashScreen>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>
#include <QMainWindow>
#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QButtonGroup>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTimeEdit>
#include <QTime>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QTabBar>
#include <QAbstractItemView>
#include <QTextBrowser>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QTabWidget>
#include <QGroupBox>
#include <QSplitter>
#include <QScrollArea>
#include <QStatusBar>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QRegularExpression>
#include <QStringList>
#include <QRandomGenerator>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QImage>
#include <QPixmap>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "engine/AutoScheduler.h"
#include "gui/TournamentPage.h"
#include "gui/StudyPlannerPage.h"

namespace {

// ---- Palette -------------------------------------------------------------
const QColor INK("#000000");
const QColor MUTED("#525252");
const QColor ACCENT("#DC2626");
const QColor HAIRLINE("#D4D4D4");
const QColor PAPER("#FFFFFF");
const QColor WASH("#F5F5F5");

// Julian day numbers: jd % 7 == 0 is a Monday.
long long mondayOfJd(long long jd) { return jd - (((jd % 7) + 7) % 7); }

// Qt style sheets can only draw spin-box / combo-box arrows and check marks from
// image files, so small chevron icons are generated once at start-up.
QString makeIcons() {
    const QString dir = QDir::tempPath() + "/smart_scheduler_icons";
    QDir().mkpath(dir);
    auto chevron = [&](const QString& file, bool up, const QColor& color) {
        QImage img(48, 48, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 5.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        if (up) { path.moveTo(13, 30); path.lineTo(24, 18); path.lineTo(35, 30); }
        else    { path.moveTo(13, 18); path.lineTo(24, 30); path.lineTo(35, 18); }
        p.drawPath(path);
        p.end();
        img.save(dir + "/" + file);
    };
    chevron("up.png", true, QColor("#000000"));
    chevron("down.png", false, QColor("#000000"));
    chevron("up_off.png", true, QColor("#BDBDBD"));
    chevron("down_off.png", false, QColor("#BDBDBD"));

    QImage tick(48, 48, QImage::Format_ARGB32_Premultiplied);
    tick.fill(Qt::transparent);
    {
        QPainter p(&tick);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#FFFFFF"), 6.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath path;
        path.moveTo(11, 25); path.lineTo(20, 34); path.lineTo(37, 14);
        p.drawPath(path);
    }
    tick.save(dir + "/check.png");
    return QDir::fromNativeSeparators(dir);
}

const QString STYLE_TEMPLATE = R"QSS(
* { font-family: "Helvetica Neue", Helvetica, Arial, sans-serif; }
QMainWindow, QWidget#root { background: #FFFFFF; }
QWidget { color: #000000; font-size: 13px; }
QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }

QFrame#sidebar { background: #000000; border: none; }
QFrame#sidebar QLabel { color: #FFFFFF; }
QFrame#sidebar QLabel#muted, QFrame#sidebar QLabel#eyebrow { color: #A3A3A3; }
QFrame#topbar { background: transparent; border: none; border-bottom: 2px solid #000000; border-radius: 0; }
QFrame#panel { background: #F5F5F5; border: none; border-radius: 0; }
QGroupBox { background: #F5F5F5; border: none; border-top: 2px solid #000000; border-radius: 0; margin-top: 4px; padding: 44px 20px 20px 20px; font-weight: 600; }
QGroupBox::title { subcontrol-origin: padding; subcontrol-position: top left; left: 20px; top: 12px; color: #000000; }

QLabel { background: transparent; }
QLabel#muted { color: #525252; }
QLabel#eyebrow { color: #525252; font-size: 12px; font-weight: 500; }
QLabel#pageTitle { font-size: 28px; font-weight: 600; color: #000000; }
QLabel#metric { font-size: 28px; font-weight: 600; color: #000000; }

QLineEdit, QAbstractSpinBox, QComboBox { background: #FFFFFF; color: #000000; border: 1px solid #000000; border-radius: 0; padding: 8px 12px; min-height: 24px; selection-background-color: #000000; selection-color: #FFFFFF; }
QAbstractSpinBox { padding: 8px 36px 8px 12px; }
QComboBox { padding-right: 40px; }
QLineEdit:focus, QAbstractSpinBox:focus, QComboBox:focus { border: 2px solid #DC2626; }
QLineEdit:disabled, QAbstractSpinBox:disabled, QComboBox:disabled { background: #F5F5F5; color: #666666; border: 1px solid #BFBFBF; }

QAbstractSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 28px; border: none; border-left: 1px solid #000000; background: #FFFFFF; }
QAbstractSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 28px; border: none; border-left: 1px solid #000000; border-top: 1px solid #D4D4D4; background: #FFFFFF; }
QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover { background: #EDEDED; }
QAbstractSpinBox::up-button:pressed, QAbstractSpinBox::down-button:pressed { background: #D4D4D4; }
QAbstractSpinBox::up-arrow { image: url(@ICONS@/up.png); width: 14px; height: 14px; }
QAbstractSpinBox::down-arrow { image: url(@ICONS@/down.png); width: 14px; height: 14px; }
QAbstractSpinBox::up-arrow:disabled, QAbstractSpinBox::up-arrow:off { image: url(@ICONS@/up_off.png); }
QAbstractSpinBox::down-arrow:disabled, QAbstractSpinBox::down-arrow:off { image: url(@ICONS@/down_off.png); }

QComboBox::drop-down, QDateEdit::drop-down { subcontrol-origin: border; subcontrol-position: top right; width: 32px; border: none; border-left: 1px solid #000000; background: #FFFFFF; }
QComboBox::drop-down:hover, QDateEdit::drop-down:hover { background: #EDEDED; }
QComboBox::down-arrow, QDateEdit::down-arrow { image: url(@ICONS@/down.png); width: 14px; height: 14px; }
QComboBox::down-arrow:disabled { image: url(@ICONS@/down_off.png); }
QComboBox QAbstractItemView { background: #FFFFFF; color: #000000; selection-background-color: #000000; selection-color: #FFFFFF; border: 1px solid #000000; outline: 0; padding: 4px; }

QCalendarWidget { background: #FFFFFF; color: #000000; border: 1px solid #000000; }
QCalendarWidget QWidget#qt_calendar_navigationbar { background: #000000; }
QCalendarWidget QToolButton { color: #FFFFFF; background: transparent; border: none; padding: 4px 8px; font-weight: 600; }
QCalendarWidget QToolButton:hover { background: #DC2626; }
QCalendarWidget QSpinBox { color: #FFFFFF; background: #000000; border: none; padding: 2px 6px; min-height: 18px; selection-background-color: #DC2626; selection-color: #FFFFFF; }
QCalendarWidget QSpinBox::up-button, QCalendarWidget QSpinBox::down-button { width: 0px; border: none; }
QCalendarWidget QMenu { background: #FFFFFF; color: #000000; }
QCalendarWidget QAbstractItemView { background: #FFFFFF; color: #000000; selection-background-color: #000000; selection-color: #FFFFFF; outline: 0; }
QCalendarWidget QAbstractItemView:disabled { color: #A3A3A3; }

QPushButton { background: #FFFFFF; border: 1px solid #000000; border-radius: 0; padding: 8px 18px; min-height: 24px; font-weight: 500; color: #000000; }
QPushButton:hover { background: #000000; color: #FFFFFF; }
QPushButton:focus { border: 2px solid #DC2626; }
QPushButton#primary { background: #000000; color: #FFFFFF; border: 1px solid #000000; }
QPushButton#primary:hover { background: #DC2626; border: 1px solid #DC2626; }
QPushButton#quiet { background: transparent; border: 1px solid transparent; }
QPushButton#quiet:hover { background: #FFFFFF; color: #000000; border: 1px solid #000000; }
QPushButton#nav { text-align: left; padding: 12px 16px; background: transparent; border: none; border-left: 4px solid transparent; border-radius: 0; color: #A3A3A3; }
QPushButton#nav:hover { background: transparent; color: #FFFFFF; border-left: 4px solid #525252; }
QPushButton#nav:checked { background: transparent; color: #FFFFFF; border-left: 4px solid #DC2626; }
QToolButton#exportBtn { background: #FFFFFF; color: #000000; border: 1px solid #000000; border-radius: 0; padding: 8px 18px; min-height: 24px; font-weight: 500; }
QToolButton#exportBtn:hover { background: #000000; color: #FFFFFF; }
QToolButton#exportBtn:focus { border: 2px solid #DC2626; }
QToolButton#exportBtn::menu-indicator { image: none; width: 0px; }

QMenu { background: #FFFFFF; color: #000000; border: 1px solid #000000; border-radius: 0; padding: 4px; }
QMenu::item { padding: 8px 24px; color: #000000; }
QMenu::item:selected { background: #000000; color: #FFFFFF; }
QMenu::separator { height: 1px; background: #D4D4D4; margin: 4px 0; }

QTabWidget::pane { border: 0; }
QTabBar::tab { background: transparent; color: #525252; padding: 8px 16px; margin-right: 8px; border: none; border-bottom: 2px solid transparent; font-weight: 500; }
QTabBar::tab:hover { color: #000000; }
QTabBar::tab:selected { color: #000000; font-weight: 600; border-bottom: 2px solid #000000; }

QTableWidget { background: #FFFFFF; alternate-background-color: #F5F5F5; color: #000000; border: 1px solid #BFBFBF; border-radius: 0; gridline-color: #D4D4D4; selection-background-color: #000000; selection-color: #FFFFFF; }
QHeaderView::section { background: #FFFFFF; color: #000000; padding: 10px 8px; border: none; border-bottom: 2px solid #000000; font-weight: 600; }
QTableWidget::item { padding: 8px 10px; }
QTextBrowser { background: #FFFFFF; color: #000000; border: 1px solid #BFBFBF; border-radius: 0; padding: 14px; }
QCheckBox { spacing: 10px; color: #000000; }
QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #000000; background: #FFFFFF; }
QCheckBox::indicator:checked { background: #000000; image: url(@ICONS@/check.png); }
QCheckBox::indicator:disabled { border: 1px solid #BFBFBF; background: #F5F5F5; }
QTableWidget:focus { border: 2px solid #DC2626; }
QTextBrowser:focus { border: 2px solid #DC2626; }

QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; border: none; }
QScrollBar::handle:vertical { background: #C4C4C4; border-radius: 4px; min-height: 40px; }
QScrollBar::handle:vertical:hover { background: #8C8C8C; }
QScrollBar::handle:vertical:pressed { background: #000000; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; width: 0; background: none; border: none; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 2px; border: none; }
QScrollBar::handle:horizontal { background: #C4C4C4; border-radius: 4px; min-width: 40px; }
QScrollBar::handle:horizontal:hover { background: #8C8C8C; }
QScrollBar::handle:horizontal:pressed { background: #000000; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; height: 0; background: none; border: none; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }
QAbstractScrollArea::corner { background: transparent; border: none; }

QSplitter::handle { background: transparent; }
QToolTip { background: #000000; color: #FFFFFF; border: none; padding: 4px 8px; }
QDialog, QMessageBox { background: #FFFFFF; color: #000000; }
QDialog QLabel, QMessageBox QLabel { color: #000000; background: transparent; }
QMessageBox QPushButton { min-width: 72px; background: #FFFFFF; color: #000000; border: 1px solid #000000; }
QMessageBox QPushButton:hover { background: #000000; color: #FFFFFF; }
QPlainTextEdit { background: #FFFFFF; color: #000000; border: 1px solid #000000; border-radius: 0; padding: 8px 12px; selection-background-color: #000000; selection-color: #FFFFFF; placeholder-text-color: #737373; }
QPlainTextEdit:focus { border: 2px solid #DC2626; }
QPlainTextEdit:disabled { background: #F5F5F5; color: #666666; }
)QSS";

QString buildStyle() {
    QString s = STYLE_TEMPLATE;
    s.replace("@ICONS@", makeIcons());
    return s;
}

QString qstr(const std::string& s) { return QString::fromStdString(s); }

QFont baseFont() {
    QFont f;
    f.setFamilies({"Helvetica Neue", "Helvetica", "Arial"});
    return f;
}

QString timeText(int mins) {
    return QString("%1:%2").arg(mins / 60, 2, 10, QChar('0')).arg(mins % 60, 2, 10, QChar('0'));
}
QString dayName(int d) {
    static const QStringList days = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
    return d >= 0 && d < days.size() ? days[d] : "Unknown";
}
QString shortDay(int d) {
    static const QStringList days = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    return d >= 0 && d < days.size() ? days[d] : "?";
}
QString durationText(int minutes) {
    if (minutes < 60) return QString::number(minutes) + " min";
    const int h = minutes / 60, m = minutes % 60;
    return m ? QString("%1 h %2 min").arg(h).arg(m) : QString("%1 h").arg(h);
}
QString repeatText(int r) {
    switch (r) {
        case 1: return "Daily";
        case 2: return "Weekly";
        case 3: return "Monthly";
        default: return "This week only";
    }
}

QLabel* muted(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("muted");
    l->setWordWrap(true);
    return l;
}

QFrame* panel() {
    auto* f = new QFrame;
    f->setObjectName("panel");
    return f;
}

QLabel* heading(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet("font-size:16px;font-weight:600;color:#000000;");
    return l;
}

void setupTable(QTableWidget* table, const QStringList& headers) {
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(40);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    table->setMinimumHeight(190);
}

QColor courseColor(int i) {
    static const QList<QColor> base = {
        QColor("#000000"), QColor("#595959"), QColor("#A3A3A3"), QColor("#262626"),
        QColor("#737373"), QColor("#CFCFCF"), QColor("#404040"), QColor("#8C8C8C")};
    return base[i % base.size()];
}
QColor textOn(const QColor& c) { return c.lightness() > 127 ? QColor("#000000") : QColor("#FFFFFF"); }

QString csvCell(QString s) {
    s.replace("\"", "\"\"");
    return "\"" + s + "\"";
}

bool parseEventText(const QString& chunk, QString& name, int& minutes) {
    QString s = chunk.trimmed();
    if (s.isEmpty()) return false;
    static const QRegularExpression hourRx(R"((\d+(?:\.\d+)?)\s*(?:hours?|hrs?|h)(?![a-z]))",
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression minRx(R"((\d+)\s*(?:minutes?|mins?|m)(?![a-z]))",
                                          QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression bareRx(R"((\d+)\s*$)");

    int total = 0;
    bool found = false;

    auto m = hourRx.match(s);
    if (m.hasMatch()) {
        total += static_cast<int>(m.captured(1).toDouble() * 60.0 + 0.5);
        found = true;
        s.remove(m.capturedStart(0), m.capturedLength(0));
    }
    auto m2 = minRx.match(s);
    bool minutesFound = false;
    if (m2.hasMatch()) {
        total += m2.captured(1).toInt();
        found = true;
        minutesFound = true;
        s.remove(m2.capturedStart(0), m2.capturedLength(0));
    }
    if (!minutesFound) {
        auto m3 = bareRx.match(s);
        if (m3.hasMatch()) {
            total += m3.captured(1).toInt();
            found = true;
            s.remove(m3.capturedStart(0), m3.capturedLength(0));
        }
    }
    s = s.trimmed();
    while (!s.isEmpty() && QString(" -:@,()").contains(s.back())) s.chop(1);
    while (!s.isEmpty() && QString(" -:@,()").contains(s.front())) s.remove(0, 1);

    if (!found || total <= 0 || s.isEmpty()) return false;
    name = s;
    minutes = total;
    return true;
}

} // namespace

void drawFitted(QPainter& p, const QRectF& r, const QString& title, const QString& sub,
                const QString& time, const QColor& color, int basePx, double padX, double padY) {
    const QRectF box = r.adjusted(padX, padY, -padX, -padY);
    if (box.width() < 10 || box.height() < 7) return;

    p.save();
    p.setClipRect(r);
    p.setPen(color);
    QFont reg = p.font();
    QFont bold = reg;
    bold.setBold(true);
    const bool hasSub = !sub.isEmpty();
    const int minReadable = std::max(8, int(basePx * 0.72));
    const int firstTier = hasSub ? 3 : 2;

    auto drawLine = [&](double y, const QFont& font, const QString& text, double lineHeight) {
        p.setFont(font);
        const QString fitted = QFontMetrics(font).elidedText(text, Qt::ElideRight, int(box.width()));
        p.drawText(QRectF(box.left(), y, box.width(), lineHeight),
                   Qt::AlignLeft | Qt::AlignVCenter, fitted);
    };

    for (int tier = firstTier; tier >= 1; --tier) {
        const int floorPx = (tier == 1) ? 7 : minReadable;
        for (int px = basePx; px >= floorPx; --px) {
            reg.setPixelSize(px);
            bold.setPixelSize(px);
            const double lineHeight = std::max(QFontMetricsF(reg).lineSpacing(),
                                               QFontMetricsF(bold).lineSpacing());
            if (lineHeight * tier > box.height() + 0.5) continue;

            const double y = box.top();
            if (tier == 3) {
                drawLine(y, bold, title, lineHeight);
                drawLine(y + lineHeight, reg, sub, lineHeight);
                drawLine(y + 2 * lineHeight, reg, time, lineHeight);
            } else if (tier == 2) {
                if (hasSub) {
                    p.setFont(bold);
                    const int codeWidth = QFontMetrics(bold).horizontalAdvance(title) + 8;
                    if (codeWidth < box.width() - 20) {
                        p.drawText(QRectF(box.left(), y, codeWidth, lineHeight),
                                   Qt::AlignLeft | Qt::AlignVCenter, title);
                        p.setFont(reg);
                        const QString fittedName = QFontMetrics(reg).elidedText(
                            sub, Qt::ElideRight, int(box.width() - codeWidth));
                        p.drawText(QRectF(box.left() + codeWidth, y,
                                          box.width() - codeWidth, lineHeight),
                                   Qt::AlignLeft | Qt::AlignVCenter, fittedName);
                    } else {
                        drawLine(y, bold, title, lineHeight);
                    }
                } else {
                    drawLine(y, bold, title, lineHeight);
                }
                drawLine(y + lineHeight, reg, time, lineHeight);
            } else {
                drawLine(y, bold, title + "  " + time, lineHeight);
            }
            p.restore();
            return;
        }
    }
    p.restore();
}

class MainWindow final : public QMainWindow {
    std::vector<AutoCourse> courses;
    std::vector<AutoEvent> universityEvents;   // event definitions (with repeat info)
    std::vector<AutoEvent> externalEvents;
    // What actually happens in the viewed week (repeats expanded). The placement
    // results below are indexed like these vectors, not like the definitions.
    std::vector<AutoEvent> uniInst, extInst;
    std::vector<int> uniSrc, extSrc;           // instance -> definition index
    AutoResult timetableResult;
    AutoEventResult universityEventResult;
    AutoEventResult externalEventResult;
    bool externalMode = false;
    AutoPrefs lastPrefs;
    bool hasTimetable = false;
    int nextCourseId = 1;
    int editingCourseId = -1;      // course being edited, -1 = adding
    int editingEventIndex = -1;    // event being edited in the active list, -1 = adding
    long long viewMonday = mondayOfJd(QDate::currentDate().toJulianDay());   // week shown in the event planner

    QTabWidget* tabs = nullptr;
    QBoxLayout* heroLayout = nullptr;
    QFrame* homeVisual = nullptr;
    QSplitter* timetableSplit = nullptr;
    QSplitter* eventSplit = nullptr;
    bool compactLayout = false;
    QButtonGroup* navGroup = nullptr;
    QPushButton* navBtns[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

    QTableWidget* courseTable = nullptr;
    QTableWidget* eventTable = nullptr;
    QTextBrowser* timetableOutput = nullptr;
    QTextBrowser* eventOutput = nullptr;
    QLabel* gridLabel = nullptr;
    QLabel* eventGridLabel = nullptr;
    QLabel* courseCount = nullptr;
    QLabel* sessionCount = nullptr;
    QLabel* placedCount = nullptr;
    QLabel* slotCountLabel = nullptr;
    QLabel* eventCount = nullptr;
    QLabel* eventPlaced = nullptr;
    QLabel* eventOverlap = nullptr;
    QLabel* statusLabel = nullptr;
    QLabel* heroTitle = nullptr;
    QLabel* slotInfo = nullptr;
    QLabel* topTitle = nullptr;
    QLabel* weekLabel = nullptr;

    QLineEdit *courseCode = nullptr, *courseName = nullptr;
    QSpinBox *slotsPerSession = nullptr, *sessionsPerWeek = nullptr;
    QSpinBox *slotMinutes = nullptr, *slotBreak = nullptr, *longMin = nullptr, *longMax = nullptr;
    QTimeEdit *dayStart = nullptr, *dayEnd = nullptr;
    QComboBox* reserveDay = nullptr;
    QCheckBox* longBreakCheck = nullptr;
    QPushButton *addCourseBtn = nullptr, *cancelCourseEditBtn = nullptr;

    QLineEdit *eventName = nullptr, *eventQuick = nullptr;
    QSpinBox* eventMinutes = nullptr;
    QComboBox* eventDay = nullptr;
    QComboBox* eventPlacementBox = nullptr;
    QComboBox* eventRepeat = nullptr;
    QTimeEdit *eventCustomStart = nullptr, *eventCustomEnd = nullptr;
    QLabel *eventLengthLabel = nullptr, *eventDayLabel = nullptr;
    QLabel *eventCustomStartLabel = nullptr, *eventCustomEndLabel = nullptr;
    QComboBox* eventModeBox = nullptr;
    QPushButton *addEventBtn = nullptr, *cancelEventEditBtn = nullptr;
    QTimeEdit *externalDayStart = nullptr, *externalDayEnd = nullptr;
    QSpinBox *externalSlotMinutes = nullptr, *externalBreakMinutes = nullptr;
    QCheckBox* externalDayChecks[7]{};
    QWidget* externalSettings = nullptr;

public:
    MainWindow() {
        setWindowTitle("Smart Scheduler Suite");
        resize(1440, 920);
        setMinimumSize(980, 640);
        setStyleSheet(buildStyle());
        buildUi();
        polishLayouts();
        updateSlotInfo();
        updateWeekLabel();
        refreshCourseTable();
        refreshEventTable();
        renderGrids();
    }

private:
    // Applies the spacing rules to every module (including Tournament and Study)
    // once the widgets exist. Group boxes and wrapped labels may never shrink
    // below their natural height, which is what made the forms look squashed.
    void polishLayouts() {
        for (QGroupBox* g : findChildren<QGroupBox*>()) {
            g->setSizePolicy(g->sizePolicy().horizontalPolicy(), QSizePolicy::Minimum);
            if (auto* grid = qobject_cast<QGridLayout*>(g->layout())) {
                grid->setHorizontalSpacing(16);
                grid->setVerticalSpacing(10);
            } else if (g->layout()) {
                g->layout()->setSpacing(12);
            }
        }
        for (QLabel* l : findChildren<QLabel*>("muted"))
            l->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        for (QFrame* f : findChildren<QFrame*>("panel")) {
            QLayout* l = f->layout();
            if (!l) continue;
            if (l->contentsMargins().left() < 12) {
                l->setContentsMargins(20, 18, 20, 18);
                l->setSpacing(10);
            }
        }
        for (QTableWidget* t : findChildren<QTableWidget*>())
            t->verticalHeader()->setDefaultSectionSize(40);
        for (QSplitter* s : findChildren<QSplitter*>()) {
            s->setMinimumHeight(std::max(s->minimumHeight(), 720));
            s->setHandleWidth(8);
        }

        // Spin boxes and time/date editors: the style sheet padding does not reach
        // their inner text field, so the text sat glued to the left border.
        // Give the inner field the same left inset as the other inputs.
        for (QAbstractSpinBox* sb : findChildren<QAbstractSpinBox*>()) {
            if (QLineEdit* inner = sb->findChild<QLineEdit*>())
                inner->setTextMargins(12, 0, 0, 0);
        }
    }

    QWidget* scrollablePage(QWidget* page) {
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        page->setMinimumWidth(0);
        scroll->setWidget(page);
        return scroll;
    }

    void resizeEvent(QResizeEvent* event) override {
        QMainWindow::resizeEvent(event);
        const bool compact = width() < 1280;
        if (heroTitle) {
            const QFontMetrics fm(heroTitle->font());
            const int requiredTitleHeight = fm.lineSpacing() * 2 + 16;
            if (heroTitle->minimumHeight() != requiredTitleHeight)
                heroTitle->setMinimumHeight(requiredTitleHeight);
        }
        if (compact == compactLayout) return;
        compactLayout = compact;
        if (heroLayout) {
            heroLayout->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
            if (homeVisual) {
                homeVisual->setMinimumWidth(compact ? 0 : 272);
                homeVisual->setMaximumWidth(compact ? QWIDGETSIZE_MAX : 340);
            }
        }
        if (timetableSplit) {
            timetableSplit->setOrientation(compact ? Qt::Vertical : Qt::Horizontal);
            timetableSplit->setSizes(compact ? QList<int>{int(height() * 0.48), int(height() * 0.52)}
                                             : QList<int>{int(width() * 0.40), int(width() * 0.60)});
        }
        if (eventSplit) {
            eventSplit->setOrientation(compact ? Qt::Vertical : Qt::Horizontal);
            eventSplit->setSizes(compact ? QList<int>{int(height() * 0.42), int(height() * 0.58)}
                                         : QList<int>{int(width() * 0.40), int(width() * 0.60)});
        }
    }

    void buildUi() {
        auto* root = new QWidget;
        root->setObjectName("root");
        auto* outer = new QHBoxLayout(root);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        auto* side = new QFrame;
        side->setObjectName("sidebar");
        side->setFixedWidth(232);
        auto* sl = new QVBoxLayout(side);
        sl->setContentsMargins(0, 24, 0, 24);
        sl->setSpacing(4);

        auto* brand = new QLabel("Smart Scheduler");
        brand->setStyleSheet("font-size:20px;font-weight:600;padding:4px 20px 0px 20px;");
        auto* brandSub = new QLabel("Timetables, events and tournaments");
        brandSub->setObjectName("muted");
        brandSub->setWordWrap(true);
        brandSub->setStyleSheet("font-size:12px;padding:0px 20px 24px 20px;");
        sl->addWidget(brand);
        sl->addWidget(brandSub);

        auto* navCaption = new QLabel("Workspace");
        navCaption->setObjectName("eyebrow");
        navCaption->setStyleSheet("padding:0px 20px 8px 20px;");
        sl->addWidget(navCaption);

        navGroup = new QButtonGroup(this);
        navGroup->setExclusive(true);
        const QStringList navLabels = {"Overview", "Timetable", "Event planner", "Tournament", "Study planner"};
        for (int i = 0; i < 5; ++i) {
            navBtns[i] = new QPushButton(navLabels[i]);
            navBtns[i]->setObjectName("nav");
            navBtns[i]->setCheckable(true);
            navBtns[i]->setCursor(Qt::PointingHandCursor);
            navGroup->addButton(navBtns[i], i);
            sl->addWidget(navBtns[i]);
        }
        navBtns[0]->setChecked(true);
        sl->addStretch();
        auto* version = muted("DSA project, Qt 6");
        version->setStyleSheet("font-size:12px;padding:0px 20px;");
        sl->addWidget(version);

        auto* content = new QWidget;
        auto* cl = new QVBoxLayout(content);
        cl->setContentsMargins(36, 28, 36, 20);
        cl->setSpacing(20);

        auto* top = new QFrame;
        top->setObjectName("topbar");
        auto* tl = new QHBoxLayout(top);
        tl->setContentsMargins(0, 4, 0, 14);
        topTitle = new QLabel("Overview");
        topTitle->setStyleSheet("font-size:16px;font-weight:600;");
        tl->addWidget(topTitle);
        tl->addStretch();
        cl->addWidget(top);

        tabs = new QTabWidget;
        tabs->tabBar()->hide();
        tabs->addTab(scrollablePage(buildHomePage()), "Overview");
        tabs->addTab(scrollablePage(buildTimetablePage()), "Timetable");
        tabs->addTab(scrollablePage(buildEventPage()), "Event planner");
        tabs->addTab(scrollablePage(new TournamentPage), "Tournament");
        tabs->addTab(scrollablePage(new StudyPlannerPage([this] { return buildStudyLinked(); })), "Study planner");
        cl->addWidget(tabs, 1);

        statusLabel = muted("Add your courses to get started");
        cl->addWidget(statusLabel);

        outer->addWidget(side);
        outer->addWidget(content, 1);
        setCentralWidget(root);

        connect(tabs, &QTabWidget::currentChanged, this, [this, navLabels](int i) {
            if (i >= 0 && i < 5 && navBtns[i]) {
                navBtns[i]->setChecked(true);
                topTitle->setText(navLabels[i]);
            }
        });
        connect(navGroup, &QButtonGroup::idClicked, this, [this](int i) { tabs->setCurrentIndex(i); });
        tabs->setCurrentIndex(0);
    }

    // ---- repeating events: expand definitions into what happens in one week ----
    // `recurringOnly` is used for the study planner: it only wants things that
    // really repeat (daily / weekly), whatever week is shown.
    void buildInstances(const std::vector<AutoEvent>& defs, long long monday, int dayCount,
                        const AutoPrefs& p, bool recurringOnly,
                        std::vector<AutoEvent>& inst, std::vector<int>& src) const {
        inst.clear();
        src.clear();
        for (std::size_t i = 0; i < defs.size(); ++i) {
            const AutoEvent& ev = defs[i];
            const long long anchor = ev.anchorJd;
            if (recurringOnly && ev.repeat != 1 && ev.repeat != 2) continue;

            auto pushPlain = [&] { inst.push_back(ev); src.push_back(int(i)); };
            auto pushDay = [&](int day) {
                AutoEvent c = ev;
                c.preferredDay = day;
                inst.push_back(c);
                src.push_back(int(i));
            };
            auto dayOk = [&](int d) { return ev.customTime || (d >= 0 && d < 7 && p.dayAllowed[d]); };

            if (ev.repeat == 0) {                       // one week only
                if (mondayOfJd(anchor) == monday) pushPlain();
            } else if (ev.repeat == 1) {                // daily
                for (int d = 0; d < dayCount; ++d)
                    if ((recurringOnly || monday + d >= anchor) && dayOk(d)) pushDay(d);
            } else if (ev.repeat == 2) {                // weekly
                if (recurringOnly || monday >= mondayOfJd(anchor)) pushPlain();
            } else {                                    // monthly: same day of the month
                const QDate a = QDate::fromJulianDay(anchor);
                for (int d = 0; d < dayCount; ++d) {
                    const long long jd = monday + d;
                    if (jd < anchor) continue;
                    const QDate q = QDate::fromJulianDay(jd);
                    if (q.day() == std::min(a.day(), q.daysInMonth())) { pushDay(d); break; }
                }
            }
        }
    }

        StudyLinked buildStudyLinked() const {
        StudyLinked l;
        l.hasTimetable = hasTimetable;

        for (const AutoCourse& c : courses)
            l.courses.push_back({QString::fromStdString(c.code), QString::fromStdString(c.name)});

        // Classes repeat every week (no fixed date).
        for (const AutoSession& session : timetableResult.sessions) {
            if (session.day < 0 || session.day > 6 ||
                session.courseIndex < 0 || session.courseIndex >= int(courses.size()))
                continue;

            StudyBusy b;
            b.weekday = session.day;
            b.startMin = session.startMin;
            b.endMin = session.endMin;
            b.kind = BusyKind::Class;
            b.label = courses[session.courseIndex].code;
            l.busy.push_back(b);
        }

        // Events: every event of BOTH modes is expanded week by week (one-off,
        // daily, weekly and monthly) and handed over with its exact date, so the
        // study planner sees exactly what the Event planner shows for each week.
        const AutoPrefs up = hasTimetable ? lastPrefs : buildPrefs();
        const AutoPrefs xp = buildExternalPrefs();
        const std::vector<AutoSession> sessions =
            hasTimetable ? timetableResult.sessions : std::vector<AutoSession>();

        auto addEvents = [&l](const std::vector<AutoEvent>& ev,
                              const AutoEventResult& res,
                              BusyKind kind,
                              long long monday,
                              int& count) {
            for (const AutoEventPlacement& p : res.placements) {
                if (!p.placed || p.day < 0 || p.day > 6 ||
                    p.eventIndex < 0 || p.eventIndex >= int(ev.size()))
                    continue;

                StudyBusy b;
                b.weekday = p.day;
                b.startMin = p.startMin;
                b.endMin = p.endMin;
                b.kind = kind;
                b.label = ev[p.eventIndex].name;
                b.jd = monday + p.day;
                l.busy.push_back(b);
                ++count;
            }
        };

        // 29 weeks from this week's Monday covers the planner's 180-day horizon.
        const long long firstMonday = mondayOfJd(QDate::currentDate().toJulianDay());
        for (int w = 0; w < 29; ++w) {
            const long long monday = firstMonday + 7LL * w;

            std::vector<AutoEvent> ui, ei;
            std::vector<int> us, es;

            buildInstances(universityEvents, monday, 5, up, false, ui, us);
            if (!ui.empty()) {
                const AutoEventResult ur = AutoScheduler::placeEvents(ui, sessions, up);
                addEvents(ui, ur, BusyKind::UniEvent, monday, l.uniEvents);
            }

            buildInstances(externalEvents, monday, 7, xp, false, ei, es);
            if (!ei.empty() && xp.dayEndMin > xp.dayStartMin) {
                const AutoEventResult er = AutoScheduler::placeEvents(ei, {}, xp);
                addEvents(ei, er, BusyKind::ExternalEvent, monday, l.externalEvents);
            }
        }
        return l;
    }

    QWidget* buildHomePage() {
        auto* page = new QWidget;
        page->setMinimumWidth(0);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(24);

        auto* hero = new QFrame;
        hero->setObjectName("hero");
        hero->setStyleSheet("QFrame#hero { background:#FFFFFF; border:none; border-top:4px solid #000000; border-radius:0; }");
        heroLayout = new QBoxLayout(QBoxLayout::LeftToRight, hero);
        heroLayout->setContentsMargins(0, 28, 0, 28);
        heroLayout->setSpacing(40);

        auto* heroText = new QVBoxLayout;
        auto* eyebrow = new QLabel("Weekly planning");
        eyebrow->setObjectName("eyebrow");
        auto* title = new QLabel("Build a clash-free week\nand fit events around it");
        heroTitle = title;
        QFont heroFont = title->font();
        heroFont.setPixelSize(36);
        heroFont.setBold(true);
        title->setFont(heroFont);
        title->setStyleSheet("font-size:36px;font-weight:600;color:#000000;");
        title->setWordWrap(false);
        title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        title->setContentsMargins(0, 8, 0, 8);
        title->setMinimumHeight(QFontMetrics(title->font()).lineSpacing() * 2 + 16);
        auto* subtitle = new QLabel(
            "Add your courses and the slots each one needs. The planner builds a timetable without overlaps, "
            "then fits your events into the free time. An event that fits nowhere is placed where it disturbs "
            "the least, and the classes it overlaps are listed.");
        subtitle->setWordWrap(true);
        subtitle->setMaximumWidth(560);
        subtitle->setMinimumWidth(0);
        subtitle->setStyleSheet("font-size:16px;color:#525252;");
        auto* start = new QPushButton("Open timetable");
        start->setObjectName("primary");
        start->setMinimumHeight(44);
        start->setMaximumWidth(200);
        start->setCursor(Qt::PointingHandCursor);
        heroText->addWidget(eyebrow);
        heroText->addSpacing(8);
        heroText->addWidget(title);
        heroText->addSpacing(8);
        heroText->addWidget(subtitle);
        heroText->addSpacing(20);
        heroText->addWidget(start, 0, Qt::AlignLeft);
        heroText->addStretch();

        auto* visual = new QFrame;
        homeVisual = visual;
        visual->setObjectName("howBox");
        visual->setMinimumWidth(0);
        visual->setMaximumWidth(340);
        visual->setStyleSheet("QFrame#howBox { background:#F5F5F5; border:none; border-radius:0; }");
        auto* visualLayout = new QVBoxLayout(visual);
        visualLayout->setContentsMargins(24, 24, 24, 24);
        visualLayout->setSpacing(0);
        auto* visualTitle = new QLabel("How it works");
        visualTitle->setStyleSheet("color:#000000;font-size:12px;font-weight:500;padding-bottom:8px;");
        visualLayout->addWidget(visualTitle);
        const QStringList steps = {"1   Add courses and slots", "2   Generate your timetable",
                                   "3   Add events by name and time", "4   Plan your exam study time",
                                   "5   Export as PDF, image or CSV"};
        for (const QString& step : steps) {
            auto* item = new QLabel(step);
            item->setWordWrap(true);
            item->setStyleSheet("color:#000000;font-size:13px;font-weight:500;padding:14px 0px;"
                                "border:none;border-top:1px solid #000000;");
            visualLayout->addWidget(item);
        }
        visualLayout->addStretch();
        heroLayout->addLayout(heroText, 3);
        heroLayout->addWidget(visual, 2);
        layout->addWidget(hero);

        auto* sectionEyebrow = new QLabel("Workspaces");
        sectionEyebrow->setObjectName("eyebrow");
        auto* sectionLabel = new QLabel("Choose what you want to plan");
        sectionLabel->setStyleSheet("font-size:20px;font-weight:600;color:#000000;");
        layout->addWidget(sectionEyebrow);
        layout->addWidget(sectionLabel);

        auto* cards = new QGridLayout;
        cards->setHorizontalSpacing(20);
        cards->setVerticalSpacing(20);
        auto makeCard = [](const QString& number, const QString& name, const QString& description,
                           const QString& actionText) {
            auto* card = panel();
            auto* box = new QVBoxLayout(card);
            box->setContentsMargins(28, 28, 28, 28);
            box->setSpacing(10);
            auto* num = new QLabel(number);
            num->setObjectName("eyebrow");
            auto* headingLabel = new QLabel(name);
            headingLabel->setStyleSheet("font-size:20px;font-weight:600;color:#000000;");
            auto* desc = new QLabel(description);
            desc->setWordWrap(true);
            desc->setMinimumHeight(0);
            desc->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
            desc->setStyleSheet("color:#525252;font-size:13px;");
            auto* action = new QPushButton(actionText);
            action->setMinimumHeight(42);
            action->setCursor(Qt::PointingHandCursor);
            box->addWidget(num);
            box->addWidget(headingLabel);
            box->addWidget(desc);
            box->addStretch();
            box->addWidget(action);
            return std::pair<QFrame*, QPushButton*>(card, action);
        };
        auto timetableCard = makeCard("01  Classes", "Timetable",
                                      "Set the slot length, add your courses and let the planner place every class without overlaps. "
                                      "Turn on random gaps for days that start, break and finish at different times.",
                                      "Open timetable");
        auto eventCard = makeCard("02  Events", "Event planner",
                                  "Plan around your university timetable or switch to an independent weekly grid for parties, functions and meetups. "
                                  "Events last one week unless you make them repeat.",
                                  "Open event planner");
        auto tournamentCard = makeCard("03  Competitions", "Tournament",
                                       "Run a round robin league with semi-finals and a final, or a knockout bracket from groups "
                                       "or straight into the Round of 32. It has its own grid, fixtures and standings.",
                                       "Open tournament");
        auto studyCard = makeCard("04  Exams", "Study planner",
                                  "Enter your exam dates, times and difficulty. The planner reads your timetable and events, "
                                  "then builds day-by-day study blocks with breaks, meals and rest days. Exam days have no classes "
                                  "but you still study before and after the exam.",
                                  "Open study planner");
        cards->addWidget(timetableCard.first, 0, 0);
        cards->addWidget(eventCard.first, 0, 1);
        cards->addWidget(tournamentCard.first, 1, 0);
        cards->addWidget(studyCard.first, 1, 1);
        cards->setColumnStretch(0, 1);
        cards->setColumnStretch(1, 1);
        layout->addLayout(cards);
        layout->addStretch();

        connect(start, &QPushButton::clicked, this, [this] { tabs->setCurrentIndex(1); });
        connect(timetableCard.second, &QPushButton::clicked, this, [this] { tabs->setCurrentIndex(1); });
        connect(eventCard.second, &QPushButton::clicked, this, [this] { tabs->setCurrentIndex(2); });
        connect(tournamentCard.second, &QPushButton::clicked, this, [this] { tabs->setCurrentIndex(3); });
        connect(studyCard.second, &QPushButton::clicked, this, [this] { tabs->setCurrentIndex(4); });
        return page;
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
        l->addWidget(muted(detail));
        return f;
    }

    QWidget* buildTimetablePage() {
        auto* page = new QWidget;
        page->setMinimumWidth(0);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(20);

        auto* titleRow = new QHBoxLayout;
        auto* titleCol = new QVBoxLayout;
        auto* eyebrow = new QLabel("Academic scheduling");
        eyebrow->setObjectName("eyebrow");
        auto* title = new QLabel("Timetable generator");
        title->setObjectName("pageTitle");
        titleCol->addWidget(eyebrow);
        titleCol->addWidget(title);
        titleRow->addLayout(titleCol);
        titleRow->addStretch();

        auto* exportBtn = makeExportButton();
        auto* generate = new QPushButton("Generate timetable");
        generate->setObjectName("primary");
        generate->setMinimumHeight(40);
        generate->setToolTip("Press again for a different layout");
        generate->setCursor(Qt::PointingHandCursor);
        titleRow->addWidget(exportBtn);
        titleRow->addSpacing(8);
        titleRow->addWidget(generate);
        layout->addLayout(titleRow);

        auto* metrics = new QHBoxLayout;
        metrics->setSpacing(16);
        courseCount = new QLabel("0");
        sessionCount = new QLabel("0");
        placedCount = new QLabel("-");
        slotCountLabel = new QLabel("0");
        metrics->addWidget(metricCard("Courses", courseCount, "Courses in this plan"));
        metrics->addWidget(metricCard("Classes per week", sessionCount, "Class meetings to place"));
        metrics->addWidget(metricCard("Placed", placedCount, "Meetings scheduled"));
        metrics->addWidget(metricCard("Slots per day", slotCountLabel, "Periods in your teaching day"));
        layout->addLayout(metrics);

        timetableSplit = new QSplitter(Qt::Horizontal);
        auto* split = timetableSplit;
        split->setChildrenCollapsible(false);
        split->setMinimumHeight(720);

        auto* left = new QWidget;
        auto* ll = new QVBoxLayout(left);
        ll->setContentsMargins(0, 0, 16, 0);
        ll->setSpacing(16);

        auto* form = new QGroupBox("Add course");
        auto* fg = new QGridLayout(form);
        fg->setHorizontalSpacing(16);
        fg->setVerticalSpacing(10);
        courseCode = new QLineEdit;
        courseCode->setPlaceholderText("e.g. CS2002");
        courseName = new QLineEdit;
        courseName->setPlaceholderText("Computer Organization");
        slotsPerSession = new QSpinBox;
        slotsPerSession->setRange(1, 8);
        slotsPerSession->setValue(2);
        slotsPerSession->setSuffix(" slots");
        sessionsPerWeek = new QSpinBox;
        sessionsPerWeek->setRange(1, 7);
        sessionsPerWeek->setValue(2);
        sessionsPerWeek->setSuffix(" / week");
        fg->addWidget(muted("Course code"), 0, 0);
        fg->addWidget(muted("Course name"), 0, 1);
        fg->addWidget(courseCode, 1, 0);
        fg->addWidget(courseName, 1, 1);
        fg->addWidget(muted("Slots per class"), 2, 0);
        fg->addWidget(muted("Classes per week"), 2, 1);
        fg->addWidget(slotsPerSession, 3, 0);
        fg->addWidget(sessionsPerWeek, 3, 1);
        addCourseBtn = new QPushButton("Add course");
        addCourseBtn->setObjectName("primary");
        cancelCourseEditBtn = new QPushButton("Cancel edit");
        cancelCourseEditBtn->setObjectName("quiet");
        cancelCourseEditBtn->setVisible(false);
        fg->addWidget(addCourseBtn, 4, 0, 1, 2);
        fg->addWidget(cancelCourseEditBtn, 5, 0, 1, 2);
        fg->addWidget(muted("Days and times are chosen for you so nothing overlaps. Select a course in the list and press Edit (or double-click it) to change it."), 6, 0, 1, 2);
        ll->addWidget(form);

        auto* prefs = new QGroupBox("Slots and day");
        auto* pg = new QGridLayout(prefs);
        pg->setHorizontalSpacing(16);
        pg->setVerticalSpacing(10);
        slotMinutes = new QSpinBox;
        slotMinutes->setRange(20, 240);
        slotMinutes->setSingleStep(5);
        slotMinutes->setValue(50);
        slotMinutes->setSuffix(" min");
        slotBreak = new QSpinBox;
        slotBreak->setRange(0, 30);
        slotBreak->setSingleStep(5);
        slotBreak->setValue(10);
        slotBreak->setSuffix(" min");
        dayStart = new QTimeEdit(QTime(8, 0));
        dayStart->setDisplayFormat("HH:mm");
        dayEnd = new QTimeEdit(QTime(16, 0));
        dayEnd->setDisplayFormat("HH:mm");
        reserveDay = new QComboBox;
        reserveDay->addItems({"No reserve day", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday"});
        reserveDay->setCurrentIndex(5);
        longBreakCheck = new QCheckBox("Random free blocks between classes");
        longBreakCheck->setChecked(true);
        longMin = new QSpinBox;
        longMin->setRange(20, 240);
        longMin->setSingleStep(10);
        longMin->setValue(50);
        longMin->setSuffix(" min");
        longMax = new QSpinBox;
        longMax->setRange(20, 300);
        longMax->setSingleStep(10);
        longMax->setValue(110);
        longMax->setSuffix(" min");
        slotInfo = muted("");

        pg->addWidget(muted("1 slot lasts"), 0, 0);
        pg->addWidget(slotMinutes, 0, 1);
        pg->addWidget(muted("Break between slots"), 1, 0);
        pg->addWidget(slotBreak, 1, 1);
        pg->addWidget(muted("Classes start at"), 2, 0);
        pg->addWidget(dayStart, 2, 1);
        pg->addWidget(muted("Classes end by"), 3, 0);
        pg->addWidget(dayEnd, 3, 1);
        pg->addWidget(muted("Reserve day (no classes)"), 4, 0);
        pg->addWidget(reserveDay, 4, 1);
        pg->addWidget(longBreakCheck, 5, 0, 1, 2);
        pg->addWidget(muted("Free block from"), 6, 0);
        pg->addWidget(longMin, 6, 1);
        pg->addWidget(muted("up to"), 7, 0);
        pg->addWidget(longMax, 7, 1);
        pg->addWidget(muted("Each day gets its own mix: some packed, some with long gaps, some finishing early"), 8, 0, 1, 2);
        pg->addWidget(slotInfo, 9, 0, 1, 2);
        ll->addWidget(prefs);
        ll->addStretch();

        auto* leftScroll = new QScrollArea;
        leftScroll->setWidgetResizable(true);
        leftScroll->setFrameShape(QFrame::NoFrame);
        leftScroll->setMinimumWidth(400);
        leftScroll->setWidget(left);

        auto* right = new QWidget;
        auto* rl = new QVBoxLayout(right);
        rl->setContentsMargins(16, 0, 0, 0);
        rl->setSpacing(16);

        auto* coursePanel = panel();
        auto* cpl = new QVBoxLayout(coursePanel);
        cpl->setContentsMargins(20, 18, 20, 18);
        cpl->setSpacing(10);
        cpl->addWidget(heading("Course list"));
        cpl->addWidget(muted("Each course with its weekly slot needs"));
        courseTable = new QTableWidget;
        setupTable(courseTable, {"Course", "Name", "Slots / class", "Classes / week", "Class length"});
        cpl->addWidget(courseTable);
        auto* courseBtnRow = new QHBoxLayout;
        auto* editCourse = new QPushButton("Edit selected course");
        editCourse->setObjectName("quiet");
        auto* remove = new QPushButton("Remove selected course");
        remove->setObjectName("quiet");
        courseBtnRow->addStretch();
        courseBtnRow->addWidget(remove);
        courseBtnRow->addWidget(editCourse);
        cpl->addLayout(courseBtnRow);
        rl->addWidget(coursePanel, 1);

        auto* resultPanel = panel();
        auto* rpl = new QVBoxLayout(resultPanel);
        rpl->setContentsMargins(20, 18, 20, 18);
        rpl->setSpacing(10);
        rpl->addWidget(heading("Weekly timetable"));
        rpl->addWidget(muted("Every class sits in a free slot. University events of the week chosen in the Event planner appear here too, and overlaps are marked"));
        auto* resultTabs = new QTabWidget;
        timetableOutput = new QTextBrowser;
        timetableOutput->setMinimumHeight(240);
        timetableOutput->setHtml(placeholderHtml("Your timetable will appear here", "Add your courses, then press Generate."));
        auto* gridScroll = new QScrollArea;
        gridScroll->setWidgetResizable(false);
        gridScroll->setFrameShape(QFrame::NoFrame);
        gridLabel = new QLabel;
        gridLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        gridScroll->setWidget(gridLabel);
        resultTabs->addTab(gridScroll, "Grid view");
        resultTabs->addTab(timetableOutput, "Day list");
        rpl->addWidget(resultTabs, 1);
        rl->addWidget(resultPanel, 2);

        split->addWidget(leftScroll);
        split->addWidget(right);
        split->setStretchFactor(0, 4);
        split->setStretchFactor(1, 6);
        layout->addWidget(split, 1);

        connect(addCourseBtn, &QPushButton::clicked, this, [this] { addCourseEntry(); });
        connect(cancelCourseEditBtn, &QPushButton::clicked, this, [this] { cancelCourseEdit(); });
        connect(editCourse, &QPushButton::clicked, this, [this] { editSelectedCourse(); });
        connect(courseTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editSelectedCourse(); });
        connect(remove, &QPushButton::clicked, this, [this] { removeSelectedCourse(); });
        connect(generate, &QPushButton::clicked, this, [this] { generateTimetable(); });
        auto slotChanged = [this] { updateSlotInfo(); refreshCourseTable(); };
        connect(slotMinutes, qOverload<int>(&QSpinBox::valueChanged), this, slotChanged);
        connect(slotBreak, qOverload<int>(&QSpinBox::valueChanged), this, slotChanged);
        connect(dayStart, &QTimeEdit::timeChanged, this, slotChanged);
        connect(dayEnd, &QTimeEdit::timeChanged, this, slotChanged);
        connect(longBreakCheck, &QCheckBox::toggled, this, [this](bool on) {
            longMin->setEnabled(on);
            longMax->setEnabled(on);
        });
        return page;
    }

    QToolButton* makeExportButton() {
        auto* btn = new QToolButton;
        btn->setObjectName("exportBtn");
        btn->setText("Export");
        btn->setMinimumHeight(40);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setPopupMode(QToolButton::InstantPopup);
        auto* menu = new QMenu(btn);
        menu->addAction("PDF  (print-ready)", this, [this] { exportAs("pdf"); });
        menu->addAction("Image  (JPG)", this, [this] { exportAs("jpg"); });
        menu->addAction("Spreadsheet  (CSV)", this, [this] { exportAs("csv"); });
        menu->addSeparator();
        menu->addAction("Calendar  (ICS)", this, [this] { exportAs("ics"); });
        menu->addAction("Data  (JSON)", this, [this] { exportAs("json"); });
        btn->setMenu(menu);
        return btn;
    }

    static QString placeholderHtml(const QString& head, const QString& body) {
        return "<div style='color:#525252;padding:24px;text-align:center'><h2 style='color:#000000'>" + head +
               "</h2><p>" + body + "</p></div>";
    }

    std::vector<AutoEvent>& activeEvents() { return externalMode ? externalEvents : universityEvents; }
    const std::vector<AutoEvent>& activeEvents() const { return externalMode ? externalEvents : universityEvents; }
    const std::vector<AutoEvent>& activeInst() const { return externalMode ? extInst : uniInst; }
    const std::vector<int>& activeSrc() const { return externalMode ? extSrc : uniSrc; }
    AutoEventResult& activeEventResult() { return externalMode ? externalEventResult : universityEventResult; }
    const AutoEventResult& activeEventResult() const { return externalMode ? externalEventResult : universityEventResult; }

    AutoPrefs buildExternalPrefs() const {
        AutoPrefs p;
        p.dayStartMin = externalDayStart->time().hour() * 60 + externalDayStart->time().minute();
        p.dayEndMin = externalDayEnd->time().hour() * 60 + externalDayEnd->time().minute();
        p.slotMin = externalSlotMinutes->value();
        p.slotBreakMin = externalBreakMinutes->value();
        p.eventStartStepMin = p.slotMin;
        p.longBreaks = false;
        for (int d = 0; d < 7; ++d) p.dayAllowed[d] = externalDayChecks[d] && externalDayChecks[d]->isChecked();
        return p;
    }

    AutoPrefs buildPrefs() const {
        AutoPrefs p;
        p.dayStartMin = dayStart->time().hour() * 60 + dayStart->time().minute();
        p.dayEndMin = dayEnd->time().hour() * 60 + dayEnd->time().minute();
        p.slotMin = slotMinutes->value();
        p.slotBreakMin = slotBreak->value();
        p.longBreaks = longBreakCheck->isChecked();
        p.longBreakMinMin = longMin->value();
        p.longBreakMaxMin = std::max(longMax->value(), longMin->value());
        const int reserve = reserveDay->currentIndex() - 1;
        for (int d = 0; d < 7; ++d) p.dayAllowed[d] = (d < 5 && d != reserve);
        return p;
    }

    AutoPrefs activePrefs() const { return externalMode ? buildExternalPrefs() : (hasTimetable ? lastPrefs : buildPrefs()); }

    void updateSlotInfo() {
        const AutoPrefs p = buildPrefs();
        const int n = AutoScheduler::slotsPerDay(p);
        slotCountLabel->setText(QString::number(n));
        slotInfo->setText(QString("%1 - %2 gives %3 slots of %4 min with %5 min between them. "
                                  "A 3-slot class lasts %6.")
                              .arg(timeText(p.dayStartMin), timeText(p.dayEndMin))
                              .arg(n).arg(p.slotMin).arg(p.slotBreakMin)
                              .arg(durationText(AutoScheduler::sessionMinutes(3, p))));
    }

    void updateWeekLabel() {
        if (!weekLabel) return;
        QString t = QString("Week of %1 - %2")
                        .arg(QDate::fromJulianDay(viewMonday).toString("d MMM"),
                             QDate::fromJulianDay(viewMonday + 6).toString("d MMM yyyy"));
        if (viewMonday == mondayOfJd(QDate::currentDate().toJulianDay())) t += "   (this week)";
        weekLabel->setText(t);
    }

    // delta: -1 previous week, +1 next week, 0 back to the current week.
    void shiftWeek(int delta) {
        viewMonday = delta == 0 ? mondayOfJd(QDate::currentDate().toJulianDay()) : viewMonday + 7LL * delta;
        cancelEventEdit();
        updateWeekLabel();
        runUniversityEventPlacement();
        runExternalEventPlacement();
        refreshAfterEventChange();
        statusLabel->setText(weekLabel ? weekLabel->text() : QString());
    }

    // ---- courses: add / edit / remove -----------------------------------
    void cancelCourseEdit() {
        editingCourseId = -1;
        addCourseBtn->setText("Add course");
        cancelCourseEditBtn->setVisible(false);
        courseCode->clear();
        courseName->clear();
        slotsPerSession->setValue(2);
        sessionsPerWeek->setValue(2);
    }

    void editSelectedCourse() {
        const int row = courseTable->currentRow();
        if (row < 0 || !courseTable->item(row, 0)) {
            QMessageBox::information(this, "Select a course", "Select a course in the list first.");
            return;
        }
        const int id = courseTable->item(row, 0)->data(Qt::UserRole).toInt();
        for (const AutoCourse& c : courses) {
            if (c.id != id) continue;
            editingCourseId = id;
            courseCode->setText(qstr(c.code));
            courseName->setText(qstr(c.name));
            slotsPerSession->setValue(c.slotsPerSession);
            sessionsPerWeek->setValue(c.sessionsPerWeek);
            addCourseBtn->setText("Save changes");
            cancelCourseEditBtn->setVisible(true);
            statusLabel->setText("Editing " + qstr(c.code) + ". Change the details and press Save changes.");
            courseCode->setFocus();
            return;
        }
    }

    void addCourseEntry() {
        const QString code = courseCode->text().trimmed().toUpper(), name = courseName->text().trimmed();
        if (code.isEmpty() || name.isEmpty()) {
            QMessageBox::warning(this, "Missing information", "Enter the course code and course name.");
            return;
        }
        for (const AutoCourse& c : courses)
            if (qstr(c.code) == code && c.id != editingCourseId) {
                QMessageBox::warning(this, "Duplicate course", "This course code was already added.");
                return;
            }
        if (editingCourseId >= 0) {
            for (AutoCourse& c : courses) {
                if (c.id != editingCourseId) continue;
                c.code = code.toStdString();
                c.name = name.toStdString();
                c.slotsPerSession = slotsPerSession->value();
                c.sessionsPerWeek = sessionsPerWeek->value();
                break;
            }
            cancelCourseEdit();
            clearTimetable();
            refreshCourseTable();
            statusLabel->setText("Course updated. Press Generate to rebuild the timetable.");
            return;
        }
        AutoCourse c;
        c.id = nextCourseId++;
        c.code = code.toStdString();
        c.name = name.toStdString();
        c.slotsPerSession = slotsPerSession->value();
        c.sessionsPerWeek = sessionsPerWeek->value();
        courses.push_back(c);
        courseCode->clear();
        courseName->clear();
        courseCode->setFocus();
        clearTimetable();
        refreshCourseTable();
        statusLabel->setText("Course added. Press Generate to build the timetable.");
    }

    void refreshCourseTable() {
        if (!courseTable) return;
        const AutoPrefs p = buildPrefs();
        courseTable->setRowCount(0);
        int sessions = 0;
        for (const AutoCourse& c : courses) {
            const int r = courseTable->rowCount();
            courseTable->insertRow(r);
            sessions += c.sessionsPerWeek;
            const QStringList vals = {qstr(c.code), qstr(c.name), QString::number(c.slotsPerSession),
                                      QString::number(c.sessionsPerWeek),
                                      durationText(AutoScheduler::sessionMinutes(c.slotsPerSession, p))};
            for (int i = 0; i < vals.size(); ++i) courseTable->setItem(r, i, new QTableWidgetItem(vals[i]));
            courseTable->item(r, 0)->setData(Qt::UserRole, c.id);
        }
        courseCount->setText(QString::number(courses.size()));
        sessionCount->setText(QString::number(sessions));
    }

    void removeSelectedCourse() {
        const int row = courseTable->currentRow();
        if (row < 0) return;
        const int id = courseTable->item(row, 0)->data(Qt::UserRole).toInt();
        if (editingCourseId == id) cancelCourseEdit();
        for (std::size_t i = 0; i < courses.size(); ++i)
            if (courses[i].id == id) {
                courses.erase(courses.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        clearTimetable();
        refreshCourseTable();
    }

    void clearTimetable() {
        hasTimetable = false;
        placedCount->setText("-");
        gridLabel->clear();
        gridLabel->resize(0, 0);
        timetableOutput->setHtml(placeholderHtml("Your timetable will appear here", "Add your courses, then press Generate."));
        runUniversityEventPlacement();
        refreshAfterEventChange();
    }

    void generateTimetable() {
        if (courses.empty()) {
            QMessageBox::information(this, "No courses", "Add at least one course first.");
            return;
        }
        AutoPrefs p = buildPrefs();
        p.seed = QRandomGenerator::global()->generate() | 1u;
        timetableResult = AutoScheduler::schedule(courses, p);

        if (!timetableResult.ok) {
            clearTimetable();
            timetableOutput->setHtml("<div style='background-color:#F5F5F5;color:#000000;padding:12px;font-weight:600'>"
                                     "<span style='color:#DC2626'>&#9632;</span> " +
                                     qstr(timetableResult.message).toHtmlEscaped() + "</div>");
            placedCount->setText("0");
            statusLabel->setText("Could not build a timetable.");
            return;
        }
        hasTimetable = true;
        lastPrefs = p;
        runUniversityEventPlacement();
        refreshAfterEventChange();
        placedCount->setText(QString::number(timetableResult.sessions.size()));
        statusLabel->setText(qstr(timetableResult.message));
    }

    QString clashText(const AutoEventPlacement& ep, bool withTimes, bool useExternalEvents) const {
        const std::vector<AutoEvent>& list = useExternalEvents ? extInst : uniInst;
        QStringList parts;
        for (const AutoClash& c : ep.clashes) {
            QString who;
            if (c.courseIndex >= 0 && c.courseIndex < int(courses.size()))
                who = qstr(courses[static_cast<std::size_t>(c.courseIndex)].code);
            else if (c.eventIndex >= 0 && c.eventIndex < int(list.size()))
                who = qstr(list[static_cast<std::size_t>(c.eventIndex)].name);
            else
                continue;
            if (withTimes)
                who += " (" + shortDay(c.day) + " " + timeText(c.startMin) + " - " + timeText(c.endMin) + ")";
            parts << who;
        }
        return parts.join(", ");
    }

    void renderTimetable() {
        if (!hasTimetable) return;
        QString html = "<div style='color:#000000'>";
        html += "<div style='color:#525252;font-weight:600'>" + qstr(timetableResult.message).toHtmlEscaped() + "</div>";
        html += "<div style='color:#525252;margin-top:4px'>Events shown for: " + weekLabel->text().toHtmlEscaped() + "</div>";
        const AutoPrefs& p = lastPrefs;
        for (int d = 0; d < 5; ++d) {
            html += "<div style='background-color:#F5F5F5;padding:12px;margin-top:8px'>";
            html += "<b style='font-size:16px'>" + dayName(d) + "</b>";
            if (!p.dayAllowed[d]) { html += "<div style='color:#525252;margin-top:8px'>Reserve day - no classes</div></div>"; continue; }
            bool any = false;
            for (const AutoSession& s : timetableResult.sessions) {
                if (s.day != d) continue;
                any = true;
                const AutoCourse& c = courses[static_cast<std::size_t>(s.courseIndex)];
                html += "<div style='margin-top:8px'><span style='background-color:" + courseColor(s.courseIndex).name() +
                        "'>&nbsp;&nbsp;&nbsp;</span> <b>" + timeText(s.startMin) + " - " + timeText(s.endMin) +
                        "</b> &nbsp; <b>" + qstr(c.code).toHtmlEscaped() + "</b> &nbsp; <span style='color:#525252'>" +
                        qstr(c.name).toHtmlEscaped() + " (" +
                        QString::number(s.slotCount) + (s.slotCount == 1 ? " slot)" : " slots)") + "</span></div>";
            }
            for (const AutoEventPlacement& ep : universityEventResult.placements) {
                if (!ep.placed || ep.day != d || ep.eventIndex >= int(uniInst.size())) continue;
                html += "<div style='margin-top:8px'><span style='background-color:#DC2626'>&nbsp;&nbsp;&nbsp;</span> <b style='color:#DC2626'>" +
                        timeText(ep.startMin) + " - " + timeText(ep.endMin) +
                        "</b> &nbsp; <b>Event</b> &nbsp; <span style='color:#525252'>" +
                        qstr(uniInst[static_cast<std::size_t>(ep.eventIndex)].name).toHtmlEscaped() + "</span>";
                if (!ep.clashes.empty())
                    html += " &nbsp; <b style='color:#DC2626'>overlaps " + clashText(ep, false, false).toHtmlEscaped() + "</b>";
                html += "</div>";
                any = true;
            }
            if (!any) html += "<div style='color:#525252;margin-top:8px'>Free day</div>";
            html += "</div>";
        }
        html += "</div>";
        timetableOutput->setHtml(html);
    }

    void renderGrids() {
        if (hasTimetable) setGridPixmap(gridLabel, false);
        setGridPixmap(eventGridLabel, true);
    }

    void setGridPixmap(QLabel* label, bool eventFocus) {
        if (!label) return;
        QImage img(1500, 940, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        {
            QPainter painter(&img);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            paintGrid(painter, QRectF(0, 0, img.width(), img.height()), eventFocus);
        }
        label->setPixmap(QPixmap::fromImage(img));
        label->resize(img.size());
    }

    void paintGrid(QPainter& p, const QRectF& area, bool eventFocus = false) const {
        const AutoPrefs pr = (eventFocus && externalMode) ? buildExternalPrefs() : (hasTimetable ? lastPrefs : buildPrefs());
        const int dayCount = (eventFocus && externalMode) ? 7 : 5;
        int start = pr.dayStartMin, end = pr.dayEndMin;
        if (eventFocus) {
            const auto& gridEvents = activeInst();
            for (const AutoEvent& ev : gridEvents) {
                if (!ev.customTime) continue;
                start = std::min(start, (ev.customStartMin / 60) * 60);
                end = std::max(end, ((ev.customEndMin + 59) / 60) * 60);
            }
        }
        const int total = end - start;
        if (total <= 0) return;
        const double u = std::min(area.width() / 100.0, area.height() / 66.0);

        p.save();
        QFont f = baseFont();
        const int basePx = std::max(9, int(std::round(u * 1.3)));
        f.setPixelSize(basePx);
        p.setFont(f);

        const double frameW = std::max(1.0, u * 0.14);
        p.setPen(QPen(INK, frameW));
        p.setBrush(PAPER);
        p.drawRect(area.adjusted(frameW / 2, frameW / 2, -frameW / 2, -frameW / 2));

        const double pad = u * 1.2, timeW = u * 6, headH = u * 4;
        const QRectF grid(area.left() + pad + timeW, area.top() + pad + headH,
                          area.width() - 2 * pad - timeW, area.height() - 2 * pad - headH);
        const double colW = grid.width() / double(dayCount);
        auto Y = [&](int m) { return grid.top() + (m - start) * grid.height() / double(total); };

        QFont hf = f;
        hf.setBold(true);
        hf.setPixelSize(std::max(8, int(u * 1.5)));
        for (int d = 0; d < dayCount; ++d) {
            const QRectF col(grid.left() + d * colW, grid.top(), colW, grid.height());
            if (!pr.dayAllowed[d]) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#EFEFEF"));
                p.drawRect(col);
                p.setPen(QColor("#737373"));
                p.setFont(f);
                p.drawText(col, Qt::AlignCenter, (eventFocus && externalMode) ? "Unavailable" : "Reserve day");
            }
            p.setPen(INK);
            p.setFont(hf);
            p.drawText(QRectF(col.left(), area.top() + pad, colW, headH), Qt::AlignCenter, dayName(d));
        }

        p.setFont(f);
        for (int t = ((start + 59) / 60) * 60; t <= end; t += 60) {
            const double y = Y(t);
            p.setPen(QPen(HAIRLINE, 1));
            p.drawLine(QPointF(grid.left(), y), QPointF(grid.right(), y));
            p.setPen(MUTED);
            p.drawText(QRectF(area.left() + pad, y - u * 1.2, timeW - u * 0.6, u * 2.4),
                       Qt::AlignRight | Qt::AlignVCenter, timeText(t));
        }
        if (eventFocus && externalMode && pr.slotMin > 0) {
            for (int t = start + pr.slotMin; t < end; t += pr.slotMin) {
                if (t % 60 == 0) continue;
                p.setPen(QPen(QColor("#EEEEEE"), 1, Qt::DotLine));
                p.drawLine(QPointF(grid.left(), Y(t)), QPointF(grid.right(), Y(t)));
            }
        }
        p.setPen(QPen(HAIRLINE, 1));
        for (int d = 0; d <= dayCount; ++d)
            p.drawLine(QPointF(grid.left() + d * colW, grid.top()), QPointF(grid.left() + d * colW, grid.bottom()));
        p.setPen(QPen(INK, std::max(1.5, u * 0.2)));
        p.drawLine(QPointF(grid.left(), grid.top()), QPointF(grid.right(), grid.top()));

        for (const AutoSession& s : timetableResult.sessions) {
            if ((eventFocus && externalMode) || !hasTimetable || s.day < 0 || s.day >= dayCount || s.courseIndex >= int(courses.size())) continue;
            const AutoCourse& c = courses[static_cast<std::size_t>(s.courseIndex)];
            const QRectF r(grid.left() + s.day * colW + u * 0.35, Y(s.startMin) + 1,
                           colW - u * 0.7, Y(s.endMin) - Y(s.startMin) - 2);
            const QColor fill = eventFocus ? QColor("#E5E5E5") : courseColor(s.courseIndex);
            const QColor ink = eventFocus ? MUTED : textOn(fill);
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawRect(r);
            QColor divider = ink;
            divider.setAlpha(70);
            p.setPen(QPen(divider, 1));
            for (int k = 1; k < s.slotCount; ++k) {
                const double y = Y(AutoScheduler::slotStart(s.startSlot + k, pr)) - 1;
                p.drawLine(QPointF(r.left() + u * 0.6, y), QPointF(r.right() - u * 0.6, y));
            }
            drawFitted(p, r, qstr(c.code), qstr(c.name),
                       timeText(s.startMin) + " - " + timeText(s.endMin),
                       ink, basePx, u * 0.9, u * 0.55);
        }
        const auto& shownEvents = eventFocus ? activeInst() : uniInst;
        const auto& shownResult = eventFocus ? activeEventResult() : universityEventResult;
        for (const AutoEventPlacement& ep : shownResult.placements) {
            if (!ep.placed || ep.day < 0 || ep.day >= dayCount || ep.eventIndex >= int(shownEvents.size())) continue;
            const bool overlaps = !ep.clashes.empty();
            const double colX = grid.left() + ep.day * colW + u * 0.35;
            const double colInner = colW - u * 0.7;
            const double inset = overlaps ? colInner * 0.38 : 0.0;
            const QRectF r(colX + inset, Y(ep.startMin) + 1, colInner - inset, Y(ep.endMin) - Y(ep.startMin) - 2);
            if (overlaps) p.setPen(QPen(INK, std::max(1.5, u * 0.2), Qt::DashLine));
            else p.setPen(Qt::NoPen);
            p.setBrush(ACCENT);
            p.drawRect(r);
            const QString sub = overlaps ? "Overlaps " + clashText(ep, false, eventFocus ? externalMode : false) : QString();
            drawFitted(p, r, qstr(shownEvents[static_cast<std::size_t>(ep.eventIndex)].name), sub,
                       timeText(ep.startMin) + " - " + timeText(ep.endMin),
                       PAPER, basePx, u * 0.9, u * 0.55);
        }
        p.restore();
    }

    void paintLegend(QPainter& p, const QRectF& area, bool eventMode) const {
        const double u = area.width() / 100.0;
        p.save();
        QFont f = baseFont();
        f.setPixelSize(std::max(7, int(u * 1.3)));
        p.setFont(f);
        double x = area.left(), y = area.top();
        const double rowH = u * 2.4;
        auto item = [&](const QColor& color, const QString& label) {
            const double w = u * 3.2 + p.fontMetrics().horizontalAdvance(label) + u * 2.0;
            if (x + w > area.right()) { x = area.left(); y += rowH; }
            if (y + rowH > area.bottom() + rowH) return;
            p.setPen(QPen(INK, 1));
            p.setBrush(color);
            p.drawRect(QRectF(x, y + u * 0.3, u * 1.8, u * 1.5));
            p.setPen(INK);
            p.drawText(QRectF(x + u * 2.5, y, w, rowH), Qt::AlignLeft | Qt::AlignVCenter, label);
            x += w;
        };
        if (eventMode) {
            if (!externalMode && hasTimetable) item(QColor("#E5E5E5"), "Classes");
        } else {
            for (std::size_t i = 0; i < courses.size(); ++i)
                item(courseColor(int(i)), qstr(courses[i].code) + "  " + qstr(courses[i].name));
        }
        const auto& legendEvents = eventMode ? activeInst() : uniInst;
        const auto& legendResult = eventMode ? activeEventResult() : universityEventResult;
        if (!legendEvents.empty()) item(ACCENT, "Events");
        if (legendResult.clashingEvents > 0) item(ACCENT, eventMode && externalMode ? "Dashed outline: event conflict" : "Dashed outline: event overlaps a class");
        p.restore();
    }

    void composeSheet(QPainter& p, const QRectF& page, bool eventMode) const {
        const AutoPrefs pr = eventMode ? activePrefs() : (hasTimetable ? lastPrefs : buildPrefs());
        const double u = page.width() / 100.0;
        p.save();
        QFont tf = baseFont();
        tf.setBold(true);
        tf.setPixelSize(int(u * 2.5));
        p.setFont(tf);
        p.setPen(INK);
        p.drawText(QRectF(page.left(), page.top(), page.width(), u * 3.4), Qt::AlignLeft | Qt::AlignVCenter,
                   eventMode ? "Event Schedule" : "Weekly Timetable");
        QFont sf = baseFont();
        sf.setPixelSize(int(u * 1.2));
        p.setFont(sf);
        p.setPen(MUTED);
        p.drawText(QRectF(page.left(), page.top() + u * 3.3, page.width(), u * 2.0), Qt::AlignLeft | Qt::AlignVCenter,
                   QString("%1 - %2   |   %3-minute slots, %4-minute breaks   |   %5")
                       .arg(timeText(pr.dayStartMin), timeText(pr.dayEndMin)).arg(pr.slotMin).arg(pr.slotBreakMin)
                       .arg(weekLabel ? weekLabel->text() : QDate::currentDate().toString("d MMM yyyy")));
        const double legendH = u * 7.5, top = page.top() + u * 6.0;
        paintGrid(p, QRectF(page.left(), top, page.width(), page.height() - (top - page.top()) - legendH - u), eventMode);
        paintLegend(p, QRectF(page.left(), page.bottom() - legendH, page.width(), legendH), eventMode);
        p.restore();
    }

    QWidget* buildEventPage() {
        auto* page = new QWidget;
        page->setMinimumWidth(0);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(20);

        auto* row = new QHBoxLayout;
        auto* col = new QVBoxLayout;
        auto* eyebrow = new QLabel("Events and activities");
        eyebrow->setObjectName("eyebrow");
        auto* title = new QLabel("Event planner");
        title->setObjectName("pageTitle");
        col->addWidget(eyebrow);
        col->addWidget(title);
        row->addLayout(col);
        row->addStretch();
        row->addWidget(makeExportButton());
        auto* refit = new QPushButton("Re-fit events");
        refit->setObjectName("primary");
        refit->setMinimumHeight(40);
        refit->setToolTip("Recalculate the active event schedule using its own planning mode");
        refit->setCursor(Qt::PointingHandCursor);
        row->addSpacing(8);
        row->addWidget(refit);
        layout->addLayout(row);

        auto* modePanel = panel();
        auto* modeLayout = new QVBoxLayout(modePanel);
        modeLayout->setContentsMargins(20, 18, 20, 18);
        modeLayout->setSpacing(10);
        auto* modeRow = new QHBoxLayout;
        modeRow->addWidget(heading("Planning mode"));
        eventModeBox = new QComboBox;
        eventModeBox->addItems({"University timetable", "External events"});
        eventModeBox->setMinimumWidth(240);
        modeRow->addSpacing(12);
        modeRow->addWidget(eventModeBox);
        modeRow->addStretch();
        modeLayout->addLayout(modeRow);
        auto* modeDescription = muted("University mode fits events around your saved generated timetable. External mode uses a separate weekly grid and its own time settings.");
        modeDescription->setObjectName("muted");
        modeLayout->addWidget(modeDescription);
        externalSettings = new QWidget;
        auto* extGrid = new QGridLayout(externalSettings);
        extGrid->setContentsMargins(0, 6, 0, 0);
        extGrid->setHorizontalSpacing(16);
        extGrid->setVerticalSpacing(8);
        externalDayStart = new QTimeEdit(QTime(9, 0));
        externalDayEnd = new QTimeEdit(QTime(23, 0));
        externalDayStart->setDisplayFormat("HH:mm");
        externalDayEnd->setDisplayFormat("HH:mm");
        externalSlotMinutes = new QSpinBox; externalSlotMinutes->setRange(5, 240); externalSlotMinutes->setValue(30); externalSlotMinutes->setSuffix(" min");
        externalBreakMinutes = new QSpinBox; externalBreakMinutes->setRange(0, 120); externalBreakMinutes->setValue(0); externalBreakMinutes->setSuffix(" min");
        extGrid->addWidget(muted("Grid starts"), 0, 0); extGrid->addWidget(externalDayStart, 1, 0);
        extGrid->addWidget(muted("Grid ends"), 0, 1); extGrid->addWidget(externalDayEnd, 1, 1);
        extGrid->addWidget(muted("Grid slot"), 0, 2); extGrid->addWidget(externalSlotMinutes, 1, 2);
        extGrid->addWidget(muted("Gap between events"), 0, 3); extGrid->addWidget(externalBreakMinutes, 1, 3);
        auto* daysRow = new QHBoxLayout;
        daysRow->setSpacing(16);
        const QStringList externalDays = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        for (int d = 0; d < 7; ++d) {
            externalDayChecks[d] = new QCheckBox(externalDays[d]);
            externalDayChecks[d]->setChecked(true);
            daysRow->addWidget(externalDayChecks[d]);
        }
        daysRow->addStretch();
        extGrid->addLayout(daysRow, 2, 0, 1, 4);
        externalSettings->setVisible(false);
        modeLayout->addWidget(externalSettings);
        layout->addWidget(modePanel);

        // ---- week navigation ----
        auto* weekPanel = panel();
        auto* weekLayout = new QHBoxLayout(weekPanel);
        weekLayout->setContentsMargins(20, 14, 20, 14);
        weekLayout->setSpacing(10);
        auto* prevWeek = new QPushButton("\u2039  Previous week");
        auto* thisWeek = new QPushButton("This week");
        auto* nextWeek = new QPushButton("Next week  \u203A");
        for (QPushButton* b : {prevWeek, thisWeek, nextWeek}) b->setCursor(Qt::PointingHandCursor);
        weekLabel = new QLabel;
        weekLabel->setStyleSheet("font-size:16px;font-weight:600;color:#000000;");
        weekLayout->addWidget(weekLabel);
        weekLayout->addStretch();
        weekLayout->addWidget(prevWeek);
        weekLayout->addWidget(thisWeek);
        weekLayout->addWidget(nextWeek);
        layout->addWidget(weekPanel);

        auto* metrics = new QHBoxLayout;
        metrics->setSpacing(16);
        eventCount = new QLabel("0");
        eventPlaced = new QLabel("0");
        eventOverlap = new QLabel("0");
        metrics->addWidget(metricCard("Events", eventCount, "Events in your list"));
        metrics->addWidget(metricCard("Scheduled", eventPlaced, "Event slots in the shown week"));
        metrics->addWidget(metricCard("Conflicts", eventOverlap, "Events that could not fit cleanly"));
        layout->addLayout(metrics);

        eventSplit = new QSplitter(Qt::Horizontal);
        auto* split = eventSplit;
        split->setChildrenCollapsible(false);
        split->setMinimumHeight(720);
        auto* left = new QWidget;
        auto* ll = new QVBoxLayout(left);
        ll->setContentsMargins(0, 0, 16, 0);
        ll->setSpacing(16);

        auto* quick = new QGroupBox("Quick add");
        auto* qg = new QVBoxLayout(quick);
        qg->setSpacing(10);
        eventQuick = new QLineEdit;
        eventQuick->setPlaceholderText("Orientation 90, Club meeting 45 min, Workshop 1h30");
        auto* quickBtn = new QPushButton("Add and schedule");
        quickBtn->setObjectName("primary");
        qg->addWidget(muted("Type event names with their length. Separate several with commas. "
                            "Quick-added events only last for the week you are viewing."));
        qg->addWidget(eventQuick);
        qg->addWidget(quickBtn);
        ll->addWidget(quick);

        auto* form = new QGroupBox("Add one event");
        auto* fg = new QGridLayout(form);
        fg->setHorizontalSpacing(16);
        fg->setVerticalSpacing(10);
        eventName = new QLineEdit;
        eventName->setPlaceholderText("e.g. Career talk");
        eventMinutes = new QSpinBox;
        eventMinutes->setRange(5, 1440);
        eventMinutes->setSingleStep(5);
        eventMinutes->setValue(60);
        eventMinutes->setSuffix(" min");
        eventDay = new QComboBox;
        eventDay->addItems({"Any day", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday"});
        eventPlacementBox = new QComboBox;
        eventPlacementBox->addItems({"Automatic placement", "Custom exact time"});
        eventRepeat = new QComboBox;
        eventRepeat->addItems({"Does not repeat (this week only)", "Repeats daily", "Repeats weekly", "Repeats monthly"});
        eventCustomStart = new QTimeEdit(QTime(16, 0));
        eventCustomEnd = new QTimeEdit(QTime(22, 0));
        eventCustomStart->setDisplayFormat("h:mm AP");
        eventCustomEnd->setDisplayFormat("h:mm AP");
        eventLengthLabel = muted("Length");
        eventDayLabel = muted("Day / preferred day");
        fg->addWidget(muted("Event name"), 0, 0, 1, 2);
        fg->addWidget(eventName, 1, 0, 1, 2);
        fg->addWidget(muted("Placement type"), 2, 0, 1, 2);
        fg->addWidget(eventPlacementBox, 3, 0, 1, 2);
        fg->addWidget(eventLengthLabel, 4, 0);
        fg->addWidget(eventDayLabel, 4, 1);
        fg->addWidget(eventMinutes, 5, 0);
        fg->addWidget(eventDay, 5, 1);
        eventCustomStartLabel = muted("Custom start");
        eventCustomEndLabel = muted("Custom end");
        fg->addWidget(eventCustomStartLabel, 6, 0);
        fg->addWidget(eventCustomEndLabel, 6, 1);
        fg->addWidget(eventCustomStart, 7, 0);
        fg->addWidget(eventCustomEnd, 7, 1);
        fg->addWidget(muted("Repeat"), 8, 0, 1, 2);
        fg->addWidget(eventRepeat, 9, 0, 1, 2);
        eventCustomStart->setVisible(false);
        eventCustomEnd->setVisible(false);
        eventCustomStartLabel->setVisible(false);
        eventCustomEndLabel->setVisible(false);
        addEventBtn = new QPushButton("Add event");
        addEventBtn->setObjectName("primary");
        cancelEventEditBtn = new QPushButton("Cancel edit");
        cancelEventEditBtn->setObjectName("quiet");
        cancelEventEditBtn->setVisible(false);
        fg->addWidget(addEventBtn, 10, 0, 1, 2);
        fg->addWidget(cancelEventEditBtn, 11, 0, 1, 2);
        ll->addWidget(form);
        ll->addWidget(muted("Events are added to the week shown above. Without a repeat they appear in that week only. "
                            "University mode uses free time around classes and reports class conflicts. External mode schedules on its own grid without university classes. "
                            "Select an event in the list and press Edit (or double-click it) to change it."));
        ll->addStretch();

        auto* leftScroll = new QScrollArea;
        leftScroll->setWidgetResizable(true);
        leftScroll->setFrameShape(QFrame::NoFrame);
        leftScroll->setMinimumWidth(400);
        leftScroll->setWidget(left);

        auto* right = new QWidget;
        auto* rl = new QVBoxLayout(right);
        rl->setContentsMargins(16, 0, 0, 0);
        rl->setSpacing(16);
        auto* list = panel();
        auto* lpl = new QVBoxLayout(list);
        lpl->setContentsMargins(20, 18, 20, 18);
        lpl->setSpacing(10);
        lpl->addWidget(heading("Event list"));
        lpl->addWidget(muted("Every event you added, and where it lands in the shown week"));
        eventTable = new QTableWidget;
        setupTable(eventTable, {"Event", "Length", "Preferred", "Repeat", "Scheduled (shown week)", "Conflicts"});
        lpl->addWidget(eventTable);
        auto* btnRow = new QHBoxLayout;
        auto* editEvent = new QPushButton("Edit selected");
        editEvent->setObjectName("quiet");
        auto* remove = new QPushButton("Remove selected");
        remove->setObjectName("quiet");
        auto* clearAll = new QPushButton("Clear all");
        clearAll->setObjectName("quiet");
        btnRow->addStretch();
        btnRow->addWidget(clearAll);
        btnRow->addWidget(remove);
        btnRow->addWidget(editEvent);
        lpl->addLayout(btnRow);
        rl->addWidget(list, 1);

        auto* results = panel();
        auto* resl = new QVBoxLayout(results);
        resl->setContentsMargins(20, 18, 20, 18);
        resl->setSpacing(10);
        resl->addWidget(heading("Event schedule"));
        resl->addWidget(muted("Events in red. Dashed outlines indicate a scheduling conflict."));
        auto* eventTabs = new QTabWidget;
        eventOutput = new QTextBrowser;
        eventOutput->setMinimumHeight(240);
        eventOutput->setHtml(placeholderHtml("No events yet", "Add an event and choose automatic placement or an exact day and time."));
        auto* eventGridScroll = new QScrollArea;
        eventGridScroll->setWidgetResizable(false);
        eventGridScroll->setFrameShape(QFrame::NoFrame);
        eventGridLabel = new QLabel;
        eventGridLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        eventGridScroll->setWidget(eventGridLabel);
        eventTabs->addTab(eventGridScroll, "Grid view");
        eventTabs->addTab(eventOutput, "Summary");
        resl->addWidget(eventTabs, 1);
        rl->addWidget(results, 2);

        split->addWidget(leftScroll);
        split->addWidget(right);
        split->setStretchFactor(0, 4);
        split->setStretchFactor(1, 6);
        layout->addWidget(split, 1);

        connect(addEventBtn, &QPushButton::clicked, this, [this] { addEventFromForm(); });
        connect(cancelEventEditBtn, &QPushButton::clicked, this, [this] { cancelEventEdit(); });
        connect(eventName, &QLineEdit::returnPressed, this, [this] { addEventFromForm(); });
        connect(editEvent, &QPushButton::clicked, this, [this] { editSelectedEvent(); });
        connect(eventTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editSelectedEvent(); });
        connect(prevWeek, &QPushButton::clicked, this, [this] { shiftWeek(-1); });
        connect(thisWeek, &QPushButton::clicked, this, [this] { shiftWeek(0); });
        connect(nextWeek, &QPushButton::clicked, this, [this] { shiftWeek(1); });
        connect(eventPlacementBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            const bool custom = index == 1;
            eventMinutes->setVisible(!custom);
            eventLengthLabel->setVisible(!custom);
            eventCustomStart->setVisible(custom);
            eventCustomEnd->setVisible(custom);
            eventCustomStartLabel->setVisible(custom);
            eventCustomEndLabel->setVisible(custom);
            eventDayLabel->setText(custom ? "Exact day" : "Preferred day");
            if (custom && eventDay->currentIndex() == 0) eventDay->setCurrentIndex(1);
        });
        connect(quickBtn, &QPushButton::clicked, this, [this] { addEventsFromText(); });
        connect(eventQuick, &QLineEdit::returnPressed, this, [this] { addEventsFromText(); });
        connect(remove, &QPushButton::clicked, this, [this] { removeSelectedEvent(); });
        connect(clearAll, &QPushButton::clicked, this, [this] {
            cancelEventEdit();
            activeEvents().clear();
            refreshAfterEventChange();
        });
        connect(refit, &QPushButton::clicked, this, [this] { refreshAfterEventChange(); });
        connect(eventModeBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) { switchEventMode(i); });
        auto refreshExternal = [this] { if (externalMode) refreshAfterEventChange(); };
        connect(externalDayStart, &QTimeEdit::timeChanged, this, [refreshExternal](const QTime&) { refreshExternal(); });
        connect(externalDayEnd, &QTimeEdit::timeChanged, this, [refreshExternal](const QTime&) { refreshExternal(); });
        connect(externalSlotMinutes, qOverload<int>(&QSpinBox::valueChanged), this, [refreshExternal](int) { refreshExternal(); });
        connect(externalBreakMinutes, qOverload<int>(&QSpinBox::valueChanged), this, [refreshExternal](int) { refreshExternal(); });
        for (int d = 0; d < 7; ++d)
            connect(externalDayChecks[d], &QCheckBox::toggled, this, [refreshExternal](bool) { refreshExternal(); });
        return page;
    }

    // ---- events: add / edit / remove -------------------------------------
    void cancelEventEdit() {
        editingEventIndex = -1;
        if (addEventBtn) addEventBtn->setText("Add event");
        if (cancelEventEditBtn) cancelEventEditBtn->setVisible(false);
        if (eventName) eventName->clear();
        if (eventRepeat) eventRepeat->setCurrentIndex(0);
    }

    void editSelectedEvent() {
        const int row = eventTable->currentRow();
        if (row < 0 || row >= int(activeEvents().size())) {
            QMessageBox::information(this, "Select an event", "Select an event in the list first.");
            return;
        }
        const AutoEvent ev = activeEvents()[static_cast<std::size_t>(row)];
        editingEventIndex = row;
        eventName->setText(qstr(ev.name));
        eventPlacementBox->setCurrentIndex(ev.customTime ? 1 : 0);
        if (ev.customTime) {
            eventCustomStart->setTime(QTime(std::min(ev.customStartMin, 1439) / 60, std::min(ev.customStartMin, 1439) % 60));
            eventCustomEnd->setTime(QTime(std::min(ev.customEndMin, 1439) / 60, std::min(ev.customEndMin, 1439) % 60));
        } else {
            eventMinutes->setValue(ev.minutes);
        }
        eventDay->setCurrentIndex(std::clamp(ev.preferredDay + 1, 0, eventDay->count() - 1));
        eventRepeat->setCurrentIndex(std::clamp(ev.repeat, 0, eventRepeat->count() - 1));
        addEventBtn->setText("Save changes");
        cancelEventEditBtn->setVisible(true);
        statusLabel->setText("Editing " + qstr(ev.name) + ". Change the details and press Save changes.");
        eventName->setFocus();
    }

    void addEventFromForm() {
        const QString name = eventName->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, "Missing event name", "Enter a name for the event.");
            return;
        }
        AutoEvent ev;
        ev.name = name.toStdString();
        ev.preferredDay = eventDay->currentIndex() - 1;
        ev.customTime = eventPlacementBox->currentIndex() == 1;
        ev.repeat = eventRepeat->currentIndex();
        if (ev.customTime) {
            if (ev.preferredDay < 0) {
                QMessageBox::warning(this, "Choose a day", "Custom events need an exact day. Select a day instead of Any day.");
                return;
            }
            ev.customStartMin = eventCustomStart->time().hour() * 60 + eventCustomStart->time().minute();
            ev.customEndMin = eventCustomEnd->time().hour() * 60 + eventCustomEnd->time().minute();
            if (ev.customEndMin <= ev.customStartMin) {
                QMessageBox::warning(this, "Invalid time range", "The event end time must be later than its start time.");
                return;
            }
            ev.minutes = ev.customEndMin - ev.customStartMin;
        } else {
            ev.minutes = eventMinutes->value();
        }
        if (editingEventIndex >= 0 && editingEventIndex < int(activeEvents().size())) {
            // Keep the week the event was created in; only its day inside that week follows the form.
            const long long oldWeek = mondayOfJd(activeEvents()[static_cast<std::size_t>(editingEventIndex)].anchorJd);
            ev.anchorJd = oldWeek + std::max(0, ev.preferredDay);
            activeEvents()[static_cast<std::size_t>(editingEventIndex)] = ev;
            cancelEventEdit();
            refreshAfterEventChange();
            statusLabel->setText("Event updated.");
            return;
        }
        ev.anchorJd = viewMonday + std::max(0, ev.preferredDay);
        activeEvents().push_back(ev);
        eventName->clear();
        eventRepeat->setCurrentIndex(0);
        eventName->setFocus();
        refreshAfterEventChange();
    }

    void addEventsFromText() {
        const QString text = eventQuick->text();
        const QStringList chunks = text.split(QRegularExpression("[,;\\n]"), Qt::SkipEmptyParts);
        int added = 0;
        QStringList bad;
        for (const QString& chunk : chunks) {
            QString name;
            int minutes = 0;
            if (parseEventText(chunk, name, minutes)) {
                AutoEvent ev;
                ev.name = name.toStdString();
                ev.minutes = minutes;
                ev.repeat = 0;                    // quick add: this week only
                ev.anchorJd = viewMonday;
                activeEvents().push_back(ev);
                ++added;
            } else if (!chunk.trimmed().isEmpty()) {
                bad << chunk.trimmed();
            }
        }
        if (!bad.isEmpty())
            QMessageBox::information(this, "Some entries were skipped",
                                     "Could not read a name and length from:\n  " + bad.join("\n  ") +
                                         "\n\nTry something like \"Workshop 90\" or \"Seminar 1h30\".");
        if (added > 0) eventQuick->clear();
        refreshAfterEventChange();
    }

    void removeSelectedEvent() {
        const int row = eventTable->currentRow();
        if (row < 0 || row >= int(activeEvents().size())) return;
        if (editingEventIndex == row) cancelEventEdit();
        else if (editingEventIndex > row) --editingEventIndex;
        activeEvents().erase(activeEvents().begin() + row);
        refreshAfterEventChange();
    }

    void runUniversityEventPlacement() {
        const AutoPrefs p = hasTimetable ? lastPrefs : buildPrefs();
        buildInstances(universityEvents, viewMonday, 5, p, false, uniInst, uniSrc);
        const std::vector<AutoSession> sessions = hasTimetable ? timetableResult.sessions : std::vector<AutoSession>();
        universityEventResult = AutoScheduler::placeEvents(uniInst, sessions, p);
    }

    void runExternalEventPlacement() {
        const AutoPrefs p = buildExternalPrefs();
        buildInstances(externalEvents, viewMonday, 7, p, false, extInst, extSrc);
        if (p.dayEndMin <= p.dayStartMin) {
            externalEventResult = AutoEventResult{};
            externalEventResult.ok = false;
            externalEventResult.message = "The external grid must end after it starts.";
            externalEventResult.placements.resize(extInst.size());
        } else {
            externalEventResult = AutoScheduler::placeEvents(extInst, {}, p);
        }
    }

    void runEventPlacement() {
        if (externalMode) runExternalEventPlacement();
        else runUniversityEventPlacement();
    }

    void switchEventMode(int index) {
        const bool nextExternal = (index == 1);
        if (externalMode == nextExternal) return;
        cancelEventEdit();
        externalMode = nextExternal;
        externalSettings->setVisible(externalMode);
        eventDay->clear();
        eventDay->addItems(externalMode
            ? QStringList{"Any day", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"}
            : QStringList{"Any day", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday"});
        refreshAfterEventChange();
        statusLabel->setText(externalMode ? "External event workspace active." : "University event workspace active.");
    }

    void refreshAfterEventChange() {
        runEventPlacement();
        refreshEventTable();
        renderEventSummary();
        renderTimetable();
        renderGrids();
    }

    void refreshEventTable() {
        if (!eventTable) return;
        eventTable->setRowCount(0);
        const auto& defs = activeEvents();
        const auto& inst = activeInst();
        const auto& src = activeSrc();
        const auto& res = activeEventResult();

        int placed = 0;
        for (const AutoEventPlacement& ep : res.placements)
            if (ep.placed) ++placed;

        for (std::size_t i = 0; i < defs.size(); ++i) {
            const int r = eventTable->rowCount();
            eventTable->insertRow(r);

            int first = -1, count = 0;
            bool anyClash = false;
            for (std::size_t k = 0; k < inst.size() && k < src.size(); ++k) {
                if (src[k] != int(i)) continue;
                if (first < 0) first = int(k);
                ++count;
                if (k < res.placements.size() && !res.placements[k].clashes.empty()) anyClash = true;
            }

            QString when = count == 0 ? "Not in the shown week" : "Not placed";
            QString affects = "-";
            bool overlaps = false;
            if (first >= 0 && first < int(res.placements.size()) && res.placements[static_cast<std::size_t>(first)].placed) {
                const AutoEventPlacement& ep = res.placements[static_cast<std::size_t>(first)];
                when = shortDay(ep.day) + "  " + timeText(ep.startMin) + " - " + timeText(ep.endMin);
                if (count > 1) when += QString("  (+%1 more)").arg(count - 1);
                if (anyClash) {
                    for (std::size_t k = 0; k < inst.size() && k < src.size(); ++k) {
                        if (src[k] != int(i) || k >= res.placements.size() || res.placements[k].clashes.empty()) continue;
                        affects = clashText(res.placements[k], true, externalMode);
                        break;
                    }
                    overlaps = true;
                }
            }
            const int pd = defs[i].preferredDay;
            const QString length = defs[i].customTime
                ? "Custom · " + durationText(defs[i].minutes)
                : durationText(defs[i].minutes);
            const QStringList vals = {qstr(defs[i].name), length,
                                      pd >= 0 ? dayName(pd) : "Any day", repeatText(defs[i].repeat), when, affects};
            for (int c = 0; c < vals.size(); ++c) {
                auto* item = new QTableWidgetItem(vals[c]);
                if (overlaps && c >= 4) item->setForeground(ACCENT);
                eventTable->setItem(r, c, item);
            }
        }
        eventCount->setText(QString::number(defs.size()));
        eventPlaced->setText(QString::number(placed));
        eventOverlap->setText(QString::number(res.clashingEvents));
    }

    void renderEventSummary() {
        if (activeEvents().empty()) {
            eventOutput->setHtml(placeholderHtml("No events yet", "Add an event and choose automatic placement or an exact day and time."));
            statusLabel->setText("No events to schedule.");
            return;
        }
        if (activeInst().empty()) {
            eventOutput->setHtml(placeholderHtml("Nothing in this week",
                                                 "None of your events fall in " + weekLabel->text().toHtmlEscaped() +
                                                 ". Use the week buttons to look at another week."));
            statusLabel->setText("No events in the shown week.");
            return;
        }
        const auto& inst = activeInst();
        const auto& src = activeSrc();
        const auto& defs = activeEvents();
        const bool anyOverlap = activeEventResult().clashingEvents > 0;
        QString html = "<div style='color:#000000'>";
        html += "<div style='color:#525252;font-weight:600'>" + weekLabel->text().toHtmlEscaped() + "</div>";
        html += QString("<div style='font-weight:600;color:%1'>%2</div>")
                    .arg(anyOverlap || !activeEventResult().ok ? QString("#DC2626") : QString("#525252"),
                         qstr(activeEventResult().message).toHtmlEscaped());
        html += "<table width='100%' cellspacing='0' cellpadding='8' style='margin-top:8px'>"
                "<tr bgcolor='#000000'><th align='left' style='color:#FFFFFF'>Event</th>"
                "<th align='left' style='color:#FFFFFF'>Repeat</th>"
                "<th align='left' style='color:#FFFFFF'>Day</th><th align='left' style='color:#FFFFFF'>Time</th>"
                "<th align='left' style='color:#FFFFFF'>Conflicts / affected items</th></tr>";
        for (std::size_t i = 0; i < inst.size() && i < activeEventResult().placements.size(); ++i) {
            const AutoEventPlacement& ep = activeEventResult().placements[i];
            const QString shade = (i % 2) ? "#F5F5F5" : "#FFFFFF";
            const QString name = qstr(inst[i].name).toHtmlEscaped();
            const int srcIdx = (i < src.size()) ? src[i] : -1;
            const QString rep = (srcIdx >= 0 && srcIdx < int(defs.size())) ? repeatText(defs[static_cast<std::size_t>(srcIdx)].repeat) : QString("-");
            if (!ep.placed) {
                html += "<tr bgcolor='" + shade + "'><td><b>" + name + "</b></td><td>" + rep +
                        "</td><td colspan='3' style='color:#DC2626'>Could not be placed</td></tr>";
                continue;
            }
            QString note = "<span style='color:#525252'>None, placed in free time</span>";
            if (!ep.clashes.empty())
                note = "<b style='color:#DC2626'>" + clashText(ep, true, externalMode).toHtmlEscaped() + "</b>";
            else if (ep.forced)
                note = "<span style='color:#525252'>None, but with less than a full break</span>";
            if (ep.shortened)
                note += "<br><span style='color:#DC2626'>Longer than the teaching day, shown cut to fit</span>";
            html += "<tr bgcolor='" + shade + "'><td><b>" + name + "</b></td><td>" + rep + "</td><td>" + dayName(ep.day) +
                    "</td><td>" + timeText(ep.startMin) + " - " + timeText(ep.endMin) + "</td><td>" + note + "</td></tr>";
        }
        html += "</table>";
        if (externalMode)
            html += "<p style='color:#525252'>External events use their own weekly grid and independent time settings. University classes are not included.</p>";
        else
            html += hasTimetable ? "<p style='color:#525252'>Events are fitted around your saved generated timetable and shown on its original grid.</p>"
                                 : "<p style='color:#525252'>No timetable yet, so university events use the empty university week.</p>";
        html += "</div>";
        eventOutput->setHtml(html);
        statusLabel->setText(qstr(activeEventResult().message));
    }

    struct Row {
        QString type, code, title;
        int day, start, end, slotUnits;
        QString note;
    };

    std::vector<Row> exportRows(bool eventMode) const {
        std::vector<Row> rows;
        if (hasTimetable && !eventMode)
            for (const AutoSession& s : timetableResult.sessions) {
                const AutoCourse& c = courses[static_cast<std::size_t>(s.courseIndex)];
                rows.push_back({"Class", qstr(c.code), qstr(c.name), s.day, s.startMin, s.endMin, s.slotCount, QString()});
            }
        const auto& exportEvents = eventMode ? activeInst() : uniInst;
        const auto& exportResult = eventMode ? activeEventResult() : universityEventResult;
        for (const AutoEventPlacement& ep : exportResult.placements)
            if (ep.placed && ep.eventIndex < int(exportEvents.size())) {
                const QString note = ep.clashes.empty() ? QString() : "Overlaps " + clashText(ep, true, eventMode && externalMode);
                rows.push_back({"Event", "", qstr(exportEvents[static_cast<std::size_t>(ep.eventIndex)].name),
                                ep.day, ep.startMin, ep.endMin, 0, note});
            }
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
            return a.day != b.day ? a.day < b.day : a.start < b.start;
        });
        return rows;
    }

    QString askPath(const QString& defaultName, const QString& filter, const QString& suffix) {
        QString path = QFileDialog::getSaveFileName(this, "Export", QDir::homePath() + "/" + defaultName, filter);
        if (path.isEmpty()) return QString();
        if (!path.endsWith("." + suffix, Qt::CaseInsensitive)) path += "." + suffix;
        return path;
    }

    bool writeText(const QString& path, const QString& text) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        f.write(text.toUtf8());
        return true;
    }

    void exportAs(const QString& kind) {
        const bool eventMode = tabs->currentIndex() == 2;
        const std::vector<Row> rows = exportRows(eventMode);
        if (rows.empty()) {
            QMessageBox::information(this, "Nothing to export",
                                     eventMode ? "Add and schedule an event first."
                                               : "Generate a timetable or add events first.");
            return;
        }
        const QString stem = eventMode ? "events" : "timetable";
        bool ok = false;
        QString path;

        if (kind == "pdf") {
            path = askPath(stem + ".pdf", "PDF document (*.pdf)", "pdf");
            if (path.isEmpty()) return;
            ok = exportPdf(path, rows, eventMode);
        } else if (kind == "jpg") {
            path = askPath(stem + ".jpg", "JPEG image (*.jpg)", "jpg");
            if (path.isEmpty()) return;
            QImage img(3300, 2300, QImage::Format_RGB32);
            img.fill(Qt::white);
            {
                QPainter p(&img);
                p.setRenderHint(QPainter::Antialiasing);
                p.setRenderHint(QPainter::TextAntialiasing);
                composeSheet(p, QRectF(100, 90, img.width() - 200, img.height() - 180), eventMode);
            }
            ok = img.save(path, "JPG", 95);
        } else if (kind == "csv") {
            path = askPath(stem + ".csv", "CSV spreadsheet (*.csv)", "csv");
            if (path.isEmpty()) return;
            QString out = "Type,Day,Start,End,Duration (min),Code,Title,Slots,Notes\r\n";
            for (const Row& r : rows)
                out += csvCell(r.type) + "," + csvCell(dayName(r.day)) + "," + csvCell(timeText(r.start)) + "," +
                       csvCell(timeText(r.end)) + "," + QString::number(r.end - r.start) + "," + csvCell(r.code) + "," +
                       csvCell(r.title) + "," + (r.slotUnits ? QString::number(r.slotUnits) : QString()) + "," +
                       csvCell(r.note) + "\r\n";
            ok = writeText(path, QString(QChar(0xFEFF)) + out);
        } else if (kind == "json") {
            path = askPath(stem + ".json", "JSON data (*.json)", "json");
            if (path.isEmpty()) return;
            const AutoPrefs pr = eventMode ? activePrefs() : (hasTimetable ? lastPrefs : buildPrefs());
            QJsonObject root;
            QJsonObject cfg;
            cfg["dayStart"] = timeText(pr.dayStartMin);
            cfg["dayEnd"] = timeText(pr.dayEndMin);
            cfg["slotMinutes"] = pr.slotMin;
            cfg["slotBreakMinutes"] = pr.slotBreakMin;
            cfg["slotsPerDay"] = AutoScheduler::slotsPerDay(pr);
            root["settings"] = cfg;
            QJsonArray arr;
            for (const Row& r : rows) {
                QJsonObject o;
                o["type"] = r.type;
                o["day"] = dayName(r.day);
                o["start"] = timeText(r.start);
                o["end"] = timeText(r.end);
                o["durationMinutes"] = r.end - r.start;
                o["code"] = r.code;
                o["title"] = r.title;
                if (r.slotUnits) o["slots"] = r.slotUnits;
                if (!r.note.isEmpty()) o["overlaps"] = r.note;
                arr.append(o);
            }
            root["entries"] = arr;
            ok = writeText(path, QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented)));
        } else if (kind == "ics") {
            path = askPath(stem + ".ics", "Calendar file (*.ics)", "ics");
            if (path.isEmpty()) return;
            QString out = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//Smart Scheduler//EN\r\nCALSCALE:GREGORIAN\r\n";
            // Use the week shown in the event planner for events, the current week for classes.
            const QDate eventMonday = QDate::fromJulianDay(viewMonday);
            const QDate classMonday = QDate::fromJulianDay(mondayOfJd(QDate::currentDate().toJulianDay()));
            auto esc = [](QString s) {
                s.replace("\\", "\\\\").replace(";", "\\;").replace(",", "\\,").replace("\n", "\\n");
                return s;
            };
            for (const Row& r : rows) {
                const QDate date = (r.type == "Class" ? classMonday : eventMonday).addDays(r.day);
                auto stamp = [&](int mins) {
                    return date.toString("yyyyMMdd") + QString("T%1%2").arg(mins / 60, 2, 10, QChar('0')).arg(mins % 60, 2, 10, QChar('0')) + "00";
                };
                out += "BEGIN:VEVENT\r\nUID:" + QUuid::createUuid().toString(QUuid::WithoutBraces) + "@smart-scheduler\r\n";
                out += "DTSTAMP:" + QDate::currentDate().toString("yyyyMMdd") + "T000000Z\r\n";
                out += "DTSTART:" + stamp(r.start) + "\r\nDTEND:" + stamp(r.end) + "\r\n";
                out += "SUMMARY:" + esc(r.code.isEmpty() ? r.title : r.code + " " + r.title) + "\r\n";
                if (!r.note.isEmpty()) out += "DESCRIPTION:" + esc(r.note) + "\r\n";
                if (r.type == "Class") out += "RRULE:FREQ=WEEKLY;COUNT=16\r\n";
                out += "END:VEVENT\r\n";
            }
            out += "END:VCALENDAR\r\n";
            ok = writeText(path, out);
        }

        if (ok) statusLabel->setText("Exported to " + path);
        else QMessageBox::warning(this, "Export failed", "The file could not be written. Check the location and try again.");
    }

    bool exportPdf(const QString& path, const std::vector<Row>& rows, bool eventMode) {
        QPdfWriter w(path);
        w.setPageSize(QPageSize(QPageSize::A4));
        w.setPageOrientation(QPageLayout::Landscape);
        w.setResolution(150);
        w.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);
        w.setTitle(eventMode ? "Event Schedule" : "Weekly Timetable");
        w.setCreator("Smart Scheduler Suite");

        QPainter p(&w);
        if (!p.isActive()) return false;
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        const QRectF page(0, 0, w.width(), w.height());
        composeSheet(p, page, eventMode);

        w.newPage();
        const double u = page.width() / 100.0;
        const double rowH = u * 3.4, x0 = page.left(), tw = page.width(), pad = u * 0.9;
        const QStringList heads = {"Day", "Time", "Type", "Code", "Title", "Notes"};
        const double cut[7] = {0.0, 0.12, 0.27, 0.37, 0.47, 0.70, 1.0};

        QFont tf = baseFont();
        tf.setBold(true);
        tf.setPixelSize(int(u * 2.4));
        p.setFont(tf);
        p.setPen(INK);
        p.drawText(QRectF(x0, page.top(), tw, u * 3.4), Qt::AlignLeft | Qt::AlignVCenter, "Schedule details");

        QFont f = baseFont();
        f.setPixelSize(int(u * 1.35));
        QFont hf = f;
        hf.setBold(true);

        auto cell = [&](double y, int c, const QString& text) {
            const QRectF r(x0 + tw * cut[c] + pad, y, tw * (cut[c + 1] - cut[c]) - 2 * pad, rowH);
            p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(text, Qt::ElideRight, int(r.width())));
        };
        auto header = [&](double y) {
            p.setPen(Qt::NoPen);
            p.setBrush(INK);
            p.drawRect(QRectF(x0, y, tw, rowH));
            p.setFont(hf);
            p.setPen(PAPER);
            for (int c = 0; c < 6; ++c) cell(y, c, heads[c]);
        };

        double y = page.top() + u * 4.4;
        header(y);
        y += rowH;
        int n = 0;
        for (const Row& r : rows) {
            if (y + rowH > page.bottom()) {
                w.newPage();
                y = page.top();
                header(y);
                y += rowH;
                n = 0;
            }
            p.setPen(Qt::NoPen);
            p.setBrush(n % 2 ? WASH : PAPER);
            p.drawRect(QRectF(x0, y, tw, rowH));
            p.setPen(QPen(HAIRLINE, 1));
            p.drawLine(QPointF(x0, y + rowH), QPointF(x0 + tw, y + rowH));
            p.setFont(f);
            p.setPen(INK);
            cell(y, 0, dayName(r.day));
            cell(y, 1, timeText(r.start) + " - " + timeText(r.end));
            p.setPen(r.type == "Event" ? ACCENT : INK);
            cell(y, 2, r.type);
            p.setPen(INK);
            cell(y, 3, r.code);
            cell(y, 4, r.title);
            p.setPen(r.note.isEmpty() ? INK : ACCENT);
            cell(y, 5, r.note);
            y += rowH;
            ++n;
        }
        p.setPen(QPen(INK, 2));
        p.drawLine(QPointF(x0, y), QPointF(x0 + tw, y));
        p.end();
        return true;
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Smart Scheduler Suite");
    app.setOrganizationName("Smart Scheduler");
    app.setApplicationDisplayName("Smart Scheduler Suite");

    // Force a light, consistent look so dialogs, message boxes and the date
    // popup stay readable even when the operating system uses a dark theme.
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette light;
    light.setColor(QPalette::Window, QColor("#FFFFFF"));
    light.setColor(QPalette::WindowText, QColor("#000000"));
    light.setColor(QPalette::Base, QColor("#FFFFFF"));
    light.setColor(QPalette::AlternateBase, QColor("#F5F5F5"));
    light.setColor(QPalette::Text, QColor("#000000"));
    light.setColor(QPalette::Button, QColor("#FFFFFF"));
    light.setColor(QPalette::ButtonText, QColor("#000000"));
    light.setColor(QPalette::BrightText, QColor("#DC2626"));
    light.setColor(QPalette::Highlight, QColor("#000000"));
    light.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
    light.setColor(QPalette::ToolTipBase, QColor("#000000"));
    light.setColor(QPalette::ToolTipText, QColor("#FFFFFF"));
    light.setColor(QPalette::PlaceholderText, QColor("#737373"));
    light.setColor(QPalette::Link, QColor("#DC2626"));
    app.setPalette(light);

    // Load application icon from embedded Qt Resource
    const QIcon appIcon(QStringLiteral(":/assets/favicon.ico"));
    app.setWindowIcon(appIcon);

    // Load and display Splash Screen with logo.png
    QPixmap splashPixmap(QStringLiteral(":/assets/logo.png"));
    if (!splashPixmap.isNull()) {
        splashPixmap = splashPixmap.scaled(
            QSize(620, 360),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );

        QSplashScreen splash(splashPixmap, Qt::WindowStaysOnTopHint);
        splash.setWindowIcon(appIcon);
        splash.show();
        splash.showMessage(
            QStringLiteral("LOADING SMART SCHEDULER SUITE  •  Preparing workspace..."),
            Qt::AlignHCenter | Qt::AlignBottom,
            QColor("#FFFFFF")
        );
        app.processEvents();

        MainWindow window;
        window.setWindowIcon(appIcon);
        window.show();
        splash.finish(&window);
        return app.exec();
    }

    // Fallback if splash image is not present
    MainWindow window;
    window.setWindowIcon(appIcon);
    window.show();
    return app.exec();
}