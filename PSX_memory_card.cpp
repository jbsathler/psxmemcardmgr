#include <qstring.h>
#include <qpixmap.h>
#include <qfile.h>
#include <qimage.h>
#include <qcolor.h>
#include "PSX_memory_card.h"
#include "card_link.h"
#include <iconv.h>

#define PSX_DIRECTORY_FREE 0xA0
#define PSX_DIRECTORY_BUSY 0x50
#define PSX_DIRECTORY_RESERVED 0xF0
#define PSX_BLOCK_NOT_USED 0x00
#define PSX_BLOCK_TOP 0x01
#define PSX_BLOCK_LINK 0x02
#define PSX_BLOCK_LINK_END 0x03

PSX_memory_card::PSX_memory_card()
{
	sjisToUtf8 = iconv_open("UTF-8", "SHIFT_JIS");

	// Initialise the card contents
	memset(memoryCard, 0, sizeof(memoryCard));

	// Create image buffer for icons
	for (int i=0; i<15; i++)
	{
		slot_icons[i] = new QImage(16, 16, QImage::Format_ARGB32);
		slot_icons[i]->setColorCount(16);
		slot_icons[i]->fill(0);
	}

	// New empty card
	load_file("empty.mc");
}

PSX_memory_card::~PSX_memory_card()
{
	for (int i=0; i<15; i++)
	{
		delete slot_icons[i];
	}

	iconv_close(sjisToUtf8);
}

int PSX_memory_card::load_file(QString filename)
{
	QFile f(filename);
	f.open( QIODevice::ReadOnly );                      // index set to 0
	if (f.size()<131072) { return 0; } // the file is too small...
	if (f.size()==134976)
	{
		// This must be a DexDrive file.
		f.seek(3904);  // skip dexdrive header, and go to the memory card header it's self.
		qDebug("Assuming dexDrive format");
	}
	else
	{
		qDebug("Reading raw memory card image");
	}
	f.read(memoryCard,131072);
	f.close();
	// Now, lets fill up data
	update();
	return 0;
}

int PSX_memory_card::save_file(QString filename)
{
	QFile f(filename);
	f.open( QIODevice::WriteOnly );
	f.write(memoryCard,131072);
	f.close();
	return 0;
}

int PSX_memory_card::load_card()
{
	char *block_data;
	for (int i=0; i<16; i++)
	{
		qDebug("Reading block %d",i);
		block_data=read_block(i);

		for (int j=0; j<8192; j++)
		{
			memoryCard[(8192*i)+j]=block_data[j];

		}
	}
	update();
	return 0;
}

int PSX_memory_card::save_card()
{
	return 0;
}

int PSX_memory_card::load_card_block(int block)
{
	char *block_data;

	block_data=read_block(block);
    for (int j=0; j<8192; j++)
	{
		memoryCard[(8192*block)+j]=block_data[j];
    }

	if (block==15) { update(); }
	return 0;
}

int PSX_memory_card::save_card_block(int block)
{
	return 0;
}

int PSX_memory_card::load_card_frame(int frame)
{
	char *block_data;

	block_data=read_frame(frame);

	if (block_data==NULL) { return 0; }
    for (int j=0; j<128; j++)
	{
		memoryCard[(128*frame)+j]=block_data[j];
    }
    return 0;
}

int PSX_memory_card::save_card_frame(int frame)
{
	char frame_data[128];

	int position;
	position=frame*128;
	for (int j=0; j<128; j++)
	{
		frame_data[j]=memoryCard[j+position];
	}
	if (write_frame(frame, frame_data)==-1)
	{
		return 0;  // an error occured
	}
	return 1; // everything went well
}

void PSX_memory_card::update_data()
{
	update();
}

int PSX_memory_card::save_single_game(QString filename, int src_slot)
{
	char directory_entry[128];
	char save_data[8192];
	int position;
	qDebug("Saving slot: %d",src_slot);
	// Read the directory entry in the directory block
	position=0x80+(src_slot*0x80);
	for (int i=0; i<0x80; i++)
	{
		directory_entry[i]=memoryCard[i+position];
	}

	// Read the save data
	position=0x2000+(src_slot*0x2000);
	for (int i=0; i<0x2000; i++)
	{
		save_data[i]=memoryCard[i+position];
	}

	// Open the file and write the data
	QFile f(filename);
	f.open( QIODevice::WriteOnly);
	f.write(directory_entry, 128);
	f.write(save_data, 8192);
	f.close();

	return 0;
}

