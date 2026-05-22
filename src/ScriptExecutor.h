//
// Created by MichaelBrunner on 22/05/2026.
//

#ifndef SCRIPTEXECUTOR_H
#define SCRIPTEXECUTOR_H

#include <QObject>
#include <memory>
#include <vector>


namespace CwAPI3D::Interfaces
{
class ICwAPI3DUtilityController;
}

class ScriptExecutor final : public QObject
{
    Q_OBJECT

public:
    explicit ScriptExecutor(CwAPI3D::Interfaces::ICwAPI3DUtilityController *utilityController, QObject *parent = nullptr);
    ~ScriptExecutor() override;

public slots:
    void executeScript(const QByteArray &script) const;

private:
    CwAPI3D::Interfaces::ICwAPI3DUtilityController *utilityController{nullptr};
};

#endif //SCRIPTEXECUTOR_H
