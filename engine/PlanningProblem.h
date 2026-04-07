#pragma once

#include <QVariant>
#include "../domain/action.h"
#include "../domain/state.h"

class PlanningProblem {
public:
    PlanningProblem();

private:

    State initial;
    State goal;
    QList<Action> actions;
    QVariant domain;

};
