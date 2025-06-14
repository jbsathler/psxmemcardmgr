/* Cette classe a pour but de gerer l'affichage du contenu de la carte
memoire, soit les icones, les descriptions et le pays d'origine. */
#include <QPixmap>
#include <QLayout>
#include <QFrame>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHeaderView>
#include "CardSlots.h"

CardSlots::CardSlots( QWidget *parent, const char *name ) : QFrame(parent), pmEmptyIcon(16,16)
{
	// create a 3 X 15 grid
	// Parent, row, cols, border, space, name
	//QGridLayout *grid = new QGridLayout( this, 5, 15, 1,0,"name" );
	setObjectName(name);
	QVBoxLayout *box = new QVBoxLayout(this);
	box->setSpacing(0);
	box->setContentsMargins(0, 0, 0, 0);
	this->setFrameStyle(QFrame::Panel | QFrame::Sunken);

	listview = new QTreeWidget(this);
	listview->setObjectName("listview");
	listview->setColumnCount(5);
	QStringList headers;
	headers << "Block" << "Icon" << "Title" << "Game ID" << "Product Code";
	listview->setHeaderLabels(headers);

	//listview->setColumnWidthMode(2,Maximum);
	listview->setAllColumnsShowFocus(true);
	listview->setSortingEnabled(false);

	for (int i=0; i<15; ++i)
	{
		card_slots[i] = new QTreeWidgetItem(listview);
		card_slots[i]->setText(0, QString::number(i+1, 10));
		card_slots[i]->setText(1, "");
		card_slots[i]->setText(2, "");
		card_slots[i]->setText(3, "");
		card_slots[i]->setText(4, "");
	}
	pmEmptyIcon = QPixmap(16, 16);
	pmEmptyIcon.fill(Qt::transparent);

	box->addWidget(listview);
}

void CardSlots::changeIcon(int num, QPixmap pixmap)
{
	card_slots[num]->setText(1,"");
	card_slots[num]->setIcon(1,QIcon(pixmap));
}

void CardSlots::changeText(int num, QString string)
{
	card_slots[num]->setText(2,string);
}

void CardSlots::changePcode(int num, QString pcode)
{
	card_slots[num]->setText(4,pcode);
}

void CardSlots::changeGameID(int num, QString gameID)
{
	card_slots[num]->setText(3,gameID);
}

void CardSlots::changeCountry(int num, int cnum)
{
	// Not very useful so not implemented yet...
}

void CardSlots::clearAll()
{
	for (int i=0; i<15; i++)
		clear(i);
}

void CardSlots::clear(int num)
{
	changeIcon(num, pmEmptyIcon);
	changeText(num, "Free");
}

int CardSlots::get_selected_slot()
{
	for (int i=0; i<15; i++)
	{
		if (card_slots[i]->isSelected()) { return i; }
	}
	return -1;  //no slot is selected
}
