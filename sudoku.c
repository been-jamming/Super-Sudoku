#define USE_TI89
#define SAVE_SCREEN
#include <tigcclib.h>
#include "sudoku2.h"
#include "extgraph.h"

/* 
 * Super Sudoku
 * for Ti89
 *
 * by Ben Jones
 * 1/20/2019
 */

void *kbq;
INT_HANDLER old_int_5;
unsigned int old_delay;

volatile unsigned char escape;
volatile unsigned char puzzling;
volatile unsigned char entering;
volatile unsigned char loading;
volatile unsigned char load_count;
volatile unsigned char last_load_count;
volatile int select_x;
volatile int select_y;
board *global_board;

char str_buffer[256];

unsigned short selection[9] = {
	0x7FC0,
	0x4040,
	0x4040,
	0x4040,
	0x4040,
	0x4040,
	0x4040,
	0x4040,
	0x7FC0
};

unsigned char count_bits(unsigned int x){
	x = ((x&0xAAAA)>>1) + (x&0x5555);
	x = ((x&0xCCCC)>>2) + (x&0x3333);
	x = ((x&0xF0F0)>>4) + (x&0x0F0F);
	x = ((x&0xFF00)>>8) + (x&0x00FF);

	return x;
}

unsigned char count_zeros(unsigned int x){
	unsigned char sum;

	sum = 0;

	if(!(x&0xFF)){
		sum += 8;
		x >>= 8;
	}
	if(!(x&0xF)){
		sum += 4;
		x >>= 4;
	}
	if(!(x&0x3)){
		sum += 2;
		x >>= 2;
	}
	if(!(x&0x1)){
		sum++;
	}
	return sum;
}

coord_queue *create_coord_queue(unsigned char x, unsigned char y){
	coord_queue *output;

	output = malloc(sizeof(coord_queue));
	output->x = x;
	output->y = y;
	output->next = (coord_queue *) 0;
	output->previous = (coord_queue *) 0;

	return output;
}

board *create_board(){
	unsigned char x;
	unsigned char y;
	unsigned int r;
	coord coords[81];
	coord c_temp;
	coord *c_pointer;
	board *output;
	coord_queue *c;

	c_pointer = coords;
	for(x = 0; x < 9; x++){
		for(y = 0; y < 9; y++){
			c_pointer->x = x;
			c_pointer->y = y;
			c_pointer++;
		}
	}
	for(x = 0; x < 81; x++){
		r = rand()%(81 - x) + x;
		c_temp = coords[r];
		coords[r] = coords[x];
		coords[x] = c_temp;
	}

	output = malloc(sizeof(board));
	output->next_square = (coord_queue *) 0;
	c_pointer = coords;
	for(x = 0; x < 9; x++){
		output->taken_row[x] = 0x1FF;
		output->taken_column[x] = 0x1FF;
		output->taken_section[x] = 0x1FF;
		for(y = 0; y < 9; y++){
			output->values[x][y] = 0;
			output->notes[x][y] = 0;
			c = create_coord_queue(c_pointer->x, c_pointer->y);
			c->next = output->next_square;
			if(c->next){
				c->next->previous = c;
			}
			output->next_square = c;
			c_pointer++;
		}
	}
	return output;
}

void free_board(board *b){
	coord_queue *next_square;
	coord_queue *temp;
	
	next_square = b->next_square;
	while(next_square){
		temp = next_square->next;
		free(next_square);
		next_square = temp;
	}
	free(b);
}

void set_value(board *b, unsigned char x, unsigned char y, unsigned char value){
	b->values[x][y] = value;
	b->taken_row[y] &= ~(1U<<(value - 1));
	b->taken_column[x] &= ~(1U<<(value - 1));
	b->taken_section[x/3 + y/3*3] &= ~(1U<<(value - 1));
}

void delete_value(board *b, unsigned char x, unsigned char y){
	b->taken_row[y] |= 1U<<(b->values[x][y] - 1);
	b->taken_column[x] |= 1U<<(b->values[x][y] - 1);
	b->taken_section[x/3 + y/3*3] |= 1U<<(b->values[x][y] - 1);
	b->values[x][y] = 0;
}

