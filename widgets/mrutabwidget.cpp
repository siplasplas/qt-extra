#include "mrutabwidget.h"
#include <QTabWidget>
#include <QTabBar>
#include <QEvent>
#include <QHoverEvent>
#include <QDialog>
#include <QListWidget>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QPointer>
#include <QTimer>
#include <QSet>
#include <QMetaMethod>
#include <cmath>
#include <QToolButton>
#include <QDebug>
#include <QMenu>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionTab>
#include <algorithm>

#include "Ev.h"

constexpr int CTRL_TAB_TIMEOUT_MS = 200;

namespace {

// Draws the preview tab's label in italics. QTabBar has no per-tab font and the style
// option does not carry the tab index, so the tab is recognized by its rectangle, or by
// its text while it is being dragged. When neither matches, the label is drawn upright.
class PreviewTabStyle : public QProxyStyle
{
public:
    PreviewTabStyle(QStyle *base, MruTabWidget *owner) : QProxyStyle(base), m_owner(owner) {}

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget) const override
    {
        const auto *tab = qstyleoption_cast<const QStyleOptionTab*>(option);
        if (element == CE_TabBarTabLabel && tab && isPreviewTab(*tab, widget)) {
            painter->save();
            QFont font = painter->font();
            font.setItalic(true);
            painter->setFont(font);
            QProxyStyle::drawControl(element, option, painter, widget);
            painter->restore();
            return;
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }

private:
    bool isPreviewTab(const QStyleOptionTab &tab, const QWidget *widget) const
    {
        if (!m_owner || !m_owner->previewTab() || widget != m_owner->tabBar())
            return false;
        const QTabBar *bar = m_owner->tabBar();
        const int previewIndex = m_owner->indexOf(m_owner->previewTab());
        if (previewIndex < 0)
            return false;
        if (bar->tabRect(previewIndex) == tab.rect)
            return true;
        for (int i = 0; i < bar->count(); ++i) {
            if (bar->tabRect(i) == tab.rect)
                return false;
        }
        return bar->tabText(previewIndex) == tab.text
               && bar->tabRect(previewIndex).size() == tab.rect.size();
    }

    QPointer<MruTabWidget> m_owner;
};

// Busy spinner or attention dot shown as a tab button.
class TabMarker : public QWidget
{
public:
    explicit TabMarker(QWidget *parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("MruTabMarker"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFixedSize(12, 12);
        m_timer.setInterval(80);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_angle = (m_angle + 30) % 360;
            update();
        });
    }

    void setState(bool busy, bool attention)
    {
        m_busy = busy;
        m_attention = attention;
        if (busy)
            m_timer.start();
        else
            m_timer.stop();
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor color = palette().color(m_attention ? QPalette::Highlight : QPalette::WindowText);
        const QRectF r = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        if (m_busy) {
            p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap));
            p.drawArc(r, -m_angle * 16, 270 * 16);
        } else if (m_attention) {
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawEllipse(r.adjusted(1.5, 1.5, -1.5, -1.5));
        }
    }

private:
    QTimer m_timer;
    int m_angle = 0;
    bool m_busy = false;
    bool m_attention = false;
};

