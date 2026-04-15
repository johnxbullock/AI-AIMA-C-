// the base of the room that the monkey is in
#pragma once

#include <QString>
#include "../engine/planning/plan_search.h"
#include "../engine/search/search.h"
#include "../engine/logic/expr.h"
#include "../engine/logic/utils.h"

// the monkey's height
enum class Height {
    LOW,
    HIGH
};

// the location of an object
enum class Location {
    A,
    B,
    C
};

// the state

struct MonkeyState {
    Height monkeyHeight; // keep track of the monkey's height
    Location monkeyLocation;
    Location boxLocation;
    Location bananaLocation;
    std::pair<bool, QString*> grasped; // whether the monkey is grasping something and what the monkey is grasping
};

// actions