unsigned char possible(board *b, unsigned char x, unsigned char y, unsigned char value){
	return !b->values[x][y] && (b->taken_row[y]&(1U<<(value - 1))) && (b->taken_column[x]&(1U<<(value - 1))) && (b->taken_section[x/3 + y/3*3]&(1U<<(value - 1)));
}

unsigned char fill_board(board *b, unsigned char x, unsigned char y, unsigned char value, unsigned char depth){
	unsigned char i;
	unsigned int r1;
	unsigned char x2;
	unsigned char y2;
	unsigned char v;
	unsigned int taken_row;
	unsigned int taken_column;
	unsigned int taken_section;

	taken_row = b->taken_row[y];
	taken_column = b->taken_column[x];
	taken_section = b->taken_section[x/3 + y/3*3];
	set_value(b, x, y, value);
	b->true_values[x][y] = value;

	if(depth >= 80){
		return 1;
	}

	r1 = rand();
	if(x < 8){
		x2 = x + 1;
		y2 = y;
	} else {
		x2 = 0;
		y2 = y + 1;
	}
	for(i = 0; i < 9; i++){
		v = (i + r1)%9 + 1;
		if(possible(b, x2, y2, v) && fill_board(b, x2, y2, v, depth + 1)){
			return 1;
		}
	}
	b->taken_row[y] = taken_row;
	b->taken_column[x] = taken_column;
	b->taken_section[x/3 + y/3*3] = taken_section;
	b->values[x][y] = 0;
	b->true_values[x][y] = 0;
	return 0;
}

unsigned char _solvable(board *b, unsigned char x, unsigned char y, unsigned char value, unsigned char depth, unsigned char reset){
	unsigned char i;
	unsigned char j;
	unsigned int r1;
	unsigned char x2;
	unsigned char y2;
	unsigned char v;
	unsigned char output;
	unsigned int taken;
	unsigned int num_bits;
	unsigned int taken_row;
	unsigned int taken_column;
	unsigned int taken_section;

	if(depth >= 80 || escape){
		return 1;
	}

	taken_row = b->taken_row[y];
	taken_column = b->taken_column[x];
	taken_section = b->taken_section[x/3 + y/3*3];
	set_value(b, x, y, value);
	for(i = 0; i < 9; i++){
		if(b->values[i][y]){
			continue;
		}
		taken = taken_row & b->taken_column[i] & b->taken_section[i/3 + y/3*3];
		num_bits = count_bits(taken);
		if(num_bits == 1){
			if(_solvable(b, i, y, count_zeros(taken) + 1, depth + 1, 1)){
				output = 1;
			} else {
				output = 0;
			}
			b->taken_row[y] = taken_row;
			b->taken_column[x] = taken_column;
			b->taken_section[x/3 + y/3*3] = taken_section;
			b->values[x][y] = 0;
			return output;
		} else if(num_bits == 0){
			b->taken_row[y] = taken_row;
			b->taken_column[x] = taken_column;
			b->taken_section[x/3 + y/3*3] = taken_section;
			b->values[x][y] = 0;
			return 0;
		}
	}
	for(i = 0; i < 9; i++){
		if(b->values[x][i]){
			continue;
		}
		taken = b->taken_row[i] & taken_column & b->taken_section[x/3 + i/3*3];
		num_bits = count_bits(taken);
		if(num_bits == 1){
			if(_solvable(b, x, i, count_zeros(taken) + 1, depth + 1, 1)){
				output = 1;
			} else {
				output = 0;
			}
			b->taken_row[y] = taken_row;
			b->taken_column[x] = taken_column;
			b->taken_section[x/3 + y/3*3] = taken_section;
			b->values[x][y] = 0;
			return output;
		} else if(num_bits == 0){
			b->taken_row[y] = taken_row;
			b->taken_column[x] = taken_column;
			b->taken_section[x/3 + y/3*3] = taken_section;
			b->values[x][y] = 0;
			return 0;
		}
	}
	for(i = 0; i < 3; i++){
		for(j = 0; j < 3; j++){
			if(b->values[x/3*3 + i][y/3*3 + j]){
				continue;
			}
			taken = b->taken_row[y/3*3 + j] & b->taken_column[x/3*3 + i] & taken_section;
			num_bits = count_bits(taken);
			if(num_bits == 1){
				if(_solvable(b, x/3*3 + i, y/3*3 + j, count_zeros(taken) + 1, depth + 1, 1)){
					output = 1;
				} else {
					output = 0;
				}
				b->taken_row[y] = taken_row;
				b->taken_column[x] = taken_column;
				b->taken_section[x/3 + y/3*3] = taken_section;
				b->values[x][y] = 0;
				return output;
			} else if(num_bits == 0){
				b->taken_row[y] = taken_row;
				b->taken_column[x] = taken_column;
				b->taken_section[x/3 + y/3*3] = taken_section;
				b->values[x][y] = 0;
				return 0;
			}
		}
	}

	r1 = rand();

	if(reset){
		x2 = 0;
		y2 = 0;
	} else {
		if(x < 8){
			x2 = x + 1;
			y2 = y;
		} else {
			x2 = 0;
			y2 = y + 1;
		}
	}
	while(b->values[x2][y2]){
		if(x2 < 8){
			x2 = x2 + 1;
		} else {
			x2 = 0;
			y2 = y2 + 1;
		}
	}

	for(i = 0; i < 10; i++){
		if(!i){
			v = b->true_values[x2][y2];
		} else {
			v = (i - 1 + r1)%9 + 1;
			if(v == b->true_values[x2][y2]){
				continue;
			}
		}
		if(possible(b, x2, y2, v) && _solvable(b, x2, y2, v, depth + 1, 0)){
			b->taken_row[y] = taken_row;
			b->taken_column[x] = taken_column;
			b->taken_section[x/3 + y/3*3] = taken_section;
			b->values[x][y] = 0;
			return 1;
		}
	}
	b->taken_row[y] = taken_row;
	b->taken_column[x] = taken_column;
	b->taken_section[x/3 + y/3*3] = taken_section;
	b->values[x][y] = 0;
	return 0;
}