int PSX_memory_card::load_single_game(QString filename, int dest_slot)
{
	char directory_entry[128];
	char save_data[8192];
	int position;
	qDebug("Loading slot: %d",dest_slot);

		// Open the file and read the data
	QFile f(filename);
	f.open(QIODevice::ReadOnly);
	f.read(directory_entry, 128);
	f.read(save_data, 8192);
	f.close();

	// Read the directory entry in the directory block
	position=0x80+(dest_slot*0x80);
	for (int i=0; i<0x80; i++)
	{
		memoryCard[i+position]=directory_entry[i];
	}

	// Read the save data
	position=0x2000+(dest_slot*0x2000);
	for (int i=0; i<0x2000; i++)
	{
		memoryCard[i+position]=save_data[i];
	}
	update();

	return 0;
}

int PSX_memory_card::delete_slot(int slot)
{
	int position;
    position=0x80+(slot*0x80);
	// delete the save
	memoryCard[position]=(memoryCard[position]|0xF0)^0xF0^PSX_DIRECTORY_FREE;

	// xor code
	unsigned char xor_code=0x00;
	position=0x80+(slot*0x80);  // get to the start of the frame
	for (int j=0; j<126; j++)
	{
		xor_code = xor_code ^ memoryCard[j+position];
	}
	memoryCard[position + 127] = xor_code;
	//memoryCard[position + 127] = 0xFF;

	update();

	return 0;
}

int PSX_memory_card::undelete_slot(int slot)
{
	int position;
    position=0x80+(slot*0x80);
	// delete the save
	memoryCard[position]=(memoryCard[position]|0xF0)^0xF0^PSX_DIRECTORY_BUSY;

	// xor code
	unsigned char xor_code=0x00;
	position=0x80+(slot*0x80);  // get to the start of the frame
	for (int j=0; j<126; j++)
	{
		xor_code = xor_code ^ memoryCard[j+position];
	}
	memoryCard[position + 127] = xor_code;
	//memoryCard[position + 127] = 0xFF;

	update();

	return 0;
}

int PSX_memory_card::clearData()
{
	for (int i=0; i<131072; i++)
	{
		memoryCard[i]=0;
	}
	load_file("empty.mc");
	//update();
	return 0;
}

void PSX_memory_card::update()
{
	// order is important here.
	update_slot_is_used();    // Verify if it is used or not
	update_slot_is_deleted(); // is it a deleted slot(which can be undeleted)?
	update_slot_has_icon();   // Verify if slot has icon
	update_slot_Pcodes();     // Read the product code
	update_slot_titles();     // Read the title
	update_slot_iconImages(); // Retreive icons
	update_slot_gameIDs();    // Read Game Id's
}

void PSX_memory_card::update_slot_is_used()
{
	int current_pos=128;
	for (int i=0; i<15; i++)
	{
		if ( (memoryCard[current_pos] & PSX_DIRECTORY_BUSY)==PSX_DIRECTORY_BUSY)
		{
			slot_is_used[i]=true;

			// Code to update block_type
			if ( (memoryCard[current_pos] & PSX_BLOCK_NOT_USED)==PSX_BLOCK_NOT_USED)
			{
				block_type[i]=PSX_BLOCK_NOT_USED;
			}
            if ( (memoryCard[current_pos] & PSX_BLOCK_TOP)==PSX_BLOCK_TOP)
			{
				block_type[i]=PSX_BLOCK_TOP;
			}
            if ( (memoryCard[current_pos] & PSX_BLOCK_LINK_END)==PSX_BLOCK_LINK)
			{
				block_type[i]=PSX_BLOCK_LINK;
			}
			if ( (memoryCard[current_pos] & PSX_BLOCK_LINK_END)==PSX_BLOCK_LINK_END)
			{
				block_type[i]=PSX_BLOCK_LINK_END;
			}
		}
		else
		{
			//qDebug( "Slot %d is unused", i);
			slot_is_used[i]=false;
			block_type[i]=PSX_BLOCK_NOT_USED;
		}
		current_pos+=128;
	}
}

void PSX_memory_card::update_slot_is_deleted()
{
	int position=128;
	for (int i=0; i<15; i++)
	{
		// If the directory is free but the block still starts with SC, then the block may be undeleted
		if (( memoryCard[position] & PSX_DIRECTORY_FREE)==PSX_DIRECTORY_FREE)
		{
			if ((memoryCard[0x2000+(i*0x2000)]=='S')&&(memoryCard[0x2000+(i*0x2000)+1]=='C'))
			{
				slot_is_deleted[i]=true;
				qDebug("1");
			}
			else
			{
				slot_is_deleted[i]=false;
				qDebug("2");
			}
		}
		else
		{
			slot_is_deleted[i]=false;
			qDebug("3");
		}
		position+=128;
	}
}

