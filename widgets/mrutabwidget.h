#ifndef MRUTABWIDGET_H
#define MRUTABWIDGET_H

#include <QTabWidget>
#include <QTabBar>
#include <QList>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QPointer>
#include <QMap>
#include <QResizeEvent>
#include <QMenu>
#include <QMetaMethod>

class QDialog;
class QListWidget;
class QListWidgetItem;
class QAbstractButton;

/**
 * @class MruTabWidget
 * @brief Extended QTabWidget with MRU navigation, tab pinning and smart tab management
 *
 * Provides enhanced tab management features for modern IDE-style applications:
 * - MRU (Most Recently Used) navigation using Ctrl+Tab/Ctrl+Shift+Tab sequences
 * - Automatic tab management with configurable unpinned tab limit and auto-saving
 * - Context-sensitive close button visibility (visible only for selected/hovered tabs)
 * - Persistent pinned tabs with independent lifetime management
 * - An optional IDE-style preview tab, tab keys and busy/attention markers
 * - don't use QTabWidget::tabCloseRequested, use instead MruTabWidget signals
 *
 * Per-tab state (pinned, preview, busy, attention, key, MRU position) belongs to the
 * page widget, so it follows a tab when the tab is moved.
 */
class MruTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    /// @brief What happens to a tab that has to make room for a new one; see makeRoomForNewTab().
    enum class LimitAction {
        Close,  ///< Close the tab (tabAboutToClose is still asked, with askPin = true)
        Keep,   ///< Keep the tab open and pin it, so it no longer counts toward the limit
        Cancel  ///< Close nothing more; the new tab must not be added
    };
    Q_ENUM(LimitAction)

signals:
    /**
     * @brief Emitted by makeRoomForNewTab() for a tab that must give way to a new tab.
     *
     * @p action is Close when the receiver leaves it unchanged. Receivers must use a direct
     * connection, because @p action is read right after emission.
     */
    void tabLimitReached(QWidget *page, MruTabWidget::LimitAction &action);
    /**
     * @brief Emitted before a tab is closed; set @p allow to false to keep the tab.
     *
     * @p askPin is true when the close comes from enforceTabLimit(); a vetoed tab is then
     * pinned. Receivers must use a direct connection (the default for receivers in the
     * same thread), because @p allow is read right after emission. With several receivers
     * @p allow may already be false when a receiver runs.
     */
    void tabAboutToClose(QWidget *page, bool askPin, bool &allow);
    /// @brief Emitted after closing was allowed, before the page is removed. Do not delete @p page here.
    void tabClosing(QWidget *page);
    /// @brief Emitted after the page was removed from the widget and before it is deleted.
    void tabClosed(QWidget *page);
    void tabContextMenuRequested(QWidget *page, QMenu *menu);
    void tabCountChanged(int count);
    /// @brief Emitted when the preview tab becomes a regular tab.
    void previewTabPromoted(QWidget *page);