// Built-in pushpin used when setPinIconUri() was not called.
QIcon paintedPinIcon(const QColor &color)
{
    QIcon icon;
    for (int size : {16, 32, 48}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(size / 16.0, size / 16.0);
        p.translate(8, 8);
        p.rotate(45);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(-3, -7, 6, 2), 0.8, 0.8);  // cap
        p.drawRect(QRectF(-2, -5.5, 4, 5.5));                // body
        p.drawRoundedRect(QRectF(-4.5, 0, 9, 1.8), 0.6, 0.6); // collar
        p.setPen(QPen(color, 1.2, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(0, 1.8), QPointF(0, 7.5));        // needle
        p.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

/**
 * @brief Constructs MruTabWidget with parent
 * @param parent Parent widget
 */
MruTabWidget::MruTabWidget(QWidget *parent)
    : QTabWidget(parent)
{
    connect(this, &QTabWidget::currentChanged, this, &MruTabWidget::onCurrentChanged);
    m_ctrlTabTimer.setSingleShot(true);
    m_ctrlTabTimer.setInterval(CTRL_TAB_TIMEOUT_MS);
    connect(&m_ctrlTabTimer, &QTimer::timeout, this, &MruTabWidget::handleCtrlTabTimeout);

    assert(tabBar());
    tabBar()->setMouseTracking(true);
    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabBar(), &QTabBar::customContextMenuRequested, this, &MruTabWidget::onTabContextMenuRequested);
    connect(tabBar(), &QTabBar::tabMoved, this, [this]() {
        mapCloseButtonsToTabs();
        updateCloseButtonVisibility();
    });
    connect(tabBar(), &QTabBar::tabBarDoubleClicked, this, [this](int index) {
        if (index >= 0 && widget(index) == m_previewPage)
            promotePreviewTab();
    });

    connect(this, &QTabWidget::tabCloseRequested, [this](int index) {
        requestCloseTab(index);
    });
}


bool MruTabWidget::requestCloseTab(int index, bool askPin)
{
    if (index < 0 || index >= count())
        return false;
    return requestCloseTab(widget(index), askPin);
}

bool MruTabWidget::requestCloseTab(QWidget *page, bool askPin)
{
    if (!page || indexOf(page) < 0)
        return false;

    // Check if we're at minimal tab count
    if (!canCloseTabs()) {
        return false;
    }

    // A question about this page is already open (in a nested event loop); don't ask twice
    if (m_closeQuestions.contains(page))
        return false;

    QPointer<QWidget> guard(page);
    bool allowClose = true;
    m_closeQuestions.insert(page);
    emit tabAboutToClose(page, askPin, allowClose);
    m_closeQuestions.remove(page);
    if (!allowClose || !guard) return false;
    emit tabClosing(page);
    if (!guard) return true;
    const int index = indexOf(page);
    if (index >= 0)
        removeTab(index);
    emit tabClosed(page);
    if (guard && m_deletePagesOnClose)
        page->deleteLater();
    return true;
}

void MruTabWidget::closePages(const QList<QWidget*> &pages)
{
    for (const QPointer<QWidget> &page : QList<QPointer<QWidget>>(pages.begin(), pages.end())) {
        if (page)
            requestCloseTab(page.data());
    }
}


/**
 * @brief Destructor cleans up resources
 */
MruTabWidget::~MruTabWidget()
{
    hideMruPopup();
    removeTabBarEventFilter();
}

void MruTabWidget::setTabLimit(int limit)
{
    // Ensure tab limit is at least minimalTabCount
    m_tabLimit = (m_minimalTabCount > 0) ? qMax(limit, m_minimalTabCount) : limit;
    enforceTabLimit();
}

void MruTabWidget::setMinimalTabCount(int minCount)
{
    m_minimalTabCount = qMax(0, minCount);
    // Update tab limit if it's now below minimal
    if (m_tabLimit > 0 && m_tabLimit < m_minimalTabCount) {
        m_tabLimit = m_minimalTabCount;
    }
}

bool MruTabWidget::canCloseTabs() const
{
    return count() > m_minimalTabCount;
}


/**
 * @brief Handles tab bar event filter installation
 */
void MruTabWidget::installTabBarEventFilter() {
    if (tabBar() && !m_isTabBarFilterInstalled) {
        tabBar()->installEventFilter(this);
        m_isTabBarFilterInstalled = true;
        // Initial mapping and visibility update
        mapCloseButtonsToTabs();
        updateCloseButtonVisibility();
    } else if (!tabBar()) {
         qWarning() << "[installTabBarEventFilter] Attempted to install filter, but tabBar is NULL!"; // Debug Hover
    } else {
    }
}

/**
 * @brief Handles tab bar event filter removal
 */
void MruTabWidget::removeTabBarEventFilter() {
     if (tabBar() && m_isTabBarFilterInstalled) {
        tabBar()->removeEventFilter(this);
        m_isTabBarFilterInstalled = false;
    }
}

/**
 * @brief Handles widget show event
 * @param event Show event
 */
void MruTabWidget::showEvent(QShowEvent *event) {
    QTabWidget::showEvent(event);
    installTabBarEventFilter(); // Install filter when widget becomes visible
}

/**
 * @brief Handles widget hide event
 * @param event Hide event
 */
void MruTabWidget::hideEvent(QHideEvent *event) {
    removeTabBarEventFilter(); // Remove filter when widget is hidden
    QTabWidget::hideEvent(event);
}

// --- Event Handlers ---

// Override resizeEvent to update button mapping on resize
void MruTabWidget::resizeEvent(QResizeEvent *event) {
    QTabWidget::resizeEvent(event); // Call base implementation first

    // Remap buttons and update their visibility after the widget has been resized
    // Using QTimer::singleShot ensures the layout is stable
    QTimer::singleShot(0, this, [this]() {
        mapCloseButtonsToTabs();
        updateCloseButtonVisibility();
    });
}

/**
 * @brief Handles key press events
 * @param event Key event
 *
 * Implements:
 * - Ctrl+Tab/Shift+Ctrl+Tab navigation
 * - MRU popup triggering
 */
void MruTabWidget::keyPressEvent(QKeyEvent *event)
{
    // Handle Ctrl+Tab / Ctrl+Shift+Tab
    if (event->modifiers() & Qt::ControlModifier)
    {
        if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab)
        {
            if (count() < 2) { // Ignore if less than 2 tabs
                QTabWidget::keyPressEvent(event);
                return;
            }

            // Sequential mode: simple next/prev tab cycling
            if (m_sequentialTabSwitching) {
                bool forward = (event->key() == Qt::Key_Tab) && !(event->modifiers() & Qt::ShiftModifier);
                int newIndex = currentIndex() + (forward ? 1 : -1);
                if (newIndex >= count())
                    newIndex = 0;
                else if (newIndex < 0)
                    newIndex = count() - 1;
                setCurrentIndex(newIndex);
                event->accept();
                return;
            }

            // MRU mode with popup
            // If Ctrl was *not* pressed before -> start timer
            if (!m_ctrlHeld) {
                m_ctrlHeld = true;
                m_expectingPopup = true; // Expecting a potential popup
                m_shiftHeldOnTabPress = (event->modifiers() & Qt::ShiftModifier); // Remember Shift state
                m_ctrlTabTimer.start(); // Start timer
            }
             // If Ctrl *was* already pressed OR popup is visible
            else {
                 // If timer was active (quick second press) -> show popup
                 if (m_ctrlTabTimer.isActive()) {
                     m_ctrlTabTimer.stop(); // Stop timer
                     m_expectingPopup = false;
                     if (!m_mruPopup) { // Show popup if not already there
                          showMruPopup();
                     }
                 }
                 // If popup *is* visible -> do nothing here, eventFilter will handle it
                 // If popup is not visible and we are not expecting it -> show as fallback?
                 else if (!m_expectingPopup && !m_mruPopup) {
                     showMruPopup();
                 }
            }

            event->accept(); // Event handled
            return;
        }
    }
    // Pass other key press events to the base class
    QTabWidget::keyPressEvent(event);
}

/**
 * @brief Handles key release events
 * @param event Key event
 *
 * Implements:
 * - Ctrl key release handling
 * - MRU popup confirmation
 */
void MruTabWidget::keyReleaseEvent(QKeyEvent *event)
{
    // Handle Ctrl key release
    if (event->key() == Qt::Key_Control) {
        bool wasCtrlHeld = m_ctrlHeld; // Store state before reset
        m_ctrlHeld = false;            // Reset state immediately
        m_expectingPopup = false;     // Reset this too

        // If timer was active (short press/release) -> perform direct switch
        if (m_ctrlTabTimer.isActive()) {
            m_ctrlTabTimer.stop();
            performDirectSwitch(); // Switch to previous tab
            event->accept();
        }
        // If popup was visible -> activate selected tab and hide popup
        else if (wasCtrlHeld && m_mruPopup) {
            activateSelectedMruTab();
            hideMruPopup();
            event->accept();
        }
        // Otherwise allow default processing
    }
    else {
        // Pass other key release events to the base class
        QTabWidget::keyReleaseEvent(event);
    }
}

/**
 * @brief Updates MRU order when current tab changes
 * @param index New current tab index
 */
