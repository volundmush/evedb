#include <QCoreApplication>
#include "evedatabase.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    qRegisterMetaType<LongIDSet>("LongIDSet");
    EveDB *data = new EveDB();

    return a.exec();
}
