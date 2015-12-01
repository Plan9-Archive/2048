#include <u.h>
#include <libc.h>
#include <draw.h>
#include <keyboard.h>
#include <event.h>

/* 2048
 * y ↓ x → */

enum
{
	BORDER = 4,
	BOARDSIZE = 4,
};

typedef struct Tile Tile;

struct Tile {
	int value;
	Image *color;
};

int bsize;
Tile grid[BOARDSIZE][BOARDSIZE];
Tile *grid1 = &grid[0][0];
int score = 0;
int gameover = 0;

Font *bigfont;

Image *colors[13];

Image *
val2col(int val)
{
	int i;

	if(val == 0)
		return colors[0];

	for(i = 32; (val & (1<<i)) == 0; i--)
		;

	if(i < nelem(colors))
		return colors[i];

	return colors[nelem(colors)-1];
}


void
initcolors(void)
{
	int i;

	int cols[] = {
		0xcdc0b4ff, 0xeee4daff, 0xede0c8ff, 0xf2b179ff, 0xf59563ff,
		0xf67c5fff, 0xf65e36ff, 0xedcf72ff, 0xedcc61ff,
		0xedc850ff, 0xedc53fff, 0xedc22eff, 0x3c3a32ff };

	for(i = 0; i < nelem(cols); i++) {
		colors[i] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, cols[i]);
	}
}

void
initboard(void)
{
	int x, y, x2, y2;

	for(y = 0; y < BOARDSIZE; y++) {
		for(x = 0; x < BOARDSIZE; x++) {
			grid[y][x].value = 0;
			grid[y][x].color = val2col(0);
		}
	}

	y = nrand(BOARDSIZE);
	x = nrand(BOARDSIZE);

	grid[y][x].value = 2;
	grid[y][x].color = val2col(2);

	y2 = nrand(BOARDSIZE);
	x2 = nrand(BOARDSIZE);

	while(y == y2 || x == x2) {
		y2 = nrand(BOARDSIZE);
		x2 = nrand(BOARDSIZE);
	}

	grid[y2][x2].value = 2;
	grid[y2][x2].color = val2col(2);

	score = 0;
}

void
redraw(void)
{
	Rectangle r;
	Point p, sp;
	int x, y; // grid indexes
	int width, height; // of each tile
	char buf[64];

	Point strp;
	int strwid;

	r = insetrect(screen->r, BORDER);
	sp = r.min;
	r.min.y += font->height;
	p = r.min;
	
	width = Dx(r) / BOARDSIZE;
	height = Dy(r) / BOARDSIZE;

	draw(screen, screen->r, display->black, nil, ZP);

	snprint(buf, sizeof buf, "Score: %d", score);
	string(screen, sp, display->white, ZP, font, buf);

	for(y = 0; y < BOARDSIZE; y++) {
		for(x = 0; x < BOARDSIZE; x++) {
			r = Rect(p.x + (x * width), p.y + (y * height), p.x + (x * width) + width, p.y + (y * height) + height);
			r = insetrect(r, BORDER);
			draw(screen, r, grid[y][x].color, nil, ZP);

			if(grid[y][x].value == 0)
				continue;

			snprint(buf, sizeof buf, "%d", grid[y][x].value);
			strwid = stringwidth(bigfont, buf);
			strp = Pt(Dx(r)/2, Dy(r)/2);
			strp.x -= strwid / 2;
			string(screen, addpt(r.min, strp), display->black, ZP, bigfont, buf);
		}
	}
}

void
addrandom(void)
{
	int x, y, nfree;
	Tile **tfree;

	nfree = 0;

	for(x = 0; x < BOARDSIZE; x++) {
		for(y = 0; y < BOARDSIZE; y++) {
			if(grid[x][y].value == 0)
				nfree++;
		}
	}

	if(nfree > 0) {
		tfree = mallocz(sizeof(Tile*) * nfree, 1);
		nfree = 0;
		for(x = 0; x < BOARDSIZE; x++) {
			for(y = 0; y < BOARDSIZE; y++) {
				if(grid[x][y].value == 0)
					tfree[nfree++] = &grid[x][y];
			}
		}

		// 10% chance of 4.
		if(nrand(10) == 0)
			x = 2;
		else
			x = 2;

		nfree = nrand(nfree);

		tfree[nfree]->value = x;
		tfree[nfree]->color = val2col(x);

		free(tfree);
	}
}

void
buildrow(Tile *out[BOARDSIZE], int n, int dir)
{
	int i, start, add;
	add = start = 0;

	switch(dir) {
	case Kright:
		add = 1;
		start = n*BOARDSIZE;
		break;
	case Kleft:
		add = -1;
		start = n*BOARDSIZE + (BOARDSIZE-1);
		break;
	case Kdown:
		add = BOARDSIZE;
		start = n;
		break;
	case Kup:
		add = -BOARDSIZE;
		start = (BOARDSIZE-1)*BOARDSIZE + n;
		break;
	}

	for(i = 0; i < BOARDSIZE; i++) {
		out[i] = &grid1[add*i + start];
	}
}

int
merge(Tile *row[BOARDSIZE])
{
	int i, j, moved;
	Tile nrow[BOARDSIZE];

	for(i = 0; i < BOARDSIZE; i++) {
		nrow[i].value = 0;
		nrow[i].color = val2col(0);
	}

	j = BOARDSIZE-1;
	for(i = j; i >= 0; i--) {
		if(row[i]->value == 0) {
			;
		} else if(nrow[j].value == 0) {
			nrow[j] = *row[i];
		} else if(nrow[j].value == row[i]->value) {
			score += nrow[j].value *= 2;
			nrow[j].color = val2col(nrow[j].value);
			j--;
		} else {
			nrow[--j] = *row[i];
		}
	}

	moved = 0;
	for(i = 0; i < BOARDSIZE; i++) {
		if(nrow[i].value != row[i]->value)
			moved = 1;

		*row[i] = nrow[i];
	}

	return moved;
}

void
shift(int dir)
{
	Tile *cur[BOARDSIZE];
	int i;
	int moved = 0;

	for(i = 0; i < BOARDSIZE; i++) {
		//print("%d\n", i);
		buildrow(cur, i, dir);
		moved = merge(cur) || moved;
	}

	if(moved) {
		addrandom();
		// check gameover here
	}
}

void
main(int argc, char **argv)
{
	char *fontname;

	USED(argc);
	USED(argv);

	if(initdraw(0, 0, "2048") < 0)
		sysfatal("initdraw: %r");

	fontname = "/lib/font/bit/fixed/unicode.10x20.font";
	if((bigfont = openfont(display, fontname)) == nil)
		sysfatal("can't open font %s: %r", fontname);

	einit(Ekeyboard|Emouse);
	srand(time(0));

	initcolors();
	initboard();

	redraw();

	while(1) {
		int key;
		redraw();
		flushimage(display, 1);

		switch(key = ekbd()) {
		case 0x7f: // p9p lacks Kdel
		case 'q':
			// quit
			exits(0);
			break;
		case ' ':
			// reset board
			initboard();
			break;
		case Kup:
		case Kdown:
		case Kleft:
		case Kright:
			shift(key);
			break;
		}
	}
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");

	redraw();
}