void MruTabWidget::onCurrentChanged(int index)
{
    // Update the MRU order when the selected tab changes
    if (index >= 0) {
        QWidget *page = widget(index);
        updateMruOrder(page);
        if (tabAttention(page)) {
            m_tabStates[page].attention = false;
            updateTabMarker(page);
        }
    }
    // Hide the popup if it's open and Ctrl is not pressed (e.g., changed by clicking)
    if (m_mruPopup && !m_ctrlHeld) {
         hideMruPopup();
    }
    // Update the visibility of close buttons when the selected tab changes
    updateCloseButtonVisibility();
}

/**
 * @brief Handles Ctrl+Tab timeout for popup display
 */
void MruTabWidget::handleCtrlTabTimeout()
{
    // Timer timed out -> show the popup if Ctrl is still held
    m_expectingPopup = false; // We are no longer expecting (either we show or we don't)
    if (m_ctrlHeld && !m_mruPopup && count() >= 2) {
        showMruPopup(); // Show the popup (and install the event filter for the list)
    }
}

/**
 * @brief Handles MRU popup list activation
 * @param item Activated list item
 */
void MruTabWidget::onPopupListItemActivated(QListWidgetItem *item)
{
    // Handle list item activation (e.g., Enter, double-click)
    if(item && m_ctrlHeld) { // Check if Ctrl is still held
         activateSelectedMruTab();
         hideMruPopup();
    }
}

/**
 * @brief Updates MRU list after tab removal
 * @param index Removed tab index
 */
void MruTabWidget::forgetRemovedPages()
{
    // Only pointers are compared here; a removed page may already be under destruction.
    auto removed = [this](QWidget *page) { return indexOf(page) < 0; };
    m_mruOrder.erase(std::remove_if(m_mruOrder.begin(), m_mruOrder.end(), removed),
                     m_mruOrder.end());
    for (auto it = m_tabStates.begin(); it != m_tabStates.end();) {
        if (removed(it.key()))
            it = m_tabStates.erase(it);
        else
            ++it;
    }
    if (m_previewPage && removed(m_previewPage)) {
        m_previewPage = nullptr;
    }
}

void MruTabWidget::tabRemoved(int index)
{
    Q_UNUSED(index);
    // The page is already gone from the stack, so drop every page it no longer holds
    forgetRemovedPages();

    // The base class handles the actual removal and emits signals
    // We update the mapping *after* the tab is visually gone
    // Using QTimer::singleShot ensures mapping happens after potential layout changes
    QTimer::singleShot(0, this, [this]() {
        mapCloseButtonsToTabs();
        updateCloseButtonVisibility();
    });
    // No need to call QTabWidget::tabRemoved(index) - the signal handles it

    emit tabCountChanged(count());
}

void MruTabWidget::tabInserted(int index) {
    // A new tab is treated as recently used: it goes right after the current tab
    // (currentChanged has already added it when it became current)
    QWidget *page = widget(index);
    if (page && !m_mruOrder.contains(page)) {
        const bool currentFirst = !m_mruOrder.isEmpty() && m_mruOrder.first() == currentWidget();
        m_mruOrder.insert(currentFirst ? 1 : 0, page);
    }

    // The base class handles insertion and emits signals
    // Update mapping *after* the tab is visually added
    QTimer::singleShot(0, this, [this]() {
        mapCloseButtonsToTabs();
        updateCloseButtonVisibility();
        // Enforce tab limit after tab is added
        enforceTabLimit();
    });
    // No need to call QTabWidget::tabInserted(index)

    emit tabCountChanged(count());
}

/**
 * @brief Main event filter for tab bar and MRU popup
 * @param watched Event target object
 * @param event Event object
 * @return true if event was handled
 *
 * Handles:
 * - Tab bar hover events
 * - MRU popup keyboard navigation
 */
bool MruTabWidget::handleCtrlTabEvent(QKeyEvent *keyEvent)
{
    if (keyEvent->type() == QEvent::KeyPress &&
        (keyEvent->modifiers() & Qt::ControlModifier) &&
        (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab)) {

        if (count() < 2) return false;

        // Sequential mode: simple next/prev tab cycling
        if (m_sequentialTabSwitching) {
            bool forward = (keyEvent->key() == Qt::Key_Tab) && !(keyEvent->modifiers() & Qt::ShiftModifier);
            int newIndex = currentIndex() + (forward ? 1 : -1);
            if (newIndex >= count())
                newIndex = 0;
            else if (newIndex < 0)
                newIndex = count() - 1;
            setCurrentIndex(newIndex);
            return true;
        }

        // MRU mode with popup
        if (!m_ctrlHeld) {
            m_ctrlHeld = true;
            m_expectingPopup = true;
            m_shiftHeldOnTabPress = (keyEvent->modifiers() & Qt::ShiftModifier);
            m_ctrlTabTimer.start();
        } else if (m_mruPopup) {
            bool forward = (keyEvent->key() == Qt::Key_Tab) && !(keyEvent->modifiers() & Qt::ShiftModifier);
            cycleMruPopup(forward);
        } else {
            if (m_ctrlTabTimer.isActive()) {
                m_ctrlTabTimer.stop();
                m_expectingPopup = false;
                showMruPopup();
            } else if (!m_expectingPopup) {
                showMruPopup();
            }
        }
        return true;
    }

    // In sequential mode, Ctrl release doesn't need special handling
    if (m_sequentialTabSwitching)
        return false;

    if (keyEvent->type() == QEvent::KeyRelease && keyEvent->key() == Qt::Key_Control && m_ctrlHeld) {
        m_ctrlHeld = false;
        m_expectingPopup = false;

        if (m_ctrlTabTimer.isActive()) {
            m_ctrlTabTimer.stop();
            performDirectSwitch();
        } else if (m_mruPopup) {
            activateSelectedMruTab();
            hideMruPopup();
        }
        return true;
    }

    return false;
}

