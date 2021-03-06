#include "limitcards.h"
#include "card.h"
#include "config.h"
#include <QDebug>

LimitCards *limitCards = nullptr;

LimitCards::LimitCards()
{
    QImage img;
    img.load("textures/lim.png", "1");

    int w = img.width() / 2, h = img.height() / 2;

    for(auto &p : limits)
    {
        p.reset(new QPixmap);
    }

    if(img.width() > 0)
    {
        limits[0]->convertFromImage(img.copy(0, 0, w, h));
        limits[1]->convertFromImage(img.copy(w, 0, w, h));
        limits[2]->convertFromImage(img.copy(0, h, w, h));
    }

    QFile lflist("lflist.conf");
    if(lflist.open(QFile::Text | QFile::ReadOnly))
    {
        QTextStream in(&lflist);
        in.setCodec("utf-8");
        QString name("N/A");
        Table table;

        for(QString line = in.readLine(); !line.isNull(); line = in.readLine())
        {
            if(line.length() < 1)
            {
                continue;
            }
            if(line[0] == '#')
            {
                continue;
            }
            if(line[0] == '!')
            {
                Table newTable;
                newTable.swap(table);
                tables.append(qMakePair(name, std::move(newTable)));
                name = line.mid(1);
                name.trimmed();
                table.reserve(150);
                continue;
            }

            int pos = line.indexOf(QChar(' '));
            if(pos > 0)
            {
                quint32 id = line.left(pos).toUInt();
                int numPos = line.indexOf(QChar(' '), pos + 1);
                int lim = line.mid(pos + 1, numPos - pos - 1).toInt();
                if(!cardPool->getCard(id).isNull())
                {
                    table.insert(id, lim);
                }
            }
        }
    }
}


QPixmap* LimitCards::getPixmap(int i)
{
    static QPixmap nullPixmap;
    if(i >= 0 && i <= 2)
    {
        return limits[i].data();
    }
    return &nullPixmap;
}

int LimitCards::getLimit(quint32 id)
{
    if(config->limit < 0)
    {
        return INT_MAX;
    }
    else if(tables.size() <= config->limit)
    {
        return 3;
    }
    auto &table = tables[config->limit].second;

    if(cardPool->getCard(id).isNull())
    {
        return 0;
    }

    call_with_ref([&](Card &card) {
        if(card.alias != 0)
        {
            id = card.alias;
        }
    }, cardPool->getCard(id));

    auto it = table.find(id);
    if(it == table.end())
    {
        return 3;
    }
    else
    {
        return it.value();
    }
}

Type::DeckP LimitCards::getCards(int index)
{
    auto cards = Type::DeckP::create();
    if(index >= 0 && index < tables.size())
    {
        auto &table = tables[index].second;
        cards->reserve(table.size());

        for(auto it = table.begin(); it != table.end(); ++it)
        {
            cards->append(it.key());
        }

        qSort(cards->begin(), cards->end(), [&] (quint32 id1, quint32 id2) {
           auto it1 = table.find(id1);
           auto it2 = table.find(id2);

           if(it1.value() == it2.value())
           {
               return call_with_def([](Card &card1, Card &card2) {
                   return (card1.type & 7) < (card2.type & 7);
               },
               (it1.value() < it2.value()),
               cardPool->getCard(id1), cardPool->getCard(id2));
           }
           return it1.value() < it2.value();
        });
    }
    return cards;
}
