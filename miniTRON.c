#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <ncurses.h>
#include <string.h>

#define red 0x00FF0000
#define blue 0x000000FF

#define red_line 0x00FF0001
#define blue_line 0x000001FF

#define sec 62500
#define width 5
#define length 8

#define LEFT_START_X 0
#define LEFT_START_Y (width / 2 + 1)

#define RIGHT_START_X (xres - 1)
#define RIGHT_START_Y (yres - width / 2 - 2)

int work_flag = 1;

void sig_handler(int sig) {
	work_flag = 0;
}

struct car {
	int x, y, lose_flag, l_x, l_y, r_x, r_y;
	uint32_t color, line, direction;
};


void* func_for_thread(void* arg) {
	char* ch = (char*)arg;
	while (work_flag == 1) {
		*ch = getchar();

	}
}

int main(int argc, char const *argv[]) {

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <resolution> <opponent`s IP>\n", argv[0]);
		return 1;
	}
	int a = 0;
	int tie_flag = 0;
	struct car car_1, car_2;
	char ch_1 = 0, ch_2 = 0, ch = 0;
	int prev_1, prev_2;
	car_1.lose_flag = car_2.lose_flag = 0;

	int sd;
	struct sockaddr_in addr;
	pthread_t thread;
	unsigned long int enemy_ip, my_ip;
	int size = sizeof(addr);

	int fb;
  	struct fb_var_screeninfo info;
  	size_t fb_size, map_size, page_size;
  	uint32_t *ptr;
	int xres, yres;
	sscanf(argv[1], "%d%*c%d", &xres, &yres);
  	
  	signal(SIGINT, sig_handler);

  	if ( (-1) == (sd = socket(AF_INET, SOCK_DGRAM, 0))) {
		perror("socket");
		return __LINE__;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(12345);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(0 > bind(sd, (struct sockaddr *)&addr, size)){
		close(sd);
    	perror("Socket bind error");
    	return __LINE__;
 	}

  	if(0 == inet_aton(argv[2], &addr.sin_addr)){
		perror("ip-adress");
		close(sd);
		return __LINE__;
	}

  	if(0 > connect(sd, (struct sockaddr *)&addr, size)){
		close(sd);
		perror("Socket connect error");
		return __LINE__;
	}

	enemy_ip = ntohl(addr.sin_addr.s_addr);
 	getsockname(sd, (struct sockaddr *)&addr, &size);
 	my_ip = ntohl(addr.sin_addr.s_addr);

 	page_size = sysconf(_SC_PAGESIZE);


  	if ( 0 > (fb = open("/dev/fb0", O_RDWR))) {
	  	perror("open");
	   	close(sd);
	    return __LINE__;
	}

  	if ( (-1) == ioctl(fb, FBIOGET_VSCREENINFO, &info)) {
	 	perror("ioctl");
	  	close(fb);
	   	close(sd);
	   	return __LINE__;
 	}

  	if(xres > info.xres || yres > info.yres){
		close(sd);
		close(fb);
		fprintf(stderr, "Incorrect resolution (max resolution = %dx%d)\n", info.xres, info.yres);
		return 2;
	}

  	
 	fb_size = sizeof(uint32_t) * info.xres_virtual * info.yres_virtual;
  	map_size = (fb_size + (page_size - 1 )) & (~(page_size-1));
 	ptr = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
 	if ( MAP_FAILED == ptr ) {
	   	perror("mmap");
	   	close(fb);
	   	close(sd);
	   	return __LINE__;
 	}
 	

	if(NULL == initscr()){
		close(sd);
		close(fb);
		munmap(ptr, map_size);
		perror("initscr error");
		return __LINE__;
	}

	noecho();
	move(LINES - 1, COLS - 1);
	refresh();

	memset(ptr, 0, fb_size);

	if (enemy_ip < my_ip) {
		car_1.direction = 'd';
 		car_1.x = LEFT_START_X, car_1.y = LEFT_START_Y;
 		car_1.color = red, car_1.line = red_line;
 		car_1.l_x = car_1.x + 1, car_1.l_y = car_1.y - width / 2;
 		car_1.r_x = car_1.x + length, car_1.r_y = car_1.y + width / 2;

 		car_2.direction = 'a';
 		car_2.x = RIGHT_START_X, car_2.y = RIGHT_START_Y;
		car_2.color = blue, car_2.line = blue_line;
		car_2.l_x = car_2.x - length , car_2.l_y = car_2.y - width / 2;
		car_2.r_x = car_2.x - 1, car_2.r_y = car_2.y + width / 2;
 	}

 	else {
 		car_1.direction = 'a';
 		car_1.x = RIGHT_START_X, car_1.y = RIGHT_START_Y;
 		car_1.color = blue, car_1.line = blue_line;
 		car_1.l_x = car_1.x - length, car_1.l_y = car_1.y - width / 2;
		car_1.r_x = car_1.x - 1, car_1.r_y = car_1.y + width / 2;

 		car_2.direction = 'd';
 		car_2.x = LEFT_START_X, car_2.y = LEFT_START_Y;
 		car_2.color = red, car_2.line = red_line;
 		car_2.l_x = car_2.x + 1, car_2.l_y = car_2.y - width / 2;
 		car_2.r_x = car_2.x + length, car_2.r_y = car_2.y + width / 2;
 	}

	if (0 != pthread_create(&thread, NULL, func_for_thread, &ch)) {
		close(sd);
		close(fb);
		munmap(ptr, map_size);
		endwin();
		perror("pthread error");
		return __LINE__;
	}


	for(int i = 0; i < yres; i++){
		ptr[i * info.xres_virtual + xres - 1] = car_1.color;
	}

	for(int i = 0; i < yres; i++){
		ptr[i * info.xres_virtual] = car_1.color;
	}

	for(int i = 0; i < xres; i++){
		ptr[(yres - 1) * info.xres_virtual + i] = car_1.color;
	}
	
	for(int i = 0; i < xres; i++){
		ptr[i] = car_1.color;
	}

	for (int i = car_1.l_y; i <= car_1.r_y; i++) {
			for (int j = car_1.l_x; j <= car_1.r_x; j++){
	 			ptr[i * info.xres_virtual + j] = car_1.color;
			}
		}

	for (int i = car_2.l_y; i <= car_2.r_y; i++) {
		for (int j = car_2.l_x; j <= car_2.r_x; j++){
 			ptr[i * info.xres_virtual + j] = car_2.color;
		}
	}



	while (ch == 0 && work_flag == 1) {}

 	while(work_flag == 1 && car_1.lose_flag != 1 && car_2.lose_flag != 1) {

 		prev_1 = ch_1;
 		ch_1 = ch;

 		if(prev_1 == 0){
 			ch_1 = car_1.direction;
 			ch = car_1.direction;
 		}
 		else {
 			switch(ch_1){
				case 'w':
					if(prev_1 == 's')
						ch_1 = prev_1;
					break;
					
				case 'a':
					if(prev_1 == 'd')
						ch_1 = prev_1;
					break;
					
				case 's':
					if(prev_1 == 'w')
						ch_1 = prev_1;
					break;
					
				case 'd':
					if(prev_1 == 'a')
						ch_1 = prev_1;
					break;
					
				default:
					ch_1 = prev_1;
					break;
			}

 		}

 		if (-1 == write(sd, &ch_1, sizeof(char))) {
 			work_flag = 0;
 			break;

 		}

 		prev_2 = ch_2;
 		if(-1 == read(sd, &ch_2, sizeof(char))){
 			work_flag = 0;
 			break;
		}
		if (prev_2 == 0){
			ch_2 = car_2.direction;
			ch = car_2.direction;
		}
		else {
			switch(ch_2){
				case 'w':
					if(prev_2 == 's')
						ch_2 = prev_2;
					break;
					
				case 'a':
					if(prev_2 == 'd')
						ch_2 = prev_2;
					break;
					
				case 's':
					if(prev_2 == 'w')
						ch_2 = prev_2;
					break;
					
				case 'd':
					if(prev_2 == 'a')
						ch_2 = prev_2;
					break;
					
				default:
					ch_2 = prev_2;
					break;
			}
		}
 		

 		curs_set(1);


		for (int i = car_1.l_y; i <= car_1.r_y; i++) {
			for (int j = car_1.l_x; j <= car_1.r_x; j++){
				if(i > 0 && i < yres - 1 && j > 0 && j < xres - 1 && ptr[i * info.xres_virtual + j] == car_1.color)
	 				ptr[i * info.xres_virtual + j] = 0;
			}
		}

		for (int i = car_2.l_y; i <= car_2.r_y; i++) {
			for (int j = car_2.l_x; j <= car_2.r_x; j++){
				if(i > 0 && i < yres - 1 && j > 0 && j < xres - 1 && ptr[i * info.xres_virtual + j] == car_2.color)
	 				ptr[i * info.xres_virtual + j] = 0;
			}
		}
		curs_set(0);
	    switch(ch_1) {

			case 'a':
				car_1.x -= 1;
				for (int i = car_1.y - width / 2 ; i <= car_1.y + width / 2  ; i++) {
					for (int j = car_1.x - length; j <= car_1.x - 1; j++) {
						if (i <= 0 || i >= yres - 1 || j <= 0 || j >= xres - 1 || ptr[i * info.xres_virtual + j] == red || ptr[i * info.xres_virtual + j] == blue || ptr[i * info.xres_virtual + j] == red_line || ptr[i * info.xres_virtual + j] == blue_line) {
							car_1.lose_flag = 1;	
						}
						else
							ptr[i * info.xres_virtual + j] = car_1.color;
					}
				}
				car_1.l_x = car_1.x - length, car_1.r_x = car_1.x - 1;
				car_1.l_y = car_1.y - width / 2, car_1.r_y = car_1.y + width / 2;
				break;

			case 'd':
				car_1.x += 1;
				for (int i = car_1.y - width / 2 ; i <= car_1.y + width / 2  ; i++) {
					for (int j = car_1.x + 1; j <= car_1.x + length ; j++) {
						if (i <= 0 || i >= yres - 1 || j <= 0 || j >= xres - 1 || ptr[i * info.xres_virtual + j] == red || ptr[i * info.xres_virtual + j] == blue || ptr[i * info.xres_virtual + j] == red_line || ptr[i * info.xres_virtual + j] == blue_line) {
							car_1.lose_flag = 1;
						}
						else
							ptr[i * info.xres_virtual + j] = car_1.color;
					}
				}
				car_1.l_x = car_1.x + 1, car_1.r_x = car_1.x + length;
				car_1.l_y = car_1.y - width / 2, car_1.r_y = car_1.y + width / 2;
				break;

			case 's':
				car_1.y += 1;
				for (int i = car_1.y + 1; i <= car_1.y + length; i++) {
					for (int j = car_1.x - width / 2; j <= car_1.x + width / 2; j++) {
						if (i <= 0 || i >= yres - 1 || j <= 0 || j >= xres - 1 || ptr[i * info.xres_virtual + j] == red || ptr[i * info.xres_virtual + j] == blue || ptr[i * info.xres_virtual + j] == red_line || ptr[i * info.xres_virtual + j] == blue_line) {
							car_1.lose_flag = 1;
						}
						else
							ptr[i * info.xres_virtual + j] = car_1.color;
					}
				}
				car_1.l_x = car_1.x - width / 2, car_1.r_x = car_1.x + width / 2;
				car_1.l_y = car_1.y + 1, car_1.r_y = car_1.y + length;			
				break;

			case 'w':
				car_1.y -= 1;	
				for (int i = car_1.y - length ; i <= car_1.y - 1 ; i++) {
					for (int j = car_1.x - width / 2; j <= car_1.x + width / 2; j++) {
						if (i <= 0 || i >= yres - 1 || j <= 0 || j >= xres - 1 || ptr[i * info.xres_virtual + j] == red || ptr[i * info.xres_virtual + j] == blue || ptr[i * info.xres_virtual + j] == red_line || ptr[i * info.xres_virtual + j] == blue_line) {
							car_1.lose_flag = 1;
						}
						else
							ptr[i * info.xres_virtual + j] = car_1.color;
					}
				}
				car_1.l_x = car_1.x - width / 2, car_1.r_x = car_1.x + width / 2;
				car_1.l_y = car_1.y - length, car_1.r_y = car_1.y - 1;			
				break;

			default:
				break;
		}
		ptr[car_1.y * info.xres_virtual + car_1.x] = car_1.line;
		curs_set(0);
    	switch(ch_2) {

			case 'a':
				car_2.x -= 1;
				for (int i = car_2.y - width / 2 ; i <= car_2.y + width / 2  ; i++) {
					for (int j = car_2.x - length; j <= car_2.x - 1; j++) {
						if (i <= 0 || i >= yres - 1 || j <= 0 || j >= xres - 1 || ptr[i * info.xres_virtual + j] == red || ptr[i * info.xres_virtual + j] == blue || ptr[i * info.xres_virtual + j] == red_line || ptr[i * info.xres_virtual + j] == blue_line) {
							car_2.lose_flag = 1;	
						}
						else
							ptr[i * info.xres_virtual + j] = car_2.color;
					}
				}
				car_2.l_x = car_2.x - length, car_2.r_x = car_2.x - 1;
				car_2.l_y = car_2.y - width / 2, car_2.r_y = car_2.y + width / 2;
				break;

			case 'd':
				car_2.x += 1;
				for (int i = car_2.y - width / 2 ; i <= car_2.y + width / 2  ; i++) {
					for (int j = car_2.x + 1; j <= car_2.x + length; j++) {
						if (i <= 0 || i >= yres - 1 || j <= 0 || j >= xres - 1 || ptr[i * info.xres_virtual + j] == red || ptr[i * info.xres_virtual + j] == blue || ptr[i * info.xres_virtual + j] == red_line || ptr[i * info.xres_virtual + j] == blue_line) {
							car_2.lose_flag = 1;	
						}
						else
							ptr[i * info.xres_virtual + j] = car_2.color;
					}
				}
				car_2.l_x = car_2.x + 1, car_2.r_x = car_2.x + length;
				car_2.l_y = car_2.y - width / 2, car_2.r_y = car_2.y + width / 2;
				break;

			case 's':
				car_2.y += 1;
				for (int i = car_2.y + 1 ; i <= car_2.y + length; i++) {
					for (int j = car_2.x - width / 2; j <= car_2.x + width / 2; j++) {
						if (i <= 0 || i >= yres - 1 || j <= 0 || j >= xres - 1 || ptr[i * info.xres_virtual + j] == red || ptr[i * info.xres_virtual + j] == blue || ptr[i * info.xres_virtual + j] == red_line || ptr[i * info.xres_virtual + j] == blue_line) {
							car_2.lose_flag = 1;	
						}
						else
							ptr[i * info.xres_virtual + j] = car_2.color;
					}
				}
				car_2.l_x = car_2.x - width / 2, car_2.r_x = car_2.x + width / 2;
				car_2.l_y = car_2.y + 1, car_2.r_y = car_2.y + length;				
				break;

			case 'w':
				car_2.y -= 1;	
				for (int i = car_2.y - length ; i <= car_2.y - 1 ; i++) {
					for (int j = car_2.x - width / 2; j <= car_2.x + width / 2; j++) {
						if (i <= 0 || i >= yres - 1 || j <= 0 || j >= xres - 1 || ptr[i * info.xres_virtual + j] == red || ptr[i * info.xres_virtual + j] == blue || ptr[i * info.xres_virtual + j] == red_line || ptr[i * info.xres_virtual + j] == blue_line) {
							car_2.lose_flag = 1;	
						}
						else
							ptr[i * info.xres_virtual + j] = car_2.color;
					}
				}
				car_2.l_x = car_2.x - width / 2, car_2.r_x = car_2.x + width / 2;
				car_2.l_y = car_2.y - length, car_2.r_y = car_2.y - 1;				
				break;

			default:
				break;
		} 
		ptr[car_2.y * info.xres_virtual + car_2.x] = car_2.line;
		a++;
		usleep(sec);

	}
	for (int i = car_1.l_y; i <= car_1.r_y; i++) {
		for (int j = car_1.l_x; j <= car_1.r_x; j++){
	 		if (i > 0 && i < yres - 1 && j > 0 && j < xres - 1)
	 			if (ptr[i * info.xres_virtual + j] == car_2.color)
	 				tie_flag++;
		}
	}

	for (int i = car_2.l_y; i <= car_2.r_y; i++) {
		for (int j = car_2.l_x; j <= car_2.r_x; j++){
 			if (i > 0 && i < yres - 1 && j > 0 && j < xres - 1)
 				if (ptr[i * info.xres_virtual + j] == car_1.color)
 					tie_flag++;
		}
	}
 	
	if (tie_flag > 0 || (car_1.lose_flag == 1 && car_2.lose_flag == 1))
		mvprintw(LINES - 3, 0, "No one win!");
	else if(car_1.lose_flag == 1 && car_2.lose_flag == 0)
		mvprintw(LINES - 3, 0, "You lose!");
	else if(car_1.lose_flag == 0 && car_2.lose_flag == 1)
		mvprintw(LINES - 3, 0, "You win!");
	refresh();

	mvprintw(LINES - 2, 0, "Frames: %d", a);
	refresh();
	munmap(ptr, map_size);
	close(fb);
	close(sd);
	endwin();
	return 0;
}