bool MruTabWidget::eventFilter(QObject *watched, QEvent *event)
{
    // Handle Ctrl+Tab forwarded from EditorFrame
    if (watched == this && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        if (handleCtrlTabEvent(static_cast<QKeyEvent*>(event)))
            return true;
    }

    // --- Filtering events for QTabBar ---
    if (watched == tabBar()) {
        // Check mouse tracking status on any event for the tab bar
        if (tabBar() && !tabBar()->hasMouseTracking()) {
             qWarning() << "[eventFilter] Mouse tracking is DISABLED on tabBar!";
        }

        switch (event->type()) {
            // --- Handle MouseMove INSTEAD of HoverMove ---
            case QEvent::MouseMove: {
                 if (!tabBar()) {
                     qWarning() << "[MouseMove] tabBar is NULL!";
                     return false;
                 }
                 QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                 int tabIndex = tabBar()->tabAt(mouseEvent->pos());
                 if (tabIndex != m_hoveredTabIndex) {
                    m_hoveredTabIndex = tabIndex;
                    updateCloseButtonVisibility();
                 }
                 return false; // Allow normal processing
            }
            // --- Keep HoverMove as a fallback (though likely not triggered) ---
            case QEvent::HoverMove: {
                if (!tabBar()) { // Paranoid check
                     qWarning() << "[HoverMove] tabBar is NULL!";
                     return false;
                }
                QHoverEvent *hoverEvent = static_cast<QHoverEvent*>(event);
                // Use QTabBar::tabAt to find the tab index under the cursor
                int tabIndex = tabBar()->tabAt(Ev::local(hoverEvent));
                // If the hover index changed
                if (tabIndex != m_hoveredTabIndex) {
                    m_hoveredTabIndex = tabIndex; // Save the new index
                    updateCloseButtonVisibility(); // Update button visibility
                }
                // Return false to allow normal hover processing (e.g., tooltips)
                return false;
            }
            // Mouse leave event from the tab bar area
            case QEvent::Leave: // Handle Leave event (often paired with MouseMove)
            case QEvent::HoverLeave: {
                 if (!tabBar()) { // Paranoid check
                     qWarning() << "[Leave/HoverLeave] tabBar is NULL!";
                     return false;
                }
                // Mouse left the tab bar area
                if (m_hoveredTabIndex != -1) { // If it was previously over a tab
                    m_hoveredTabIndex = -1; // Reset the hover index
                    updateCloseButtonVisibility(); // Update visibility
                }
                return false; // Allow normal processing
            }
            // Mouse clicks - update visibility just in case
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseButtonDblClick:
                 // Update visibility with a slight delay to ensure the state is current
                 QTimer::singleShot(0, this, &MruTabWidget::updateCloseButtonVisibility);
                 return false; // Allow normal click processing

            default:
                break; // Ignore other events for the tab bar
        }
    }
    // --- Filtering events for the MRU popup list (as before) ---
    else if (watched == m_mruListWidget && m_mruPopup) {
        // (MRU popup event handling code remains the same)
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            // Handle Ctrl+Tab/Ctrl+Shift+Tab in the popup
            if (keyEvent->modifiers() & Qt::ControlModifier) {
                if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
                    bool isForward = (keyEvent->key() == Qt::Key_Tab);
                    bool isShiftPressed = (keyEvent->modifiers() & Qt::ShiftModifier);
                    cycleMruPopup(isForward && !isShiftPressed); // Cycle selection in the popup
                    return true; // Event handled
                }
            }
            // Handle Escape in the popup
            else if (keyEvent->key() == Qt::Key_Escape) {
                hideMruPopup(); // Hide popup
                m_ctrlHeld = false; // Reset Ctrl state
                return true; // Event handled
            }
        }
        // Handle Ctrl release while the popup is active
        else if (event->type() == QEvent::KeyRelease) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Control) {
                // If the popup was visible -> activate the selected tab and hide
                if (m_mruPopup && m_mruPopup->isVisible()) {
                    activateSelectedMruTab();
                    hideMruPopup(); // This will also remove the event filter from the list
                }
                m_ctrlHeld = false; // Reset Ctrl state
                return true; // Event handled
            }
        }
    }

    // Pass unhandled events to the base class filter
    return QTabWidget::eventFilter(watched, event);
}

// --- Close Button Visibility Logic ---

// Maps found close buttons to tab indices
void MruTabWidget::mapCloseButtonsToTabs() {
    if (!tabBar()) return; // Check if the tab bar exists

    m_tabIndexToCloseButtonMap.clear(); // Clear the old map
    // Find all QAbstractButton widgets that are children of the QTabBar
    QList<QAbstractButton*> buttons = tabBar()->findChildren<QAbstractButton*>();

    for (QAbstractButton *button : buttons) {
        // Ensure the button is valid and has geometry
        if (!button || button->geometry().isEmpty()) {
             continue; // Skip invalid or geometry-less buttons
        }

        // --- NEW HEURISTIC (Class Focus) ---
        const QMetaObject *metaObj = button->metaObject();
        QString className = metaObj ? metaObj->className() : "";
        QString objName = button->objectName(); // Get object name for scroll button check

        // 1. Ignore known scroll buttons
        if (objName == "ScrollLeftButton" || objName == "ScrollRightButton") {
            continue;
        }

        // 2. Check if the class is "CloseButton"
        bool looksLikeCloseButton = (className == "CloseButton");

        if (looksLikeCloseButton) {
            // 3. Find the tab index for this button
            QPoint buttonCenter = button->geometry().center();
            int tabIndex = tabBar()->tabAt(buttonCenter);

            if (tabIndex != -1) {
                 m_tabIndexToCloseButtonMap[tabIndex] = button; // Add to map
            } else {
            }
        } else {
        }
        // --- END OF NEW HEURISTIC ---
    }
}

