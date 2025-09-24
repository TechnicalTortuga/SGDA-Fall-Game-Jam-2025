#include "CommandManager.h"
#include "MainWindow.h"

// Command Manager implementation
CommandManager::CommandManager() : currentIndex_(0) {}

void CommandManager::ExecuteCommand(std::unique_ptr<Command> command)
{
    // Remove any commands after current index (when executing new command after undo)
    if (currentIndex_ < history_.size()) {
        history_.erase(history_.begin() + currentIndex_, history_.end());
    }

    command->Execute();
    history_.push_back(std::move(command));
    currentIndex_ = history_.size();
}

void CommandManager::Undo()
{
    if (CanUndo()) {
        currentIndex_--;
        history_[currentIndex_]->Undo();
    }
}

void CommandManager::Redo()
{
    if (CanRedo()) {
        history_[currentIndex_]->Execute();
        currentIndex_++;
    }
}

bool CommandManager::CanUndo() const
{
    return currentIndex_ > 0;
}

bool CommandManager::CanRedo() const
{
    return currentIndex_ < history_.size();
}

void CommandManager::Clear()
{
    history_.clear();
    currentIndex_ = 0;
}

// Command implementations
CreateBrushCommand::CreateBrushCommand(MainWindow* mainWindow, const Brush& brush)
    : mainWindow_(mainWindow), brush_(brush), brushIndex_(0)
{
}

void CreateBrushCommand::Execute()
{
    brushIndex_ = mainWindow_->AddBrush(brush_);
}

void CreateBrushCommand::Undo()
{
    if (brushIndex_ < mainWindow_->GetBrushCount()) {
        mainWindow_->RemoveBrush(brushIndex_);
    }
}

std::string CreateBrushCommand::GetDescription() const
{
    return "Create Brush";
}

DeleteBrushCommand::DeleteBrushCommand(MainWindow* mainWindow, size_t brushIndex)
    : mainWindow_(mainWindow), brushIndex_(brushIndex)
{
    if (brushIndex_ < mainWindow_->GetBrushCount()) {
        deletedBrush_ = mainWindow_->GetBrush(brushIndex_);
    }
}

void DeleteBrushCommand::Execute()
{
    if (brushIndex_ < mainWindow_->GetBrushCount()) {
        mainWindow_->RemoveBrush(brushIndex_);
    }
}

void DeleteBrushCommand::Undo()
{
    mainWindow_->AddBrush(deletedBrush_);
}

std::string DeleteBrushCommand::GetDescription() const
{
    return "Delete Brush";
}