public:
    /**
     * @brief Constructs an MRU-enabled tab widget
     * @param parent Parent widget
     */
    explicit MruTabWidget(QWidget *parent = nullptr);

    /**
     * @brief Destructor cleans up resources
     */
    ~MruTabWidget() override;

    /**
     * @brief Closes a tab after asking tabAboutToClose.
     * @return true when the tab was closed
     *
     * Emits tabAboutToClose, tabClosing, removes the page, emits tabClosed and deletes the
     * page with deleteLater() unless deletePagesOnClose() is false.
     */
    bool requestCloseTab(QWidget *page, bool askPin = false);
    bool requestCloseTab(int index, bool askPin = false);

    /// @brief When false, closed pages are only removed and the client owns them after tabClosed.
    void setDeletePagesOnClose(bool deletePages) { m_deletePagesOnClose = deletePages; }
    bool deletePagesOnClose() const { return m_deletePagesOnClose; }

    /**
     * @brief Sets maximum number of allowed unpinned tabs
     * @param limit Maximum tab count (0 = unlimited)
     * @note limit is clamped to at least minimalTabCount; the preview tab is not counted
     */
    void setTabLimit(int limit);

    /**
     * @brief Sets minimum number of tabs that must remain open
     * @param minCount Minimum tab count (0 = no minimum, can close all)
     *
     * When minCount > 0:
     * - Close actions are disabled/hidden when count <= minCount
     * - Ctrl+W is blocked when count <= minCount
     * - "Close All Tabs" menu item is hidden
     */
    void setMinimalTabCount(int minCount);
    int minimalTabCount() const { return m_minimalTabCount; }

    /**
     * @brief Checks if closing tabs is currently allowed
     * @return true if count() > minimalTabCount
     */
    bool canCloseTabs() const;

    /**
     * @brief Enforces currently set tab limit
     *
     * Closes least recently used unpinned tabs (never the preview tab) until under limit
     * @return number of closed tabs
     */
    int enforceTabLimit();

    /**
     * @brief Makes room for one more unpinned tab; call it before adding the tab.
     * @return false when the new tab must not be added
     *
     * While the limit would be exceeded, emits tabLimitReached for the least recently used
     * unpinned tab and closes or pins it as the receiver decides. Returns false when a
     * receiver chose Cancel, when a close was vetoed in tabAboutToClose (the vetoed tab is
     * not pinned) or while another tab limit question is still open.
     */
    bool makeRoomForNewTab();

    /**
     * @brief Sets pin state for a tab
     * @param page Page of the tab to modify
     * @param pinned Whether to pin the tab
     *
     * Pinning the preview tab promotes it.
     */
    void setTabPinned(QWidget *page, bool pinned);
    void setTabPinned(int tabIndex, bool pinned);
    bool isTabPinned(QWidget *page) const;
    bool isTabPinned(int tabIndex) const;
    bool requestCloseAllTabs();
    void closeOtherTabs(int keepIndex);
    void closeTabsToLeft(int fromIndex);
    void closeTabsToRight(int fromIndex);
    /// @brief Overrides the built-in pin icon; an empty string restores it.
    void setPinIconUri(QString iconUri) { m_pinIconUri = iconUri; }

    void swapTabs(int a, int b);
    void swapExternal(MruTabWidget* other, int thisIndex, int otherIndex);

    /**
     * @brief Sets the text shown for a tab in the Ctrl+Tab MRU popup.
     * @param index Tab index
     * @param text  Text to display in the popup
     *
     * The tab's title (tabText) is what gets drawn on the tab itself; this
     * property is drawn instead in the MRU popup. When empty (default), the
     * popup falls back to tabText. Typically a longer string than the title,
     * e.g. a tab titled "subdir" with a popup text "subdir1/subdir2".
     */
    void    setTabPopupText(int index, const QString& text);

    /**
     * @brief Returns the popup text for a tab, or tabText() when none is set.
     */
    QString tabPopupText(int index) const;

    /**
     * @brief Marks a tab as the preview tab (drawn in italics).
     *
     * There is at most one preview tab; setting a new one clears the old flag. The
     * preview tab does not count toward setTabLimit() and is never closed by
     * enforceTabLimit(). Double-clicking it in the tab bar promotes it.
     */
    void setTabPreview(QWidget *page, bool preview);
    /// @brief Returns the preview tab's page, or nullptr when there is none.
    QWidget *previewTab() const { return m_previewPage; }
    /// @brief Clears the preview flag and emits previewTabPromoted.
    void promotePreviewTab();

    /// @brief Sets a client-defined key identifying what the tab shows.
    void setTabKey(QWidget *page, const QString &key);
    QString tabKey(QWidget *page) const;
    /// @brief Returns the page whose key is @p key, or nullptr.
    QWidget *findTab(const QString &key) const;

    /**
     * @brief Shows a busy spinner on the tab.
     *
     * Busy and attention markers use the tab button on the side opposite the close
     * button; they are not shown when the client put its own widget there.
     */
    void setTabBusy(QWidget *page, bool busy);
    bool isTabBusy(QWidget *page) const;
    /// @brief Marks a background tab; cleared when the tab becomes current. No-op for the current tab.
    void setTabAttention(QWidget *page, bool attention);
    bool tabAttention(QWidget *page) const;

    /// @brief Sets tab switching mode (false = MRU popup, true = sequential)
    void setSequentialTabSwitching(bool sequential) { m_sequentialTabSwitching = sequential; }
    bool sequentialTabSwitching() const { return m_sequentialTabSwitching; }

    // The event filter method
    bool eventFilter(QObject *watched, QEvent *event) override;

