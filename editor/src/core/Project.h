#pragma once

#include <string>
#include <memory>



class Scene;

class Project {
public:
    Project();
    ~Project();

    bool Initialize();
    void Shutdown();

    bool CreateNewProject(const std::string& projectPath);
    bool LoadProject(const std::string& projectPath);
    bool SaveProject();
    bool SaveProjectAs(const std::string& projectPath);

    const std::string& GetProjectPath() const { return projectPath_; }
    std::string GetProjectName() const;
    bool HasUnsavedChanges() const { return hasUnsavedChanges_; }
    void SetModified(bool modified = true) { hasUnsavedChanges_ = modified; }

private:
    std::string projectPath_;
    bool hasUnsavedChanges_;

    bool CreateDefaultScene();
    bool SerializeProject(const std::string& path);
    bool DeserializeProject(const std::string& path);
};