unsigned char solvable(board *b, unsigned char depth){
	unsigned char i;
	unsigned char x;
	unsigned char y;
	unsigned char output;

	x = 0;
	y = 0;
	while(b->values[x][y]){
		if(x < 8){
			x = x + 1;
		} else {
			x = 0;
			y = y + 1;
		}
	}

	output = 0;
	if(possible(b, x, y, b->true_values[x][y]) && _solvable(b, x, y, b->true_values[x][y], depth + 1, 0)){
		output = 1;
	} else {
		for(i = 1; i < 10; i++){
			if(i == b->true_values[x][y]){
				continue;
			}
			if(possible(b, x, y, i) && _solvable(b, x, y, i, depth + 1, 0)){
				output = 1;
				break;
			}
		}
	}

	return output;
}

void generate_puzzle(board *b){
	unsigned char value;
	unsigned char x;
	unsigned char y;
	unsigned char i;
	coord_queue *next_square;
	unsigned char num_squares;
	unsigned char count;

	next_square = b->next_square;
	num_squares = 81;
	count = 0;
	while(next_square && !escape){
		x = next_square->x;
		y = next_square->y;
		value = b->values[x][y];
		delete_value(b, x, y);
		num_squares--;
		for(i = 1; i < 10; i++){
			if(i != value){
				if(possible(b, x, y, i)){
					set_value(b, x, y, i);
					if(solvable(b, num_squares)){
						delete_value(b, x, y);
						set_value(b, x, y, value);
						num_squares++;
						break;
					} else {
						delete_value(b, x, y);
					}
				}
			}
		}
		next_square = next_square->next;
		load_count++;
	}
}

void mark_original(board *b){
	unsigned char x;
	unsigned char y;
	
	for(x = 0; x < 9; x++){
		for(y = 0; y < 9; y++){
			if(b->values[x][y]){
				b->original[x][y] = 1;
			} else {
				b->original[x][y] = 0;
			}
		}
	}
}

