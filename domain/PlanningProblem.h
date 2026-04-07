#pragma once

#include <QVariantMap>

struct PlanningProblem {

    QVariantMap initial;
    QVariantMap goals;
    QVariantMap actions;
    QVariantMap domain;

};