// Updates the visibility of close buttons based on state
void MruTabWidget::updateCloseButtonVisibility() {
    if (!tabBar()) return; // Check if the tab bar exists

    int currentSelectedTabIndex = currentIndex(); // Current selected index

    // Set of buttons that *should* be visible according to logic
    QSet<QAbstractButton*> buttonsToShow;

    // Iterate through the mapped buttons to determine which should be shown
    // We now use the map which should only contain the actual CloseButtons
    for (auto it = m_tabIndexToCloseButtonMap.constBegin(); it != m_tabIndexToCloseButtonMap.constEnd(); ++it) {
        int tabIndex = it.key();
        QPointer<QAbstractButton> button = it.value();

        if (!button) continue; // Skip if the button pointer is invalid

        bool isSelected = (tabIndex == currentSelectedTabIndex); // Is the tab selected?
        bool isHovered = (tabIndex == m_hoveredTabIndex);      // Is the tab hovered?

        // If the tab is selected OR hovered -> the button should be visible
        if (isSelected || isHovered) {
            buttonsToShow.insert(button.data()); // Add the raw pointer to the set
        }
    }

    // Now iterate through *all* buttons we mapped (i.e., only CloseButtons)
    // and set their visibility
    for (auto it = m_tabIndexToCloseButtonMap.constBegin(); it != m_tabIndexToCloseButtonMap.constEnd(); ++it) {
         QPointer<QAbstractButton> button = it.value();
         if (!button) continue;

         bool shouldBeVisible = buttonsToShow.contains(button.data());

         if (button->isVisible() != shouldBeVisible) {
             button->setVisible(shouldBeVisible);
         }
    }

    // We no longer need to iterate through `allButtons` because we only manage
    // the ones identified as CloseButton in the map.
    // The visibility of scroll arrows is managed by QTabBar itself.
}

bool MruTabWidget::isTabPinned(int tabIndex) const
{
    if (tabIndex < 0 || tabIndex >= count()) {
        return false;
    }
    return isTabPinned(widget(tabIndex));
}

bool MruTabWidget::isTabPinned(QWidget *page) const
{
    return m_tabStates.value(page).pinned;
}

QVector<QWidget*> MruTabWidget::findLeastRecentlyUsedUnpinnedTabs(int atMost) const
{
    QVector<QWidget*> result;
    if (atMost <= 0) return result;

    auto candidate = [this, &result](QWidget *w) {
        return w && indexOf(w) >= 0 && !isTabPinned(w) && w != m_previewPage && !result.contains(w);
    };

    // First, try to find unpinned tabs from MRU list (least recent to most recent)
    for (int i = m_mruOrder.size() - 1; i >= 0 && result.size() < atMost; --i) {
        if (candidate(m_mruOrder[i]))
            result.append(m_mruOrder[i]);
    }

    // If we still need more, search through all tabs from right to left
    for (int i = count() - 1; i >= 0 && result.size() < atMost; --i) {
        if (candidate(widget(i)))
            result.append(widget(i));
    }

    return result;
}

// Tabs that count toward the tab limit: unpinned tabs except the preview tab
int MruTabWidget::limitedTabCount() const
{
    int limited = 0;
    for (int i = 0; i < count(); ++i) {
        QWidget *page = widget(i);
        if (!isTabPinned(page) && page != m_previewPage)
            limited++;
    }
    return limited;
}

void MruTabWidget::updateMruOrder(QWidget *page) {
    if (!page)
        return;
    m_mruOrder.removeAll(page);
    // Insert the page at the beginning (most recently used)
    m_mruOrder.prepend(page);
}

void MruTabWidget::showMruPopup()
{
    // Check preconditions
    if (m_mruPopup || count() < 2) {
        return;
    }

    // Build the list of tabs to show in the popup (MRU + rest)
    QList<QWidget*> tabsToShowOrder;
    for (QWidget *page : m_mruOrder) {
        if (indexOf(page) >= 0 && !tabsToShowOrder.contains(page))
            tabsToShowOrder.append(page);
    }
    for (int i = 0; i < count(); ++i) {
        if (!tabsToShowOrder.contains(widget(i)))
            tabsToShowOrder.append(widget(i));
    }

    if (tabsToShowOrder.size() < 2) {
         return; // Don't show if less than 2 tabs
    }

    // Create and configure the popup
    m_mruPopup = new QDialog(this, Qt::Popup | Qt::FramelessWindowHint);
    m_mruPopup->setObjectName("MruPopupDialog");

    m_mruListWidget = new QListWidget(m_mruPopup);
    m_mruListWidget->setObjectName("MruPopupList");
    m_mruListWidget->setAlternatingRowColors(true);
    connect(m_mruListWidget, &QListWidget::itemActivated, this, &MruTabWidget::onPopupListItemActivated);
    if (m_mruListWidget) {
        m_mruListWidget->installEventFilter(this); // Event filter for keys on the list
    }

    // Populate the list widget based on the new order
    for (QWidget *page : tabsToShowOrder) {
        const int tabIndex = indexOf(page);
        QListWidgetItem *item = new QListWidgetItem(tabPopupText(tabIndex), m_mruListWidget);
        // Store the page, resolved to an index when the item is activated
        item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(page)));
        item->setIcon(tabIcon(tabIndex));
        if (page == m_previewPage) {
            QFont font = item->font();
            font.setItalic(true);
            item->setFont(font);
        }
        m_mruListWidget->addItem(item);
    }

    // Popup layout
    QVBoxLayout *layout = new QVBoxLayout(m_mruPopup);
    layout->addWidget(m_mruListWidget);
    layout->setContentsMargins(2, 2, 2, 2);
    m_mruPopup->setLayout(layout);

    // Set initial selection (second tab in the list - the next one to switch to)
    if (m_mruListWidget->count() > 1) {
        m_mruListWidget->setCurrentRow(1);
    } else {
         m_mruListWidget->setCurrentRow(0); // Fallback
    }

    // Position and show the popup
    QPoint center = mapToGlobal(rect().center());
    QSize popupSizeHint = m_mruPopup->sizeHint();
    popupSizeHint.setWidth(qMax(popupSizeHint.width(), width() / 3)); // Minimum width
    popupSizeHint.setHeight(qMin(popupSizeHint.height(), qMin(height() * 2 / 3, 400))); // Limit height
    m_mruPopup->resize(popupSizeHint);

    int x = center.x() - m_mruPopup->width() / 2;
    int y = center.y() - m_mruPopup->height() / 2;

    // Ensure the popup fits on the screen
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        x = qBound(screenGeometry.left(), x, screenGeometry.right() - m_mruPopup->width());
        y = qBound(screenGeometry.top(), y, screenGeometry.bottom() - m_mruPopup->height());
    }

    m_mruPopup->move(x, y);
    m_mruPopup->setFocus(); // Set focus to the popup
    m_mruListWidget->setFocus(); // Set focus to the list
    m_mruPopup->show();
}