void display_board(board *b){
	unsigned char i;
	unsigned char j;
	FastFillRect(LCD_MEM, 36, 11, 124, 99, A_NORMAL);
	for(i = 0; i < 2; i++){
		FastDrawVLine(LCD_MEM, 30*i + 65, 11, 99, A_REVERSE);
		FastDrawHLine(LCD_MEM, 36, 124, 30*i + 40, A_REVERSE);
	}
	for(i = 0; i < 9; i++){
		for(j = 0; j < 9; j++){
			if(b->values[i][j]){
				sprintf(str_buffer, "%d", (int) b->values[i][j]);
				DrawStr(i*10 + 37, j*10 + 12, str_buffer, A_REVERSE);
			}
		}
	}
}

void display_message(){
	FastFillRect(LCD_MEM, 0, 0, 159, 10, A_REVERSE);
	DrawStr(0, 0, str_buffer, A_NORMAL);
}

void display_notes(){
	unsigned int notes;
	char chars[10];
	unsigned char i;
	unsigned int mask;

	notes = global_board->notes[select_x][select_y];
	mask = 1;
	for(i = 0; i < 9; i++){
		if(mask&notes){
			chars[i] = '1' + i;
		} else {
			chars[i] = ' ';
		}
		mask <<= 1;
	}

	chars[9] = (char) 0;
	sprintf(str_buffer, "Notes: %s", chars);
	display_message();
}

void display_selection(){
	Sprite16_XOR(select_x*10 + 35, select_y*10 + 11, 9, selection, LCD_MEM);
}

DEFINE_INT_HANDLER (update){
	unsigned int key;

	while(!OSdequeue(&key, kbq)){
		if(key == KEY_ESC){
			escape = 1;
		}
		if(puzzling && !entering){
			if(key == KEY_LEFT){
				display_selection();
				if(select_x > 0){
					select_x--;
				} else {
					select_x = 8;
				}
				display_selection();
				display_notes();
			} else if(key == KEY_RIGHT){
				display_selection();
				select_x = (select_x + 1)%9;
				display_selection();
				display_notes();
			} else if(key == KEY_UP){
				display_selection();
				if(select_y > 0){
					select_y--;
				} else {
					select_y = 8;
				}
				display_selection();
				display_notes();
			} else if(key == KEY_DOWN){
				display_selection();
				select_y = (select_y + 1)%9;
				display_selection();
				display_notes();
			} else if(key == KEY_ENTER){
				if(global_board->original[select_x][select_y]){
					sprintf(str_buffer, "Can't modify");
					display_message();
					ngetchx();
					display_notes();
				} else {
					entering = 1;
				}
			} else if(key >= (unsigned int) '1' && key <= (unsigned int) '9'){
				global_board->notes[select_x][select_y] ^= 1<<(key - '1');
				display_notes();
			}
		} else if(entering){
			entering = 0;
			if(key >= (unsigned int) '0' && key <= (unsigned int) '9'){
				global_board->values[select_x][select_y] = key - (unsigned int) '0';
				display_board(global_board);
				display_selection();
			}
		}
	}
	if(loading){
		if(last_load_count != load_count){
			clrscr();
			printf("Loading: %d%%\n",(((int) load_count)*100/81));
			last_load_count = load_count;
		}
	}
}

void _main(){
	escape = 0;
	puzzling = 0;
	entering = 0;
	loading = 0;
	load_count = 0;
	last_load_count = 0;
	select_x = 0;
	select_y = 0;
	clrscr();
	randomize();
	kbq = kbd_queue();
	old_delay = OSInitKeyInitDelay(0);
	old_int_5 = GetIntVec(AUTO_INT_5);
	SetIntVec(AUTO_INT_5, update);

	printf("Sudoku puzzle\ngenerator\n\nPress any key");
	ngetchx();
	global_board = create_board();
	fill_board(global_board, 0, 0, rand()%9 + 1, 0);
	loading = 1;
	generate_puzzle(global_board);
	mark_original(global_board);
	loading = 0;
	clrscr();
	display_board(global_board);
	display_selection();
	display_notes();
	puzzling = 1;
	
	while(!escape){
	//pass
	}

	free_board(global_board);
	SetIntVec(AUTO_INT_5, old_int_5);
	OSInitKeyInitDelay(old_delay);
}
