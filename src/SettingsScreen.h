#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Screen.h"
#include "SystemInfo.h"
#include "UpdateService.h"

class SettingsScreen : public Screen {
public:
    SettingsScreen(Context &context, std::function<void()> onReload,
                   std::function<void()> onBuildDatabase,
                   std::function<void()> onFetchArtwork,
                   std::function<void()> onDownloadSystemIcons,
                   std::function<std::string()> systemIconDownloadStatus,
                   std::function<void()> onManageSystems,
                   std::function<void()> onManageControllers,
                   std::function<void()> onManageArcade,
                   std::function<void()> onToggleGamesTab,
                   std::function<void()> onToggleArcadeTab,
                   std::function<void()> onCycleDefaultView,
                   std::function<void()> onStartMisterCore,
                   std::function<void()> onToggleAutomaticCheck,
                   std::function<void()> onCheckUpdates,
                   std::function<void()> onConfirmUpdate,
                   std::function<void()> onInstallUpdate,
                   std::function<void()> onCancelUpdate,
                   std::function<void()> onDismissInstallSuccess,
                   std::function<void()> onShowInstallSuccess,
                   std::function<void()> onRemovePinnedVersion,
                   std::function<void(bool)> onRestartAfterUpdate);

    void setFramebufferInfo(const std::string &info) { framebufferInfo_ = info; }
    void setUpdateSnapshot(const UpdateSnapshot &snapshot);
    bool wantsBack() const { return optionsFocused_; }

    void update(float deltaSeconds) override;
    void render(Canvas &canvas, const Rect &area, bool fullRedraw) override;
    void handle(Action action) override;
    std::string hints() const override;

private:
    struct Row {
        std::string label;
        std::function<std::string()> value;
        std::function<void()> activate;
        bool enabled = true;
    };

    struct Category {
        std::string label;
        std::vector<Row> rows;
    };

    struct Fact {
        std::string label;
        std::string value;
    };

    void buildRows();
    void buildUpdateRows();
    void renderUpdateConfirmation(Canvas &canvas, const Rect &area);
    void renderUpdateSuccess(Canvas &canvas, const Rect &area);
    std::vector<Fact> facts() const;
    void renderCategories(Canvas &canvas, const Rect &area);
    void renderOptions(Canvas &canvas, const Rect &area);
    void renderInfo(Canvas &canvas, const Rect &area);

    Context &context_;
    std::function<void()> onReload_;
    std::function<void()> onBuildDatabase_;
    std::function<void()> onFetchArtwork_;
    std::function<void()> onDownloadSystemIcons_;
    std::function<std::string()> systemIconDownloadStatus_;
    std::function<void()> onManageSystems_;
    std::function<void()> onManageControllers_;
    std::function<void()> onManageArcade_;
    std::function<void()> onToggleGamesTab_;
    std::function<void()> onToggleArcadeTab_;
    std::function<void()> onCycleDefaultView_;
    std::function<void()> onStartMisterCore_;
    std::function<void()> onToggleAutomaticCheck_;
    std::function<void()> onCheckUpdates_;
    std::function<void()> onConfirmUpdate_;
    std::function<void()> onInstallUpdate_;
    std::function<void()> onCancelUpdate_;
    std::function<void()> onDismissInstallSuccess_;
    std::function<void()> onShowInstallSuccess_;
    std::function<void()> onRemovePinnedVersion_;
    std::function<void(bool)> onRestartAfterUpdate_;
    UpdateSnapshot updateSnapshot_;
    int notesScroll_ = 0;
    std::vector<Category> categories_;
    SystemInfo system_;
    std::string framebufferInfo_;
    int categoryCursor_ = 0;
    int optionCursor_ = 0;
    bool optionsFocused_ = false;
};