void MruTabWidget::hideMruPopup()
{
    if (m_mruPopup) {
        if (m_mruListWidget) {
            m_mruListWidget->removeEventFilter(this); // Remove event filter from the list
        }
        m_mruPopup->hide();
        m_mruPopup->deleteLater(); // Schedule object deletion
        // QPointer will automatically set itself to nullptr
    }
}

void MruTabWidget::cycleMruPopup(bool forward)
{
    // Cycle selection in the popup list
    if (!m_mruPopup || !m_mruListWidget || m_mruListWidget->count() == 0) {
        return;
    }
    int count = m_mruListWidget->count();
    int currentRow = m_mruListWidget->currentRow();
    int nextRow = forward ? (currentRow + 1) % count : (currentRow - 1 + count) % count;
    m_mruListWidget->setCurrentRow(nextRow);
}

void MruTabWidget::activateSelectedMruTab()
{
    // Activate the tab selected in the popup
    if (!m_mruPopup || !m_mruListWidget) return;
    QListWidgetItem *selectedItem = m_mruListWidget->currentItem();
    if (selectedItem) {
        auto *page = reinterpret_cast<QWidget*>(selectedItem->data(Qt::UserRole).value<quintptr>());
        const int indexToActivate = indexOf(page);
        if (indexToActivate >= 0) {
            setCurrentIndex(indexToActivate); // Set as current tab
        }
    }
}

void MruTabWidget::performDirectSwitch()
{
    // Perform the quick switch (without popup): the most recent tab other than the current one
    QWidget *previous = nullptr;
    for (QWidget *page : m_mruOrder) {
        if (page != currentWidget() && indexOf(page) >= 0) {
            previous = page;
            break;
        }
    }
    if (previous) {
        setCurrentWidget(previous);
    } else {
        // Cyclic logic when MRU < 2
        int current = currentIndex();
        int numTabs = count();
        if (numTabs > 1) {
            int nextIndex = m_shiftHeldOnTabPress ? (current - 1 + numTabs) % numTabs // Backwards if Shift
                                                  : (current + 1) % numTabs; // Forwards if no Shift
            setCurrentIndex(nextIndex);
        }
    }
}

bool MruTabWidget::requestCloseAllTabs()
{
    QList<QPointer<QWidget>> pages;
    for (int index = count() - 1; index >= 0; index--)
        pages.append(widget(index));
    bool allClosed = true;
    for (const QPointer<QWidget> &page : pages) {
        if (page && !requestCloseTab(page.data()))
            allClosed = false;
    }
    return allClosed;
}

void MruTabWidget::closeOtherTabs(int keepIndex)
{
    QList<QWidget*> pages;
    for (int i = count() - 1; i >= 0; --i)
    {
        if (i != keepIndex)
            pages.append(widget(i));
    }
    closePages(pages);
}

void MruTabWidget::closeTabsToLeft(int fromIndex)
{
    QList<QWidget*> pages;
    for (int i = qMin(fromIndex, count()) - 1; i >= 0; --i)
        pages.append(widget(i));
    closePages(pages);
}

void MruTabWidget::closeTabsToRight(int fromIndex)
{
    QList<QWidget*> pages;
    for (int i = count() - 1; i > fromIndex; --i)
        pages.append(widget(i));
    closePages(pages);
}

void MruTabWidget::onTabContextMenuRequested(const QPoint& pos)
{
    int tabIndex = tabBar()->tabAt(pos);
    if (tabIndex < 0) return; // A blank space was clicked

    // Actions resolve the page to its current index when triggered
    QPointer<QWidget> page = widget(tabIndex);
    auto pageIndex = [this, page]() { return page ? indexOf(page.data()) : -1; };

    QMenu menu(this);

    bool canClose = canCloseTabs();

    QAction* closeAction = menu.addAction(tr("Close"));
    closeAction->setShortcut(QKeySequence::Close); // Ctrl+F4
    closeAction->setEnabled(canClose);
    connect(closeAction, &QAction::triggered, this, [this, page]() {
        if (page)
            requestCloseTab(page.data());
    });

    QAction* closeOthersAction = menu.addAction(tr("Close Other Tabs"));
    closeOthersAction->setEnabled(canClose && count() > 1);
    connect(closeOthersAction, &QAction::triggered, this, [this, pageIndex]() {
        if (pageIndex() >= 0)
            closeOtherTabs(pageIndex());
    });

    // Only show "Close All Tabs" when minimalTabCount is 0
    if (m_minimalTabCount == 0) {
        QAction* closeAllAction = menu.addAction(tr("Close All Tabs"));
        connect(closeAllAction, &QAction::triggered, this, [this]() {
            requestCloseAllTabs();
        });
    }

    menu.addSeparator();

    QAction* closeLeftAction = menu.addAction(tr("Close Tabs to the Left"));
    closeLeftAction->setEnabled(canClose && tabIndex > 0);
    connect(closeLeftAction, &QAction::triggered, this, [this, pageIndex]() {
        if (pageIndex() >= 0)
            closeTabsToLeft(pageIndex());
    });

    QAction* closeRightAction = menu.addAction(tr("Close Tabs to the Right"));
    closeRightAction->setEnabled(canClose && tabIndex < count() - 1);
    connect(closeRightAction, &QAction::triggered, this, [this, pageIndex]() {
        if (pageIndex() >= 0)
            closeTabsToRight(pageIndex());
    });

    menu.addSeparator();

    const bool pinned = isTabPinned(tabIndex);
    QAction* pinAction = menu.addAction(pinned ? tr("Unpin Tab") : tr("Pin Tab"));
    connect(pinAction, &QAction::triggered, this, [this, page, pinned]() {
        if (page)
            setTabPinned(page.data(), !pinned);
    });

    emit tabContextMenuRequested(page.data(), &menu);
    menu.exec(tabBar()->mapToGlobal(pos));
}

QTabBar::ButtonPosition MruTabWidget::closeButtonSide() const
{
    return static_cast<QTabBar::ButtonPosition>(
        tabBar()->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, tabBar()));
}

QIcon MruTabWidget::pinIcon() const
{
    if (!m_pinIconUri.isEmpty())
        return QIcon(m_pinIconUri);
    return paintedPinIcon(palette().color(QPalette::WindowText));
}

