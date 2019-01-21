typedef struct coord coord;

struct coord{
	unsigned char x;
	unsigned char y;
};

typedef struct coord_queue coord_queue;

struct coord_queue{
	unsigned char x;
	unsigned char y;
	coord_queue *next;
	coord_queue *previous;
};

typedef struct board board;

struct board{
	unsigned int taken_row[9];
	unsigned int taken_column[9];
	unsigned int taken_section[9];
	unsigned char values[9][9];
	unsigned char true_values[9][9];
	unsigned int notes[9][9];
	unsigned char original[9][9];
	coord_queue *next_square;
};