void PSX_memory_card::update_slot_has_icon()
{
	for (int i=0; i<15; i++)
	{
		if ((block_type[i]==PSX_BLOCK_TOP)||slot_is_deleted[i])
		{
			slot_has_icon[i]=true;
		}
		else
		{
			slot_has_icon[i]=false;
		}
	}
}


void PSX_memory_card::update_slot_Pcodes()
{
	int current_pos=128;
	int pcode_pos=0;
	int char_count=0;
	for (int i=0; i<15; i++)
	{
		if (slot_is_used[i]||slot_is_deleted[i])
		{
			pcode_pos=current_pos+12; // The product code is the 12th byte
			char_count=0;
			slot_Pcodes[i]="";
			while ((memoryCard[pcode_pos]!=0)&(char_count<10))
			{
				slot_Pcodes[i]+=memoryCard[pcode_pos];
				pcode_pos++;
                char_count++;
			}
			//qDebug("Slot %d pcode is: %s",i, slot_Pcodes[i].latin1());
		}
		else
		{
			slot_Pcodes[i]="";
		}
		current_pos+=128;
	}
}

void PSX_memory_card::update_slot_gameIDs()
{
	int current_pos=128;
	int pcode_pos=0;
	int char_count=0;
	for (int i=0; i<15; i++)
	{
		if (slot_is_used[i]||slot_is_deleted[i])
		{
			pcode_pos=current_pos+22; // The game ID is the 22th byte
			char_count=0;
			slot_gameID[i]="";
			while ((memoryCard[pcode_pos]!=0))
			{
				slot_gameID[i]+=memoryCard[pcode_pos];
				pcode_pos++;
                char_count++;
			}
			//qDebug("Slot %d pcode is: %s",i, slot_gameID[i].latin1());
		}
		else
		{
			slot_gameID[i]="";
		}
		current_pos+=128;
	}
}

void PSX_memory_card::update_slot_titles()
{
	int current_pos=0x2000;  // The second block starts here

	for (int i=0; i<15; i++)
	{
		if (slot_is_used[i]||slot_is_deleted[i])
		{
			if ((block_type[i]==PSX_BLOCK_TOP)||slot_is_deleted[i])
			{
				char tmpbuf[64] = { };
				size_t olen = sizeof(tmpbuf)-1;
				char *o = tmpbuf;
				char *jis_title;
				size_t tlen;

				slot_titles[i]="";

				//  04h-43h  Title in Shift-JIS format (64 bytes = max 32 characters)
				jis_title = (char*)&memoryCard[current_pos + 4];
				tlen = strnlen(jis_title, 64);

				iconv(sjisToUtf8, &jis_title, &tlen, &o, &olen);

				slot_titles[i] += QString::fromUtf8(tmpbuf);
				if (slot_is_deleted[i])
					slot_titles[i]= "(" + slot_titles[i] + ")";

			}   // endif block_type
			else
			{
				if (block_type[i]==PSX_BLOCK_LINK) { slot_titles[i]="Link Block"; }
				if (block_type[i]==PSX_BLOCK_LINK_END) { slot_titles[i]="Link end Block"; }
			}
				//qDebug("Slot %d pcode is: %s",i, slot_titles[i].latin1());
		}
		else
		{
			slot_titles[i]="Free";
		}
		current_pos+=0x2000;
	}
}