void MruTabWidget::updateTabButton(QWidget *page)
{
    QTabBar* bar = tabBar();
    const int index = indexOf(page);
    if (!bar || index < 0)
        return;

    const QTabBar::ButtonPosition side = closeButtonSide();

    // Remove the old button
    QWidget* existingButton = bar->tabButton(index, side);
    if (existingButton) {
        existingButton->deleteLater();
    }

    // Buttons keep the page, not the index, so they still work after the tab is moved
    QPointer<QWidget> target(page);
    if (isTabPinned(page))
    {
        // Pinned: show the pin, which unpins the tab
        QToolButton* pinButton = new QToolButton(bar);
        pinButton->setIcon(pinIcon());
        pinButton->setIconSize(QSize(16, 16));
        pinButton->setCursor(Qt::PointingHandCursor);
        pinButton->setToolTip(tr("Unpin Tab"));
        pinButton->setStyleSheet("QToolButton { border: none; padding: 0px; }");

        connect(pinButton, &QToolButton::clicked, this, [this, target]() {
            if (target)
                setTabPinned(target.data(), false);
        });

        bar->setTabButton(index, side, pinButton);
    }
    else if (tabsClosable())
    {
        // Not pinned: show the close button
        QToolButton* closeButton = new QToolButton(bar);
        closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        closeButton->setIconSize(QSize(16, 16));
        closeButton->setCursor(Qt::ArrowCursor);
        closeButton->setToolTip(tr("Close Tab"));
        closeButton->setStyleSheet("QToolButton { border: none; padding: 0px; }");

        connect(closeButton, &QToolButton::clicked, this, [this, target]() {
            const int targetIndex = target ? indexOf(target.data()) : -1;
            if (targetIndex >= 0)
                emit tabCloseRequested(targetIndex);
        });

        bar->setTabButton(index, side, closeButton);
    }
    else
    {
        bar->setTabButton(index, side, nullptr);
    }
}

void MruTabWidget::updateTabMarker(QWidget *page)
{
    QTabBar* bar = tabBar();
    const int index = indexOf(page);
    if (index < 0)
        return;

    const QTabBar::ButtonPosition side =
        closeButtonSide() == QTabBar::LeftSide ? QTabBar::RightSide : QTabBar::LeftSide;
    QWidget* existing = bar->tabButton(index, side);
    auto* marker = dynamic_cast<TabMarker*>(existing);
    if (existing && !marker)
        return; // The client's own button, leave it alone

    const TabState state = m_tabStates.value(page);
    if (!state.busy && !state.attention) {
        if (marker) {
            bar->setTabButton(index, side, nullptr);
            marker->deleteLater();
        }
        return;
    }
    if (!marker) {
        marker = new TabMarker(bar);
        bar->setTabButton(index, side, marker);
    }
    marker->setState(state.busy, state.attention);
}

int MruTabWidget::enforceTabLimit()
{
    if (m_tabLimit <= 0) return 0;
    if (m_limitBusy) {
        // Called from a nested event loop while a limit question is open; check again later
        m_limitCheckPending = true;
        return 0;
    }

    const int limitedCount = limitedTabCount();
    if (limitedCount <= m_tabLimit) return 0;

    const QVector<QWidget*> found = findLeastRecentlyUsedUnpinnedTabs(limitedCount - m_tabLimit);
    const QList<QPointer<QWidget>> tabsToClose(found.begin(), found.end());

    m_limitBusy = true;
    int removedCount = 0;
    for (const QPointer<QWidget> &w : tabsToClose) {
        if (!w || indexOf(w.data()) == -1) continue; // Widget not found, skip

        // Try to close the tab with askPin = true
        if (requestCloseTab(w.data(), true)) {
            // Tab was closed successfully
            removedCount++;
        } else if (w) {
            // User cancelled - pin the tab
            setTabPinned(w.data(), true);
        }
    }

    m_limitBusy = false;
    finishLimitCheck();
    return removedCount;
}

bool MruTabWidget::makeRoomForNewTab()
{
    if (m_tabLimit <= 0) return true;
    // Another limit question is open; adding a tab now would bypass it
    if (m_limitBusy) return false;

    m_limitBusy = true;
    bool canAdd = true;
    while (limitedTabCount() + 1 > m_tabLimit) {
        const QVector<QWidget*> found = findLeastRecentlyUsedUnpinnedTabs(1);
        if (found.isEmpty()) break;
        const QPointer<QWidget> page(found.first());

        LimitAction action = LimitAction::Close;
        emit tabLimitReached(page.data(), action);
        if (!page || indexOf(page.data()) < 0)
            continue; // The receiver closed it itself
        if (action == LimitAction::Cancel) {
            canAdd = false;
            break;
        }
        if (action == LimitAction::Keep) {
            setTabPinned(page.data(), true);
            continue;
        }
        if (!requestCloseTab(page.data(), true)) {
            canAdd = false;
            break;
        }
    }
    m_limitBusy = false;
    finishLimitCheck();
    return canAdd;
}

void MruTabWidget::finishLimitCheck()
{
    if (!m_limitCheckPending) return;
    m_limitCheckPending = false;
    QTimer::singleShot(0, this, [this]() { enforceTabLimit(); });
}


void MruTabWidget::swapTabs(int a, int b)
{
    if (a == b || a < 0 || b < 0 || a >= count() || b >= count())
        return;

    if (a > b) std::swap(a, b);

    // Visual swap via two moveTab calls (no tabInserted/tabRemoved triggered).
    // Per-tab state belongs to the pages, so nothing else needs to change.
    tabBar()->moveTab(a, b);
    tabBar()->moveTab(b - 1, a);
}

