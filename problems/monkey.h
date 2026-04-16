// the base of the room that the monkey is in
#pragma once

#include <QString>
#include "../engine/planning/plan_search.h"
#include "../engine/search/search.h"
#include "../engine/logic/expr.h"
#include "../engine/planning/action.h"

QList<Expr> initialState() {
    QList<Expr> state{

        // objects
        Expr("Monkey", {Expr("monkey")}), // add the monkey
        Expr("NotBox", {Expr("monkey")}), // box is a box
        Expr("NotBananas", {Expr("monkey")}),
        Expr("NotLocaton", {Expr("monkey")}),

        Expr("Box", {Expr("box")}), // box is a box
        Expr("NotMonkey", {Expr("box")}), // add the monkey
        Expr("NotBananas", {Expr("box")}), // add the monkey
        Expr("NotLocaton", {Expr("box")}),

        Expr("Bananas", {Expr("bananas")}),
        Expr("NotMonkey", {Expr("bananas")}),
        Expr("NotBox", {Expr("bananas")}),
        Expr("NotLocaton", {Expr("bananas")}),

        Expr("Location", {Expr("A")}),
        Expr("NotMonkey", {Expr("A")}),
        Expr("NotBox", {Expr("A")}),
        Expr("NotLocaton", {Expr("A")}),

        Expr("Location", {Expr("B")}),
        Expr("NotMonkey", {Expr("B")}),
        Expr("NotBox", {Expr("B")}),
        Expr("NotLocaton", {Expr("B")}),

        Expr("Location", {Expr("C")}),
        Expr("NotMonkey", {Expr("C")}),
        Expr("NotBox", {Expr("C")}),
        Expr("NotLocaton", {Expr("C")}),

        // locations of objects
        Expr("At", {Expr("monkey"), Expr("A")}),
        Expr("NotAt", {Expr("monkey"), Expr("B")}),
        Expr("NotAt", {Expr("monkey"), Expr("C")}),

        Expr("At", {Expr("box"), Expr("B")}),
        Expr("NotAt", {Expr("box"), Expr("A")}),
        Expr("NotAt", {Expr("box"), Expr("C")}),

        Expr("At", {Expr("bananas"), Expr("C")}),
        Expr("NotAt", {Expr("bananas"), Expr("A")}),
        Expr("NotAt", {Expr("bananas"), Expr("B")}),

        // Height of object
        Expr("NotHigh", {Expr("monkey")}),
        Expr("NotHigh", {Expr("box")}),
        Expr("High", {Expr("bananas")}),

        // what is on what
        Expr("NotOn", {Expr("monkey"), Expr("box")}),
        Expr("NotOn", {Expr("monkey"), Expr("bananas")}),

    };

    return state;
}


QList<Action> monkeyActions() {

    QList<Action> actions;

    Action Go
    (
        Expr("Go", {Expr("object"), Expr("origin"), Expr("destination")}),
        {
            Expr("Monkey", {Expr("object")}), // object must be a monkey
            Expr("Location", {Expr("origin")}),
            Expr("Location", {Expr("destination")}),
            Expr("NotHigh", {Expr("object")}),
            Expr("At", {Expr("object"), Expr("origin")}), // object needs to be at origin
        },
        {
            Expr("At", {Expr("object"), Expr("destination")}), // object now at the destination
            Expr("NotAt", {Expr("object"), Expr("origin")}) // object is no longer at the origin
        }
    );

    Action ClimbUp
    (
        Expr("ClimbUp", {Expr("climber"), Expr("climbee"), Expr("location")}),
        {
            Expr("Monkey", {Expr("climber")}), // object must be a monkey
            Expr("Box", {Expr("climbee")}), // only a box can be climbed on
            Expr("NotHigh", {Expr("climber")}),
            Expr("NotHigh", {Expr("climbee")}),
            Expr("Location", {Expr("location")}), // climber is at the location
            Expr("At", {Expr("climber"), Expr("location")}),
            Expr("At", {Expr("climbee"), Expr("location")}), // climbee is also at the location
            Expr("NotOn", {Expr("climber"), Expr("climbee")}) // climber is not on the climber
        },
        {
            Expr("On", {Expr("climber"), Expr("climbee")}),
            Expr("High", {Expr("climber")})
        }
    );

    Action ClimbDown
        (
            Expr("ClimbDown", {Expr("climber"), Expr("climbee")}),
            {
                Expr("On", {Expr("climber"), Expr("climbee")}),
            },
            {
                Expr("NotHigh", {Expr("climber")}),
                Expr("NotOn", {Expr("climber"), Expr("climbee")}), // climber not longer on climbee
            }
        );

    Action Push
    (
        Expr("Push", {Expr("pusher"), Expr("pushee"), Expr("origin"), Expr("destination")}),
        {
            Expr("Monkey", {Expr("pusher")}), // object must be a monkey
            Expr("Box", {Expr("pushee")}), // only a box can be climbed on
            Expr("NotHigh", {Expr("pusher")}),
            Expr("NotHigh", {Expr("pushee")}),
            Expr("Location", {Expr("origin")}), // climber is at the location
            Expr("Location", {Expr("destination")}),
            Expr("At", {Expr("pusher"), Expr("origin")}),
            Expr("At", {Expr("pushee"), Expr("origin")}), // climbee is also at the location
        },
        {
            Expr("At", {Expr("pusher"), Expr("destination")}),
            Expr("At", {Expr("pushee"), Expr("destination")}),
        }
    );

    actions.append(Go);
    actions.append(ClimbUp);
    actions.append(ClimbDown);
    actions.append(Push);

    return actions;
}

QList<Expr> makeGoals() {
    QList<Expr> state
    {
        Expr("At", {Expr("monkey"), Expr("C")}),
        Expr("High", {Expr("monkey")})
    };

    return state;
}