void PSX_memory_card::update_slot_iconImages()
{
	int icn_pos=0;
	QRgb palette[16];
	for (int i=0; i<15; i++)
	{
		int paladdr=0x2060+(i*0x2000);
		int dataaddr=0x2080+(i*0x2000);

		if (slot_has_icon[i])
		{

			for (int p=0; p<16; p++)
			{
				unsigned char pp;
				unsigned char red, green, blue;


				// Calculate blue component
				pp = memoryCard[paladdr+(p*2)+1];
				//blue = (pp|0xE0) ^ 0xE0;
				blue = pp>>2;

				pp = memoryCard[paladdr+(p*2)];
				green = ((pp>>5)|0xF8) ^ 0xF8;
				pp = memoryCard[paladdr+(p*2)+1];
				green = green + ( ((pp|0xFC)^0xFC)<<3 );

				// Calculate blue component
				pp = memoryCard[paladdr+(p*2)];
				//red = ((pp|0x83) ^ 0x83);
				red = (pp|0xE0)^0xE0;

				//if (i==0) { qDebug("Color %d: R:%d  G:%d  B:%d",p,red,green,blue); }
				//if (i==0) { qDebug("Color %d: R:%d  G:%d  B:%d",p,red*8,green*8,blue*8); }
				if (slot_is_deleted[i])
				{
					palette[p] = qRgb((red*4+127),(green*4+127),(blue*4+127));
				}
				else
				{
					palette[p] = qRgb(red*8,green*8,blue*8);
				}

			}

			icn_pos=0;

			for (int y=0; y<16; y++)
			{
	        	for (int x=0; x<16; x+=2)
				{
					unsigned char index;

					index = (memoryCard[dataaddr+icn_pos]|0xF0)^0xF0;
					//if (i==0) { qDebug("%d", index); }
					slot_icons[i]->setPixel(x, y, palette[index]);
					index = (((memoryCard[dataaddr+icn_pos])>>4)|0xF0)^0xF0;
                	slot_icons[i]->setPixel(x+1, y, palette[index]);
					icn_pos+=1;
				}
			}
		}
		else
		{

			for (int y=0; y<16; y++)
			{
				for (int x=0; x<16; x++)
				{
					slot_icons[i]->setPixel(x, y, qRgba(0,0,0,127));
				}
			}
		}
	}
}

QString PSX_memory_card::get_slot_Pcode(int slot)
{
	return slot_Pcodes[slot];
}

QString PSX_memory_card::get_slot_title(int slot)
{
	return slot_titles[slot];
}

QString PSX_memory_card::get_slot_gameID(int slot)
{
	return slot_gameID[slot];
}

bool PSX_memory_card::get_slot_is_used(int slot)
{
	return slot_is_used[slot];
}

bool PSX_memory_card::get_slot_is_free(int slot)
{
	if (block_type[slot]==PSX_BLOCK_NOT_USED)
	{
		return true;
	}
	return false;
}

QPixmap PSX_memory_card::get_slot_icon(int slot)
{
	QPixmap pixmap;
	pixmap.convertFromImage(*slot_icons[slot],0);
	//pixmap::convertFromImage(slot_icons[slot],0);
	return pixmap;
}

void PSX_memory_card::set_slot_gameID(int slot, QString newID)
{
	int position;
	int i=0;
	int max_title_length=102; // not counting 0
	const char *id_string = newID.toLatin1().constData();


	position=0x80+(slot*0x80);  // get to the start of the frame
	position+=0x0C;  // go to the product code + Game ID ASCIIZ string
	position+=10; // jump to the Game ID start

	qDebug("New gameID would be written ad: %0X", position);
	qDebug("New gameID would be: %s", id_string);

	// write the new game id
	do
	{
		memoryCard[position+i]=id_string[i];
		i++;
	} while ( (id_string[i]!=0)&&(i<max_title_length) );

	// compute the new XOR code
	unsigned char xor_code=0x00;
	position=0x80+(slot*0x80);  // get to the start of the frame
	for (int j=0; j<126; j++)
	{
		xor_code = xor_code ^ memoryCard[j+position];
	}
	memoryCard[position + 127] = xor_code;
	//memoryCard[position + 127] = 0xFF;

	update();
}

void PSX_memory_card::set_slot_title(int slot, QString newTitle)
{
	// hahaha, ca ne me tente pas....
}

void PSX_memory_card::set_slot_Pcode(int slot, QString newPcode)
{
	int position;
	int i=0;
	int max_title_length=10; // not counting 0
	const char *id_string = newPcode.toLatin1().constData();


	position=0x80+(slot*0x80);  // get to the start of the frame
	position+=0x0C;  // go to the product code + Game ID ASCIIZ string

	qDebug("New gameID would be written ad: %0X", position);
	qDebug("New gameID would be: %s", id_string);

	// write the new game id

	while ( (id_string[i]!=0)&&(i<max_title_length) )
	{
		memoryCard[position+i]=id_string[i];
		i++;
	}

	// compute the new XOR code
	unsigned char xor_code=0x00;
	position=0x80+(slot*0x80);  // get to the start of the frame
	for (int j=0; j<126; j++)
	{
		xor_code = xor_code ^ memoryCard[j+position];
	}
	memoryCard[position + 127] = xor_code;
	//memoryCard[position + 127] = 0xFF;

	update();
}
