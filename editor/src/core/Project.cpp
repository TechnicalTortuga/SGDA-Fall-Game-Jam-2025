#include "Project.h"
#include "Logger.h"



Project::Project()
    : hasUnsavedChanges_(false)
{
}

Project::~Project()
{
    Shutdown();
}

bool Project::Initialize()
{
    
    // TODO: Initialize project management
    return true;
}

void Project::Shutdown()
{
    
    if (hasUnsavedChanges_) {
        // TODO: Prompt to save
        
    }
}

bool Project::CreateNewProject(const std::string& projectPath)
{
    
    projectPath_ = projectPath;
    hasUnsavedChanges_ = true;

    // TODO: Create default scene
    return CreateDefaultScene();
}

bool Project::LoadProject(const std::string& projectPath)
{
    
    projectPath_ = projectPath;
    hasUnsavedChanges_ = false;

    // TODO: Load project file
    return true;
}

bool Project::SaveProject()
{
    if (projectPath_.empty()) {
        
        return false;
    }

    return SaveProjectAs(projectPath_);
}

bool Project::SaveProjectAs(const std::string& projectPath)
{
    

    if (SerializeProject(projectPath)) {
        projectPath_ = projectPath;
        hasUnsavedChanges_ = false;
        
        return true;
    }

    
    return false;
}

std::string Project::GetProjectName() const
{
    if (projectPath_.empty()) {
        return "Untitled Project";
    }

    // Extract filename from path
    size_t lastSlash = projectPath_.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) ?
        projectPath_.substr(lastSlash + 1) : projectPath_;

    // Remove extension
    size_t lastDot = filename.find_last_of('.');
    if (lastDot != std::string::npos) {
        filename = filename.substr(0, lastDot);
    }

    return filename;
}

bool Project::CreateDefaultScene()
{
    // TODO: Create a basic scene with default objects
    
    return true;
}

bool Project::SerializeProject(const std::string& path)
{
    // TODO: Implement YAML serialization
    
    return true;
}

bool Project::DeserializeProject(const std::string& path)
{
    // TODO: Implement YAML deserialization
    
    return true;
}