protected:
    // Override key event handlers
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

    // Override tab management methods to keep MRU list consistent
    void tabRemoved(int index) override;
    void tabInserted(int index) override;

    // Override show/hide events to manage filter installation
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

    // Override resize event to handle geometry changes
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onCurrentChanged(int index);
    void handleCtrlTabTimeout();
    void onPopupListItemActivated(QListWidgetItem *item);

private:
    struct TabState {
        bool pinned = false;
        bool busy = false;
        bool attention = false;
        QString key;
    };

    void updateMruOrder(QWidget *page);
    void showMruPopup();
    void hideMruPopup();
    void cycleMruPopup(bool forward);
    void activateSelectedMruTab();
    void performDirectSwitch();

    void onTabContextMenuRequested(const QPoint& pos);
    void updateTabButton(QWidget *page);
    void updateTabMarker(QWidget *page);
    QTabBar::ButtonPosition closeButtonSide() const;
    QIcon pinIcon() const;
    void ensurePreviewStyle();
    bool handleCtrlTabEvent(QKeyEvent *keyEvent);
    void closePages(const QList<QWidget*> &pages);
    void forgetRemovedPages();

    void installTabBarEventFilter();
    void removeTabBarEventFilter();
    void updateCloseButtonVisibility();
    void mapCloseButtonsToTabs();

    QVector<QWidget*> findLeastRecentlyUsedUnpinnedTabs(int atMost) const;
    int limitedTabCount() const;
    void finishLimitCheck();


    // --- Member Variables ---
    // Pages, most recently used first. Pages are dropped in tabRemoved().
    QList<QWidget*> m_mruOrder;
    QHash<QWidget*, TabState> m_tabStates;
    QWidget *m_previewPage = nullptr;
    bool m_previewStyleInstalled = false;
    bool m_deletePagesOnClose = true;
    bool m_ctrlHeld = false;
    QTimer m_ctrlTabTimer;
    bool m_expectingPopup = false;
    bool m_shiftHeldOnTabPress = false;
    QString m_pinIconUri;

    QPointer<QDialog> m_mruPopup;
    QPointer<QListWidget> m_mruListWidget;

    // --- New members for close button visibility ---
    int m_hoveredTabIndex = -1; // Index of the tab currently hovered over (-1 if none)
    bool m_isTabBarFilterInstalled = false;
    // Map to store which button corresponds to which tab index; rebuilt after every
    // insert, remove, move and resize. QPointer handles buttons being deleted.
    QMap<int, QPointer<QAbstractButton>> m_tabIndexToCloseButtonMap;

    int m_tabLimit = 0;
    // Set while the tab limit is being enforced. A question to the user (e.g. a message box)
    // runs a nested event loop, in which the deferred check after a tab insertion would
    // otherwise ask about the same tab a second time.
    bool m_limitBusy = false;
    bool m_limitCheckPending = false;
    // Pages whose tabAboutToClose is being answered
    QSet<QWidget*> m_closeQuestions;
    int m_minimalTabCount = 0;
    bool m_sequentialTabSwitching = false;
};

#endif // MRUTABWIDGET_H