void MruTabWidget::swapExternal(MruTabWidget* other, int thisIndex, int otherIndex)
{
    if (!other || thisIndex < 0 || otherIndex < 0
            || thisIndex >= count() || otherIndex >= other->count())
        return;

    QWidget* thisWidget  = widget(thisIndex);
    QWidget* otherWidget = other->widget(otherIndex);
    QString  thisText    = tabText(thisIndex);
    QString  otherText   = other->tabText(otherIndex);
    QIcon    thisIcon    = tabIcon(thisIndex);
    QIcon    otherIcon   = other->tabIcon(otherIndex);
    QVariant thisData    = tabBar()->tabData(thisIndex);
    QVariant otherData   = other->tabBar()->tabData(otherIndex);
    const TabState thisState  = m_tabStates.value(thisWidget);
    const TabState otherState = other->m_tabStates.value(otherWidget);
    const bool thisPreview  = thisWidget == m_previewPage;
    const bool otherPreview = otherWidget == other->m_previewPage;

    // Block currentChanged and tabCountChanged during structural changes
    QSignalBlocker b1(this), b2(other);

    removeTab(thisIndex);
    other->removeTab(otherIndex);

    insertTab(thisIndex, otherWidget, otherText);
    setTabIcon(thisIndex, otherIcon);
    tabBar()->setTabData(thisIndex, otherData);
    other->insertTab(otherIndex, thisWidget, thisText);
    other->setTabIcon(otherIndex, thisIcon);
    other->tabBar()->setTabData(otherIndex, thisData);

    setCurrentIndex(thisIndex);
    other->setCurrentIndex(otherIndex);

    setCurrentWidget(otherWidget);
    other->setCurrentWidget(thisWidget);

    // Unblock before pinning — setTabPinned may call enforceTabLimit which needs signals
    b1.unblock();
    b2.unblock();

    // The pages carry their state to the other widget; pinning goes through
    // setTabPinned so that the pin button is created
    auto carry = [](MruTabWidget *to, QWidget *page, TabState state, bool preview) {
        const bool pinned = state.pinned;
        state.pinned = false;
        to->m_tabStates[page] = state;
        to->updateTabMarker(page);
        if (preview)
            to->setTabPreview(page, true);
        if (pinned)
            to->setTabPinned(page, true);
    };
    carry(this, otherWidget, otherState, otherPreview);
    carry(other, thisWidget, thisState, thisPreview);

    // If this lost a pinned and gained an unpinned, unpinned count may exceed limit.
    // Pin the newly inserted tab instead of risking enforceTabLimit closing something.
    if (thisState.pinned && !otherState.pinned && m_tabLimit > 0) {
        if (limitedTabCount() > m_tabLimit)
            setTabPinned(otherWidget, true);
    }

    // Symmetric: other lost a pinned and gained an unpinned
    if (otherState.pinned && !thisState.pinned && other->m_tabLimit > 0) {
        if (other->limitedTabCount() > other->m_tabLimit)
            other->setTabPinned(thisWidget, true);
    }

    // Restore currentChanged state
    onCurrentChanged(currentIndex());
    other->onCurrentChanged(other->currentIndex());
}

void MruTabWidget::setTabPopupText(int index, const QString& text)
{
    if (index < 0 || index >= count()) return;
    tabBar()->setTabData(index, text);
}

QString MruTabWidget::tabPopupText(int index) const
{
    if (index < 0 || index >= count()) return {};
    const QString text = tabBar()->tabData(index).toString();
    return text.isEmpty() ? tabText(index) : text;
}

void MruTabWidget::setTabPinned(int tabIndex, bool pinned)
{
    if (tabIndex < 0 || tabIndex >= count()) return;
    setTabPinned(widget(tabIndex), pinned);
}

void MruTabWidget::setTabPinned(QWidget *page, bool pinned)
{
    if (!page || indexOf(page) < 0) return;

    TabState &state = m_tabStates[page];
    if (state.pinned == pinned) return;
    state.pinned = pinned;

    // Update visual appearance
    updateTabButton(page);

    if (pinned && page == m_previewPage) {
        // A pinned tab is kept, so it is no longer a preview
        promotePreviewTab();
    } else if (!pinned) {
        // If unpinning, we might need to enforce the tab limit
        enforceTabLimit();
    }
}

void MruTabWidget::ensurePreviewStyle()
{
    if (m_previewStyleInstalled)
        return;
    m_previewStyleInstalled = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
    const QString styleName = QApplication::style()->name();
#else
    const QString styleName = QApplication::style()->objectName();
#endif
    // The proxy owns its base style, so it gets its own instance of the application style
    auto *previewStyle = new PreviewTabStyle(QStyleFactory::create(styleName), this);
    previewStyle->setParent(this);
    tabBar()->setStyle(previewStyle);
}

void MruTabWidget::setTabPreview(QWidget *page, bool preview)
{
    if (!page || indexOf(page) < 0) return;

    QWidget *oldPreview = m_previewPage;
    if (preview) {
        if (page == oldPreview) return;
        if (isTabPinned(page)) {
            m_tabStates[page].pinned = false;
            updateTabButton(page);
        }
        ensurePreviewStyle();
        m_previewPage = page;
    } else {
        if (page != oldPreview) return;
        m_previewPage = nullptr;
    }
    tabBar()->update();
    // A former preview tab now counts toward the limit
    if (oldPreview)
        enforceTabLimit();
}

void MruTabWidget::promotePreviewTab()
{
    QWidget *page = m_previewPage;
    if (!page) return;
    m_previewPage = nullptr;
    tabBar()->update();
    emit previewTabPromoted(page);
    enforceTabLimit();
}

void MruTabWidget::setTabKey(QWidget *page, const QString &key)
{
    if (!page || indexOf(page) < 0) return;
    m_tabStates[page].key = key;
}

QString MruTabWidget::tabKey(QWidget *page) const
{
    return m_tabStates.value(page).key;
}

QWidget *MruTabWidget::findTab(const QString &key) const
{
    if (key.isEmpty()) return nullptr;
    for (int i = 0; i < count(); ++i) {
        if (m_tabStates.value(widget(i)).key == key)
            return widget(i);
    }
    return nullptr;
}

void MruTabWidget::setTabBusy(QWidget *page, bool busy)
{
    if (!page || indexOf(page) < 0) return;
    TabState &state = m_tabStates[page];
    if (state.busy == busy) return;
    state.busy = busy;
    updateTabMarker(page);
}

bool MruTabWidget::isTabBusy(QWidget *page) const
{
    return m_tabStates.value(page).busy;
}

void MruTabWidget::setTabAttention(QWidget *page, bool attention)
{
    if (!page || indexOf(page) < 0) return;
    if (attention && page == currentWidget()) return;
    TabState &state = m_tabStates[page];
    if (state.attention == attention) return;
    state.attention = attention;
    updateTabMarker(page);
}

bool MruTabWidget::tabAttention(QWidget *page) const
{
    return m_tabStates.value(page).attention;
}
