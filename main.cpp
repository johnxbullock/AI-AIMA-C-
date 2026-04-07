#include <QCoreApplication>
#include "engine/logic/expr.h"
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QString john("John");
    QString mommy("Mommy");

    Expr ismother("IsMotherOf", {Expr(mommy), Expr(john)});

    qInfo() << ismother.toString();

    qInfo() << ismother.args();

    return a.exec();
}
