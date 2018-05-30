// Agent009.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <conio.h>
#include <graphics.h>
#include <math.h>
#include <ctype.h>
#include <time.h>


#pragma comment(lib,"Winmm.lib")//win多媒体API

#define PI 3.1415926536		// 圆周率
#define WIDETH 20
#define HEIGHT 15
#define MoveCoolDownTime 1//每次移动冷却时间
#define Guardspeedconsume_defaut 7//守卫非chase状态每次移动消耗
#define Guardspeedconsume_chase 5//守卫chase状态每次移动消耗
#define GuardAlarmThreshold 100//守卫警觉阈值
#define GuardWanderTime 10//警卫四处闲逛的时间
#define ELEC_ENERGY_MAX 80 //充能最大值
#define UNCONCIOUS_TIME 90 //守卫晕厥时长


DWORD* g_bufMask;			// 指向“建筑物”IMAGE 的指针
DWORD* g_bufRender;			// 指向渲染 IMAGE 的指针

LOGFONT f;                 // 新建字体格式结构体用于后面输出文本

char is_seen_by[800][600];//创建二维数组储存每个像素是否被某个警卫看到

						  //note结构体用于守卫路径搜索
struct note
{
	int x;
	int y;
	int f;//上一步在队列中的编号，方便输出路径
	int s;//步数
};
struct role
{
	int x;
	int y;//表示人物坐标
	int por_num;
	int elec_energy;//电击充能
	int cya_num;
};
struct cell
{
	bool downwall;
	bool rightwall;//表示各方向墙的有无，0表示没有，1表示有
	bool portal;//表示传送设备的有无，0表示无，1表示有
	bool exit;//表示出口
	bool guard;//表示有没有守卫
	bool cyanide;//表示有没有氰化物
};
struct cell map[HEIGHT][WIDETH];//15*20的地图
								//声明10个player 将会在每关地图生成时对player进行具体初始化
struct role player;
//定义守卫结构体并声明8个实例
struct guard_still
{
	int x;
	int y;
	int x_initial;
	int y_initial;

	char direction;//表示守卫朝向，d表示右，s表示下，a表示左，w表示上
	char direction_initial;
	int aim_x;
	int aim_y;
	int alarm;//表示是否引起警觉，值越高警觉性越高
	char status;
	char status_initial;//储存守卫初始状态
	int speed;//speed用于放慢守卫移动
	int speed_consume;//一次移动增加的speed
	int wandertime;//守卫游荡倒计时 归0时返回初始状态

	int patrol_x;//巡逻检查点坐标
	int patrol_y;
	bool patrol_check;//FALSE表示从起始点到检查点 TRUE表示正在返程

	bool is_dead;//是否死亡
	bool body_found;//死亡后尸体是否被发现
	int unconsious_time;//昏迷清醒倒计时
};

//声明8个守卫
struct guard_still guard[8];

// 创建渲染对象
IMAGE imgRender(800, 600);


//保存用户正在游玩的关卡
int mission_num = 0;
//key=1则表示通关
bool key = FALSE;
//储存当前守卫数目
int guard_num = 0;
//force表示能力是否开启 将会被用于绘制游戏界面故声明为全局变量
bool force;

//倒计时计时器
int counter;
//用于产生闪烁效果
bool Blink;
//控制玩家移动速度的冷却
short int MoveCoolDown = 0;
//设定重试游戏为全局变量 避免函数自身调用导致程序崩溃
bool retrygame = FALSE;
//设定重选关卡为全局变量 避免函数自身调用导致程序崩溃
bool rechoosegame = FALSE;
//设定布尔值判断玩家是否完成关卡任务
bool mission_goal = FALSE;

//守卫寻路函数
char findway(int x1, int y1, int x2, int y2)//守卫从(x1,y1)走到(x2,y2)
{
	if (x1 == x2 && y1 == y2)
	{
		return '?';
	}


	int startx = x1, starty = y1;
	int *p, *q;
	p = &x1;
	q = &y1;
	struct note que[301];
	int book[15][20] = { 0 };
	int head, tail, flag;
	head = 1;
	tail = 1;//初始化队列
	que[tail].x = x1;
	que[tail].y = y1;
	que[tail].f = 0;
	que[tail].s = 0;
	tail++;
	book[y1][x1] = 1;
	flag = 0;//标记是否到达目的地，1表示到达
	int next[4][2] = { { 1,0 },//向右走
	{ 0,1 },//向下走
	{ -1,0 },//向左走
	{ 0,-1 },//向上走
	};
	while (head < tail)
	{
		for (int i = 0; i < 4; i++)//从右开始判断
		{
			int tx, ty;//表示下一步的坐标
			tx = que[head].x + next[i][0];
			ty = que[head].y + next[i][1];
			if (tx < 1 || tx>18 || ty < 1 || ty>13)//判断是否越界
				continue;
			//接下来判断是否撞墙
			if (i == 0 && map[que[head].y][que[head].x].rightwall == 1)//如果右边是墙
				continue;
			else if (i == 1 && map[que[head].y][que[head].x].downwall == 1)//如果下面是墙
				continue;
			else if (i == 2 && map[que[head].y][que[head].x - 1].rightwall == 1)//如果左边是墙
				continue;
			else if (i == 3 && map[que[head].y - 1][que[head].x].downwall == 1)//如果上面是墙
				continue;
			if (book[ty][tx] == 0)
			{
				book[ty][tx] = 1;//标记表示已经走过
				que[tail].x = tx;
				que[tail].y = ty;
				que[tail].f = head;//插入新的点到队列中
				que[tail].s = que[head].s + 1;
				tail++;
			}
			if (tx == x2 && ty == y2)//如果到达了目的地
			{
				flag = 1;
				break;
			}
		}
		if (flag == 1)
			break;
		head++;
	}
	if (flag == 0)
		return '?';
	int step[300] = { 0 };//记录路径
	step[0] = que[tail - 1].f;
	for (int i = 1; i<que[tail - 1].s; i++)
	{
		step[i] = que[step[i - 1]].f;
	}
	int i = que[tail - 1].s - 1;
	if (i)
	{
		map[*q][*p].guard = 0;
		*p = que[step[i - 1]].x;
		*q = que[step[i - 1]].y;
		map[*q][*p].guard = 1;
	}
	else
	{
		map[*q][*p].guard = 0;
		*p = x2;
		*q = y2;
		map[*q][*p].guard = 1;
	}
	if (map[*q][*p].guard == 0)
		return '?';
	if (*p == startx && *q < starty)
		return 'w';
	else if (*p == startx && *q > starty)
		return 's';
	else if (*p < startx&&*q == starty)
		return 'a';
	else if (*p > startx&&*q == starty)
		return 'd';
	return '?';
}


//每次刷新的警卫行动
void guardaction(void)
{
	for (int i = 0; i < guard_num; i++)
	{
		if (!guard[i].is_dead)
		{
			if (!guard[i].unconsious_time)
			{
				//当守卫离开追逐状态时渐渐降低警戒值
				if (guard[i].status != 'c'&& guard[i].alarm > 0)
				{
					guard[i].alarm--;
				}
				//刷新守卫移动冷却
				if (guard[i].speed != 0)
				{
					guard[i].speed--;
				}


				//根据守卫状态确定守卫移动消耗 若Chase状态刷新一次移动一格 其他情况刷新Guardspeedconsume_defaut次移动一格
				if (guard[i].status == 'c')
				{
					guard[i].speed_consume = Guardspeedconsume_chase;
				}
				else
				{
					guard[i].speed_consume = Guardspeedconsume_defaut;
				}

				if (guard[i].status == 'p')
				{
					if (guard[i].x == guard[i].patrol_x && guard[i].y == guard[i].patrol_y)
					{
						guard[i].patrol_check = TRUE;
					}
					if (guard[i].x == guard[i].x_initial && guard[i].y == guard[i].y_initial)
					{
						guard[i].patrol_check = FALSE;
					}

					if (guard[i].patrol_check == TRUE)
					{
						guard[i].aim_x = guard[i].x_initial;
						guard[i].aim_y = guard[i].y_initial;
					}
					else
					{
						guard[i].aim_x = guard[i].patrol_x;
						guard[i].aim_y = guard[i].patrol_y;
					}
				}



				//若守卫 back   investigate   chase   (patrol) 状态且移动冷却清零开始移动 
				if (guard[i].status != 's' && guard[i].status != 'w' && guard[i].speed == 0)
				{
					char guard_move = findway(guard[i].x, guard[i].y, guard[i].aim_x, guard[i].aim_y);
					switch (guard_move)
					{
					case 'w':
						guard[i].y--;
						guard[i].direction = 'w';
						guard[i].speed += guard[i].speed_consume;
						break;
					case 's':
						guard[i].y++;
						guard[i].direction = 's';
						guard[i].speed += guard[i].speed_consume;
						break;
					case 'a':
						guard[i].x--;
						guard[i].direction = 'a';
						guard[i].speed += guard[i].speed_consume;
						break;
					case 'd':
						guard[i].x++;
						guard[i].direction = 'd';
						guard[i].speed += guard[i].speed_consume;
						break;
					case '?':
						if (guard[i].status == 'b')
						{
							guard[i].status = guard[i].status_initial;
							guard[i].direction = guard[i].direction_initial;
							break;
						}
						//当返回问号是因为不可到达时 将目的地改为自身坐标
						if (guard[i].x != guard[i].aim_x&&guard[i].y != guard[i].aim_y)
						{
							guard[i].aim_x = guard[i].x;
							guard[i].aim_y = guard[i].y;
						}
						
						guard[i].status = 'w';
						guard[i].wandertime = GuardWanderTime;
						break;
					}
				}

				//若进入wander状态随机游荡
				if (guard[i].status == 'w'&& guard[i].speed == 0)
				{
					srand((unsigned)time(NULL));
					int temp = rand() % 4;
					switch (temp)
					{
					case 0:
						guard[i].aim_x = guard[i].x + 1;
						guard[i].aim_y = guard[i].y;
						break;
					case 1:
						guard[i].aim_x = guard[i].x;
						guard[i].aim_y = guard[i].y + 1;
						break;
					case 2:
						guard[i].aim_x = guard[i].x - 1;
						guard[i].aim_y = guard[i].y;
						break;
					case 3:
						guard[i].aim_x = guard[i].x;
						guard[i].aim_y = guard[i].y - 1;
						break;
					}

					char guard_move = findway(guard[i].x, guard[i].y, guard[i].aim_x, guard[i].aim_y);
					switch (guard_move)
					{
					case 'w':
						guard[i].y--;
						guard[i].direction = 'w';
						guard[i].speed += guard[i].speed_consume;
						break;
					case 's':
						guard[i].y++;
						guard[i].direction = 's';
						guard[i].speed += guard[i].speed_consume;
						break;
					case 'a':
						guard[i].x--;
						guard[i].direction = 'a';
						guard[i].speed += guard[i].speed_consume;
						break;
					case 'd':
						guard[i].x++;
						guard[i].direction = 'd';
						guard[i].speed += guard[i].speed_consume;
						break;
					default:
						break;
					}
					if (guard[i].wandertime > 0)
					{
						guard[i].wandertime--;
					}
					
					if (guard[i].wandertime == 0)
					{
						guard[i].aim_x = guard[i].x_initial;
						guard[i].aim_y = guard[i].y_initial;
						guard[i].status = 'b';
					}
				}

			}
			else
			{
				guard[i].unconsious_time--;
				if (guard[i].unconsious_time == 1)
				{
					guard[i].status = 'w';
					guard[i].wandertime = GuardWanderTime;
				}
			}
		}

	}
}


void mapping_border()//将地图的边界设置为墙
{
	for (int i = 1; i < WIDETH - 1; i++)
	{
		map[0][i].downwall = 1;
		map[HEIGHT - 2][i].downwall = 1;
	}
	for (int i = 1; i < HEIGHT - 1; i++)
	{
		map[i][0].rightwall = 1;
		map[i][WIDETH - 2].rightwall = 1;
	}

}

//所有map函数应当对玩家及守卫的所有初始值进行赋值
//第0关地图
void mapping0()
{
	for (int i = 4; i <= 13; i++)
		map[i][3].rightwall = 1;
	for (int i = 1; i <= 13; i++)
	{
		map[i][10].rightwall = 1;
		map[i][15].rightwall = 1;
	}
	map[7][7].portal = 1;
	map[13][6].portal = 1;
	map[13][7].portal = 1;
	map[13][8].portal = 1;
	/*该段用于测试
	map[13][1].portal = 1; map[13][2].portal = 1;
	map[13][3].portal = 1; map[13][4].portal = 1;
	map[13][5].portal = 1; map[13][9].portal = 1;
	map[13][10].portal = 1; map[13][11].portal = 1;
	map[13][12].portal = 1; map[13][13].portal = 1;
	map[13][14].portal = 1; map[13][15].portal = 1;
	*/
	map[7][13].portal = 1;//传送设备位置
	map[3][17].exit = 1;//出口位置
	player.x = 2;
	player.y = 12;//玩家初始位置
	player.por_num = 0;//初始化传送设备数目
	guard_num = 0;
}
//第1关地图
void mapping1()
{
	for (int i = 1; i < 19; i++)
		map[8][i].downwall = 1;
	for (int i = 15; i < 19; i++)
		map[9][i].downwall = 1;
	for (int i = 10; i < 14; i++)
		map[i][14].rightwall = 1;
	map[12][16].rightwall = 1;
	map[13][16].rightwall = 1;
	map[11][17].downwall = 1;
	map[11][18].downwall = 1;
	map[3][14].portal = 1;
	map[13][6].portal = 1;
	map[13][7].portal = 1;
	map[13][8].portal = 1;
	map[10][13].portal = 1;
	map[13][18].exit = 1;
	player.x = 2;
	player.y = 2;
	player.por_num = 0;//初始化传送设备数目
	guard[0].x = 8;
	guard[0].y = 1;
	guard[0].direction = 's';
	map[1][8].guard = 1;
	guard[1].x = 15;
	guard[1].y = 3;
	guard[1].direction = 'd';
	map[3][15].guard = 1;
	guard[2].x = 12;
	guard[2].y = 9;
	guard[2].direction = 's';
	map[9][12].guard = 1;
	guard_num = 3;
	
	for (int i = 0; i < 3; i++)
	{
		guard[i].status = 's';
		guard[i].speed = 0;
		guard[i].speed_consume = Guardspeedconsume_defaut;//初始化speedconsume为Guardspeedconsume_defaut 即刷新Guardspeedconsume_defaut次移动一格
		guard[i].wandertime = 0;
		guard[i].status_initial = guard[i].status;
		guard[i].x_initial = guard[i].x;
		guard[i].y_initial = guard[i].y;
		guard[i].direction_initial = guard[i].direction;
		guard[i].is_dead = 0;
		guard[i].unconsious_time = 0;
	}
}

void mapping2()
{
	//墙
	for (int i = 1; i <= 18; i++)
		map[7][i].downwall = 1;
	//传送设备
	map[7][2].portal = 1;
	map[8][4].portal = 1;
	map[7][9].portal = 1;
	map[8][15].portal = 1;
	player.por_num = 0;//初始化传送设备数目
					   //出口
	map[2][18].exit = 1;
	//玩家
	player.x = 1;
	player.y = 7;
	//守卫
	guard_num = 6;
	for (int i = 0; i < guard_num; i++)
	{
		guard[i].status = 's';
		guard[i].speed = 0;
		guard[i].speed_consume = Guardspeedconsume_defaut;//初始化speedconsume为Guardspeedconsume_defaut 即刷新Guardspeedconsume_defaut次移动一格
		guard[i].wandertime = 0;
		guard[i].patrol_check = FALSE;
		guard[i].is_dead = 0;
		guard[i].unconsious_time = 0;
	}

	
	guard[0].x = 3;
	guard[0].y = 1;
	guard[0].patrol_x = 3;
	guard[0].patrol_y = 6;
	guard[0].direction = 's';
	map[1][3].guard = 1;
	guard[1].x = 8;
	guard[1].y = 1;
	guard[1].patrol_x = 8;
	guard[1].patrol_y = 6;
	guard[1].direction = 's';
	map[1][8].guard = 1;
	guard[2].x = 14;
	guard[2].y = 1;
	guard[2].patrol_x = 14;
	guard[2].patrol_y = 6;
	guard[2].direction = 's';
	map[1][14].guard = 1;
	guard[3].x = 3;
	guard[3].y = 8;
	guard[3].patrol_x = 3;
	guard[3].patrol_y = 13;
	guard[3].direction = 's';
	map[8][3].guard = 1;
	guard[4].x = 8;
	guard[4].y = 8;
	guard[4].patrol_x = 8;
	guard[4].patrol_y = 13;
	guard[4].direction = 's';
	map[8][8].guard = 1;
	guard[5].x = 14;
	guard[5].y = 8;
	guard[5].patrol_x = 14;
	guard[5].patrol_y = 13;
	guard[5].direction = 's';
	map[8][14].guard = 1;

	for (int i = 0; i < guard_num; i++)
	{
		guard[i].status = 'p';
		guard[i].status_initial = guard[i].status;
		guard[i].x_initial = guard[i].x;
		guard[i].y_initial = guard[i].y;
		guard[i].direction_initial = guard[i].direction;
	}

}

void mapping3()
{
	//墙
	for (int i = 5; i <= 8; i++)
		for (int j = 1; j <= 14; j++)
			map[i][j].downwall = 1;

	map[2][3].rightwall = 1;
	map[3][3].rightwall = 1;
	map[4][3].rightwall = 1;
	map[4][2].downwall = 1;
	map[4][3].downwall = 1;
	map[1][11].rightwall = 1;
	map[2][11].rightwall = 1;
	map[3][11].rightwall = 1;
	map[4][11].rightwall = 1;

	map[6][14].rightwall = 1;
	map[7][14].rightwall = 1;
	map[8][14].rightwall = 1;
	//传送设备
	for (int i = 2; i <= 5; i++)
		for (int j = 16; j <= 18; j++)
			map[i][j].portal = 1;
	player.por_num = 0;//初始化传送设备数目
					   //出口
	map[13][1].exit = 1;
	//玩家
	player.x = 1;
	player.y = 1;
	player.elec_energy = 0;
	player.por_num = 1;
	//守卫
	guard_num = 5;

	guard[0].x = 6;
	guard[0].y = 1;
	guard[0].direction = 's';
	map[1][6].guard = 1;
	guard[1].x = 8;
	guard[1].y = 4;
	guard[1].direction = 'w';
	map[4][8].guard = 1;
	guard[2].x = 13;
	guard[2].y = 4;
	guard[2].direction = 'd';
	map[4][13].guard = 1;
	guard[3].x = 3;
	guard[3].y = 9;
	guard[3].direction = 's';
	map[9][3].guard = 1;
	guard[4].x = 7;
	guard[4].y = 9;
	guard[4].direction = 's';
	map[9][7].guard = 1;
	guard[5].x = 11;
	guard[5].y = 9;
	guard[5].direction = 's';
	map[9][11].guard = 1;

	for (int i = 0; i < guard_num; i++)
	{
		guard[i].status = 's';
		guard[i].speed = 0;
		guard[i].speed_consume = Guardspeedconsume_defaut;//初始化speedconsume为Guardspeedconsume_defaut 即刷新Guardspeedconsume_defaut次移动一格
		guard[i].wandertime = 0;
		guard[i].status_initial = guard[i].status;
		guard[i].x_initial = guard[i].x;
		guard[i].y_initial = guard[i].y;
		guard[i].direction_initial = guard[i].direction;
		guard[i].is_dead = 0;
		guard[i].unconsious_time = 0;
	}
}

void mapping4()
{
	//墙
	for (int i = 2; i <= 7; i++)
		map[3][i].downwall = 1;
	map[2][2].rightwall = 1;
	map[2][7].rightwall = 1;
	map[3][7].rightwall = 1;
	for (int i = 10; i <= 13; i++)
		map[3][i].downwall = 1;
	map[2][9].rightwall = 1;
	map[3][9].rightwall = 1;
	map[2][13].rightwall = 1;
	map[1][12].downwall = 1;
	map[1][13].downwall = 1;
	for (int i = 5; i <= 8; i++)
		map[9][i].downwall = 1;
	for (int i = 11; i <= 14; i++)
		map[9][i].downwall = 1;
	map[10][8].rightwall = 1;
	map[11][8].rightwall = 1;
	map[10][10].rightwall = 1;
	map[11][10].rightwall = 1;
	map[11][12].rightwall = 1;
	map[10][13].downwall = 1;
	for (int i = 3; i <= 5; i++)
		map[11][i].downwall = 1;
	map[12][5].rightwall = 1;
	map[13][2].rightwall = 1;
	//传送设备
	player.por_num = 0;
	//出口
	map[12][16].exit = 1;
	//玩家
	player.x = 4;
	player.y = 2;
	//守卫
	guard_num = 4;

	guard[0].x = 7;
	guard[0].y = 4;
	guard[0].direction = 's';
	map[4][7].guard = 1;
	guard[1].x = 11;
	guard[1].y = 4;
	guard[1].direction = 's';
	map[4][11].guard = 1;
	guard[2].x = 8;
	guard[2].y = 9;
	guard[2].direction = 'w';
	map[9][8].guard = 1;
	guard[3].x = 12;
	guard[3].y = 9;
	guard[3].direction = 'w';
	map[9][12].guard = 1;

	for (int i = 0; i < guard_num; i++)
	{
		guard[i].speed = 0;
		guard[i].speed_consume = Guardspeedconsume_defaut;//初始化speedconsume为Guardspeedconsume_defaut 即刷新Guardspeedconsume_defaut次移动一格
		guard[i].wandertime = 0;
		guard[i].status_initial = guard[i].status;
		guard[i].x_initial = guard[i].x;
		guard[i].y_initial = guard[i].y;
		guard[i].direction_initial = guard[i].direction;
		guard[i].is_dead = 0;
		guard[i].unconsious_time = 0;
	}


}

void mapping5()
{
	//墙
	int i;
	for (i = 1; i <= 5; i++)
	{
		map[i][1].rightwall = 1;
	}
	map[8][1].rightwall = 1;
	map[9][1].rightwall = 1;
	map[12][1].rightwall = 1;
	map[2][2].rightwall = 1;
	map[3][2].rightwall = 1;
	map[6][2].rightwall = 1;
	map[9][2].rightwall = 1;
	map[10][2].rightwall = 1;
	map[1][3].rightwall = 1;
	map[2][3].rightwall = 1;
	for (i = 5; i <= 11; i++)
	{
		map[i][3].rightwall = 1;
	}
	for (i = 4; i <= 12; i++)
	{
		map[i][4].rightwall = 1;
	}
	map[3][5].rightwall = 1;
	map[4][5].rightwall = 1;
	map[9][5].rightwall = 1;
	map[10][5].rightwall = 1;
	map[12][5].rightwall = 1;
	map[13][5].rightwall = 1;
	map[2][6].rightwall = 1;
	map[5][6].rightwall = 1;
	map[6][6].rightwall = 1;
	map[7][6].rightwall = 1;
	map[8][6].rightwall = 1;
	map[12][6].rightwall = 1;
	map[1][7].rightwall = 1;
	map[4][7].rightwall = 1;
	map[5][7].rightwall = 1;
	map[6][7].rightwall = 1;
	map[9][7].rightwall = 1;
	map[13][7].rightwall = 1;
	map[5][8].rightwall = 1;
	map[12][8].rightwall = 1;
	map[7][9].rightwall = 1;
	map[8][9].rightwall = 1;
	map[11][9].rightwall = 1;
	map[5][10].rightwall = 1;
	map[6][10].rightwall = 1;
	map[7][10].rightwall = 1;
	map[8][10].rightwall = 1;
	map[9][10].rightwall = 1;
	map[11][10].rightwall = 1;
	map[3][11].rightwall = 1;
	map[10][11].rightwall = 1;
	map[12][11].rightwall = 1;
	map[2][12].rightwall = 1;
	map[3][12].rightwall = 1;
	map[4][12].rightwall = 1;
	map[6][12].rightwall = 1;
	map[7][12].rightwall = 1;
	map[8][12].rightwall = 1;
	map[9][12].rightwall = 1;
	map[11][12].rightwall = 1;
	map[13][12].rightwall = 1;
	map[1][13].rightwall = 1;
	map[5][13].rightwall = 1;
	map[6][13].rightwall = 1;
	map[9][13].rightwall = 1;
	map[10][13].rightwall = 1;
	map[11][13].rightwall = 1;
	map[12][13].rightwall = 1;
	for (i = 2; i <= 6; i++)
	{
		map[i][14].rightwall = 1;
	}
	for (i = 8; i <= 13; i++)
	{
		map[i][14].rightwall = 1;
	}
	for (i = 2; i <= 4; i++)
	{
		map[i][15].rightwall = 1;
	}
	for (i = 7; i <= 12; i++)
	{
		map[i][15].rightwall = 1;
	}
	map[3][16].rightwall = 1;
	map[5][16].rightwall = 1;
	for (i = 7; i <= 13; i++)
	{
		map[i][16].rightwall = 1;
	}
	map[2][17].rightwall = 1;
	map[3][17].rightwall = 1;
	map[6][17].rightwall = 1;
	map[1][5].downwall = 1;
	map[1][6].downwall = 1;
	for (int i = 8; i <= 12; i++)
		map[1][i].downwall = 1;
	map[1][14].downwall = 1;
	map[1][16].downwall = 1;
	map[1][17].downwall = 1;
	map[2][4].downwall = 1;
	map[2][5].downwall = 1;
	for (int i = 7; i <= 11; i++)
		map[2][i].downwall = 1;
	map[2][13].downwall = 1;
	map[3][3].downwall = 1;
	map[3][4].downwall = 1;
	for (int i = 7; i <= 11; i++)
		map[3][i].downwall = 1;
	map[3][14].downwall = 1;
	map[3][17].downwall = 1;
	map[4][2].downwall = 1;
	map[4][3].downwall = 1;
	map[4][6].downwall = 1;
	for (int i = 9; i <= 13; i++)
		map[4][i].downwall = 1;
	for (int i = 16; i <= 18; i++)
		map[4][i].downwall = 1;
	map[5][5].downwall = 1;
	map[5][9].downwall = 1;
	map[5][12].downwall = 1;
	map[6][1].downwall = 1;
	map[6][2].downwall = 1;
	map[6][6].downwall = 1;
	map[6][11].downwall = 1;
	map[6][15].downwall = 1;
	map[6][17].downwall = 1;
	map[7][2].downwall = 1;
	map[7][3].downwall = 1;
	map[7][5].downwall = 1;
	map[7][8].downwall = 1;
	map[7][12].downwall = 1;
	map[7][13].downwall = 1;
	map[7][14].downwall = 1;
	map[7][18].downwall = 1;

	map[8][6].downwall = 1;
	map[8][8].downwall = 1;
	map[8][9].downwall = 1;
	map[8][11].downwall = 1;
	map[8][17].downwall = 1;
	map[9][7].downwall = 1;
	map[9][8].downwall = 1;
	map[9][9].downwall = 1;
	map[9][10].downwall = 1;
	map[9][12].downwall = 1;
	map[9][18].downwall = 1;
	map[10][1].downwall = 1;
	map[10][2].downwall = 1;
	map[10][7].downwall = 1;
	map[10][8].downwall = 1;
	map[10][9].downwall = 1;
	map[10][11].downwall = 1;
	map[10][13].downwall = 1;
	map[10][17].downwall = 1;
	map[11][2].downwall = 1;
	map[11][3].downwall = 1;
	map[11][7].downwall = 1;
	map[11][8].downwall = 1;
	map[11][10].downwall = 1;
	map[11][12].downwall = 1;
	map[11][18].downwall = 1;
	map[12][2].downwall = 1;
	map[12][3].downwall = 1;
	map[12][4].downwall = 1;
	map[12][9].downwall = 1;
	map[12][10].downwall = 1;
	map[12][11].downwall = 1;
	map[12][13].downwall = 1;
	map[12][17].downwall = 1;
	//氰化物
	map[6][8].cyanide = 1;
	map[7][8].cyanide = 1;
	map[6][9].cyanide = 1;
	map[7][9].cyanide = 1;
	map[3][17].cyanide = 1;
	//出口
	map[13][17].exit = 1;
	//玩家
	player.x = 1;
	player.y = 1;
	player.por_num = 1;
	player.cya_num = 0;
	//守卫
	guard_num = 2;
	guard[0].x = 17;
	guard[0].y = 2;
	guard[0].direction = 'a';
	guard[0].status = 'p';
	guard[0].patrol_x = 7;
	guard[0].patrol_y = 7;
	guard[1].x = 17;
	guard[1].y = 13;
	guard[1].status = 'p';
	guard[1].patrol_x = 1;
	guard[1].patrol_y = 1;
	guard[1].direction = 'd';

	for (int i = 0; i < guard_num; i++)
	{
		//guard[i].status = 's';
		guard[i].speed = 0;
		guard[i].speed_consume = Guardspeedconsume_defaut;//初始化speedconsume为Guardspeedconsume_defaut 即刷新Guardspeedconsume_defaut次移动一格
		guard[i].wandertime = 0;
		guard[i].status_initial = guard[i].status;
		guard[i].x_initial = guard[i].x;
		guard[i].y_initial = guard[i].y;
		guard[i].direction_initial = guard[i].direction;
		guard[i].is_dead = 0;
		guard[i].unconsious_time = 0;
	}

	
}

void mapping6()
{
	//墙
	for (int i = 2; i <= 17; i++)
		map[1][i].downwall = 1;
	for (int i = 2; i <= 5; i++)
		map[i][1].rightwall = 1;
	for (int i = 2; i <= 5; i++)
		map[i][17].rightwall = 1;
	for (int i = 3; i <= 5; i++)
		map[i][11].rightwall = 1;
	map[5][5].rightwall = 1;
	map[5][9].rightwall = 1;
	for (int i = 2; i <= 17; i++)
		map[5][i].downwall = 1;
	map[5][3].downwall = 0;
	map[5][8].downwall = 0;
	map[5][9].downwall = 0;
	map[5][14].downwall = 0;
	for (int i = 6; i <= 13; i++)
		map[10][i].downwall = 1;
	for (int i = 6; i <= 10; i++)
		map[i][5].rightwall = 1;
	for (int i = 6; i <= 10; i++)
		map[i][13].rightwall = 1;
	//传送设备
	player.por_num = 0;
	//出口
	map[13][18].exit = 1;
	//玩家
	player.x = 1;
	player.y = 1;
	player.cya_num = 1;
	player.por_num = 0;
	//守卫
	guard_num = 5;
	guard[0].x = 3;
	guard[0].y = 11;
	guard[0].direction = 'w';
	guard[0].patrol_x = 8;
	guard[0].patrol_y = 5;
	map[11][3].guard = 1;
	guard[1].x = 3;
	guard[1].y = 13;
	guard[1].direction = 'w';
	map[13][3].guard = 1;
	//guard 2 是目标
	guard[2].x = 8;
	guard[2].y = 8;
	guard[2].direction = 's';
	map[8][8].guard = 1;
	guard[3].x = 17;
	guard[3].y = 13;
	guard[3].direction = 'a';
	map[13][17].guard = 1;
	guard[4].x = 18;
	guard[4].y = 12;
	guard[4].direction = 'w';
	map[12][18].guard = 1;

	for (int i = 0; i < guard_num; i++)
	{
		guard[i].status = 's';
		guard[i].speed = 0;
		guard[i].speed_consume = Guardspeedconsume_defaut;//初始化speedconsume为Guardspeedconsume_defaut 即刷新Guardspeedconsume_defaut次移动一格
		guard[i].wandertime = 0;
		guard[i].status_initial = guard[i].status;
		guard[i].x_initial = guard[i].x;
		guard[i].y_initial = guard[i].y;
		guard[i].direction_initial = guard[i].direction;
		guard[i].is_dead = 0;
		guard[i].unconsious_time = 0;
	}

	guard[0].status = 'p';
	guard[0].status_initial = guard[0].status;

	//test
	//guard[2].is_dead = TRUE;

}


void rightmove()
{
	int *p, *q, *r;
	r = &mission_num;
	p = &player.x; q = &player.y;
	if (force == 0)//判断行动是否强化
	{
		if (map[*q][*p].rightwall)
			return;
		else
		{
			player.x++;
			//移动陷入冷却
			MoveCoolDown = MoveCoolDownTime;

			return;
		}
	}
	else if (force == 1)
	{
		player.x++;
		player.por_num--;
		force = 0;
		MoveCoolDown = MoveCoolDownTime;
		return;
	}
}
void leftmove()
{
	int *p, *q, *r;
	r = &mission_num;
	p = &player.x; q = &player.y;
	if (force == 0)//判断行动是否强化
	{
		if (map[*q][*p - 1].rightwall)
			return;
		else
		{
			player.x--;
			MoveCoolDown = MoveCoolDownTime;
			return;
		}
	}
	else if (force == 1)
	{
		player.x--;
		player.por_num--;
		MoveCoolDown = MoveCoolDownTime;
		force = 0;
		return;
	}
}
void upmove()
{
	int *p, *q, *r;
	r = &mission_num;
	p = &player.x; q = &player.y;
	if (force == 0)//判断行动是否强化
	{
		if (map[*q - 1][*p].downwall)
			return;
		else
		{
			player.y--;
			MoveCoolDown = MoveCoolDownTime;
			return;
		}
	}
	else if (force == 1)
	{
		player.y--;
		player.por_num--;
		MoveCoolDown = MoveCoolDownTime;
		force = 0;
		return;
	}
}
void downmove()
{
	int *p, *q, *r;
	r = &mission_num;
	p = &player.x; q = &player.y;
	if (force == 0)//判断行动是否强化
	{
		if (map[*q][*p].downwall)
			return;
		else
		{
			player.y++;
			MoveCoolDown = MoveCoolDownTime;
			return;
		}
	}
	else if (force == 1)
	{
		player.y++;
		player.por_num--;
		MoveCoolDown = MoveCoolDownTime;
		force = 0;
		return;
	}
}

//用于判断是否已经完成任务目标
bool MissionCheck(void)
{
	//待完成
	switch (mission_num)
	{
	case 3:
		if (player.por_num < 10)
		{
			settextstyle(35, 0, _T("Courier"));
			outtextxy(150, 250, _T("需要收集10个能源才可离开！"));
			FlushBatchDraw();
			Sleep(500);
			BeginBatchDraw();
			return FALSE;
		}
		else
		{
			return TRUE;
		}
		break;
	case 4:
		if (mission_goal)
			return TRUE;
		else
			return FALSE;
		break;
	case 6:
		if (guard[2].is_dead)
			return TRUE;
		else
			return FALSE;
		break;
	default:
		return TRUE;
	}
	return TRUE;
}


void test()
{
	int *p, *q, *r;
	r = &mission_num;
	p = &player.x; q = &player.y;
	if (map[*q][*p].portal == 1)
	{
		player.por_num++;
		map[*q][*p].portal = 0;
	}
	if (map[*q][*p].exit == 1)
		key = MissionCheck();
	if (map[*q][*p].cyanide == 1)
	{
		player.cya_num++;
		map[*q][*p].cyanide = 0;
	}
}

//关卡机制函数 用于在每次刷新时产生关卡特定效果
void MissionMachanism(void)
{
	switch (mission_num)
	{
	case 4:
		//鉴定玩家是否拿到物品 若拿到物品开始警报
		if (player.x == 9 && player.y == 7 &&counter==0)
		{
			//警报持续三百次刷新
			counter = 300;

			settextstyle(50, 0, _T("Courier"));
			outtextxy(250, 250, _T("触发了警报！"));
			FlushBatchDraw();
			Sleep(1000);
			BeginBatchDraw();
		}
		if (counter > 0)
		{
			//每20次刷新将玩家坐标发给所有守卫 test 声音需要布置
			if (counter % 20 == 0)
			{
				for (int i = 0; i < guard_num; i++)
				{
					if (guard[i].status != 'c'&&guard[i].status != 'i' && !guard[i].is_dead && guard[i].unconsious_time == 0)
					{
						guard[i].aim_x = player.x;
						guard[i].aim_y = player.y;
						guard[i].status = 'i';
					}
				}
			}
			counter--;
			if (counter == 0)
			{
				settextstyle(50, 0, _T("Courier"));
				outtextxy(300, 250, _T("警报解除！"));
				FlushBatchDraw();
				Sleep(1000);
				BeginBatchDraw();
				
				mission_goal = TRUE;
			}
		}
		break;
	}

	return;
}


//任务介绍函数   待完善 未完成关卡默认使用第一关
void intro_mission(void)
{
	//确定文字输出范围
	RECT r = { 50,150,760,560 };
	//将文字在新的IMAGE上绘制 避免原背景被删除
	IMAGE text(800, 600);
	SetWorkingImage(&text);

	/*文字背景图片是否采用待定
	IMAGE textbg;//建立IMAGE储存文字背景
	loadimage(&textbg, _T("textbg2.jpg"));
	putimage(0, 0, &textbg);*/

	//clearrectangle(0,0,800,400);

	gettextstyle(&f);                     // 获取当前字体设置
	f.lfHeight = 30;                      // 设置字体高度为 30
	_tcscpy_s(f.lfFaceName, _T("楷体"));    // 设置字体
	f.lfQuality = ANTIALIASED_QUALITY;    // 设置输出效果为抗锯齿  
	settextstyle(&f);                     // 设置字体样式

	settextcolor(0xfffa57);

	switch (mission_num)
	{
	case 0:
		drawtext(_T("欢迎回来！Agent009！\n\
			你在上一次执行任务时头部受创，可能导致你的记忆\n\
			出现了缺失，怕你忘记了怎么行动，就再说一遍吧：\n\
			使用WSAD移动,F键可以用来开启/或关闭传送能力，\n\
			那是我们公司的机密科技,可以让你无视墙壁阻碍到\n\
			达附近区域。蓝色的圆圈可以补充你的剩余传送次数\n\
			。绿色的圆即是出口，到达那里即可完成任务。\n\
            按F键关闭本条消息"), &r, DT_LEFT);
		break;
	case 1:
		drawtext(_T("    看来身手依然矫健啊！\n\
			    让我们看看你的潜入技能是否一如既往！          \n\
			    前面站了些守卫正在原地发呆 小心的绕过他们的\n\
			视野!或者，对自己身手还有信心的话...逗一逗他们\n\
			(坏笑)在关键时刻可以试着使用传送能力脱离险境！\n\
			不要试着长时期待在守卫视野里！呢样会让他们确定\n\
			你的存在，并展开快速追捕。当守卫颜色是黄色时，\n\
			表示他们还在怀疑状态，要保持小心！\n\
			    按F键关闭本条消息"), &r, DT_LEFT);
		break;
	case 2:
		drawtext(_T("    表现不错！\n\
			    你现在已经可以回到正常的任务中去了！      \n\
			    公司的传送门科技需要大量的能源驱动，所以我\n\
			们需要你去渗透敌方公司并拿回一些能量。现在我们\n\
			将送你到敌方公司的门口。他们为了保护能源雇了许\n\
			多退伍军人来保护入口。他们似乎还保持着列队习惯\n\
			，这应该会是一个突破点。想办法利用我们的科技不\n\
			被发现渗透进去！\n\
			    按F键关闭本条消息"), &r, DT_LEFT);
		/*拿到能量以后\n\
			不可以滥用，如果没有带着10个以上的能量，即使你到了\n\
			出口我们也不会接你离开的！\n\*/
		break;
	case 3:
		drawtext(_T("    很好！看样子你已经成功渗透。\n\
			    你现在已经可以使用公司的电击科技了。      \n\
			    电击需要一段时间进行充能，其进度将时刻显示\n\
			在屏幕的左上方。当其开始闪烁时，表示可以使用。\n\
			按E键即可释放电击。强大的电流将使你周围的守卫陷\n\
			入暂时休克，但别楞在原地不动！过些时候他们会醒\n\
			来搜索是谁电击了他们！小心使用电击来完成任务！\n\
			    拿到能量以后不可以滥用，如果没有带着10个以\n\
			上的能量，即使你到了出口我们也不会接你离开的！\n\
			    按F键关闭本条消息"), &r, DT_LEFT);
		break;
	case 4:
		drawtext(_T("你的上次行动挽救了公司的能源危机！\n\
			    现在你需要解决一下公司的经济危机了。     \n\
			    我们刚刚得知一个敌对公司拥有着世界上最大的\n\
			钻石，黑市上给这个钻石开出了天价，拿回这个钻石\n\
			将让公司有能力开发更好的科技！但是据说他们使用\n\
			了最新的防盗科技，钻石一旦被拿走将会一直报警并\n\
			告知自己的位置。保持隐蔽！ 当警报解除后我们就\n\
			会接你离开。\n\
			    按F键关闭本条消息"), &r, DT_LEFT);
		break;

	default:
		drawtext(_T("欢迎回来！Agent009！\n\
			你在上一次执行任务时头部受创，可能导致你的记忆\n\
			出现了缺失，怕你忘记了怎么行动，就再说一遍吧：\n\
			使用WSAD移动,F键可以用来开启/或关闭传送能力，\n\
			那是我们公司的机密科技,可以让你无视墙壁阻碍到\n\
			达附近区域。蓝色的圆圈可以补充你的剩余传送次数\n\
			。绿色的圆即是出口，到达那里即可完成任务。\n\
            按F键关闭本条消息"), &r, DT_LEFT);



	}
	SetWorkingImage(NULL);
	putimage(0, 0, &text, SRCPAINT);
}


// 在指定位置“照明”,direction只能为'w','a','s','d'.  _x _y均为cell坐标 在函数内改变为像素坐标
void Lighting(int _x, int _y, char direction, int guard_index)
{
	double delta_direction;
	double sidelen = 180.0;//这里边长实际为正方形的边长的1/4
	int range;

	//将cell坐标改变为像素坐标
	_x = 20 + _x * 40;
	_y = 20 + _y * 40;

	//根据朝向控制循环的初始弧度
	switch (direction)
	{
	case 's':
		delta_direction = PI / 6.0;
		break;
	case 'w':
		delta_direction = 7.0* PI / 6.0;
		break;
	case 'a':
		delta_direction = 2.0* PI / 3.0;
		break;
	case 'd':
		delta_direction = 5.0* PI / 3.0;
		break;
	}


	// 计算视野照亮的区域
	for (double a = delta_direction; a < 2.0 * PI / 3.0 + delta_direction; a += PI / 540)	// 圆周循环
	{
		/*
		switch (direction)
		{
		case 's':
			range = (int)sidelen / sin(a);
			break;
		case 'w':
			range = -(int)sidelen / sin(a);
			break;
		case 'a':
			range = -(int)sidelen / cos(a);
			break;
		case 'd':
			range = (int)sidelen / cos(a);
			break;

		}*/

		range = 180;


		for (int r = 0; r < range; r++)				// 半径循环
		{
			// 计算照射到的位置
			int x = (int)(_x + cos(a) * r);
			int y = (int)(_y + sin(a) * r);

			// 光线超出屏幕范围，终止
			// （为了简化，不处理最上和最下一行，实际也不可能影响最下一边界行）
			if (x < 0 || x >= 800 || y <= 0 || y >= 600 - 1)
				break;

			// 光线碰到建筑物，终止
			if (g_bufMask[y * 800 + x])
				break;

			// 光线叠加
			g_bufRender[y * 800 + x] += 0x505000;
			//临时量储存守卫信息 该守卫对应蓝色字节增加量为2^n  
			int temp = 1;
			for (int i = 0; i < guard_index; i++)
			{
				temp *= 2;
			}
			if ((is_seen_by[x][y] >> guard_index) % 2 == 0)
			{
				is_seen_by[x][y] += temp;	// 这个地方需要根据具体守卫具体处理
			}

		}
	}


}

//显示守卫视觉的函数
void GenerateGuardsVision(void)
{
	memset(g_bufRender, 0, 800 * 600 * 4);
	for (int i = 0 ; i < 800; i++)
	{
		for (int j = 0 ; j < 600; j++)
		{
			is_seen_by[i][j] = 0;
		}
	}

	for (int i = 0; i < guard_num; i++)
	{
		if (!guard[i].is_dead && guard[i].unconsious_time == 0)
		{
			Lighting(guard[i].x, guard[i].y, guard[i].direction, i);
		}		
	}
	//建立指针储存屏幕显存
	DWORD* bufScreen = GetImageBuffer(NULL);
	//建立缓冲图层 用缓冲图层储存RB信息 进行模糊反曝光操作 避免对Render层的Blue信息造成影响
	IMAGE cusion(800, 600);

	//建立指针储存cusion显存
	DWORD* bufCusion = GetImageBuffer(&cusion);
	//清空cusion内存
	memset(bufCusion, 0, 800 * 600);
	//将Render层的信息存至Cusion层
	for (int i = 0; i < 800 * 600 - 1; i++)
	{
		if (g_bufRender[i])
		{
			bufCusion[i] = g_bufRender[i];
			//bufCusion[i] /= 256;
			//bufCusion[i] *= 256;
		}
	}


	// 修正曝光过度的点
	for (int i = 800 * 600 - 1; i >= 0; i--)
		if (bufCusion[i] > 0xffffff)
			bufCusion[i] = 0xffffff;

	// 将光线模糊处理（避开建筑物）
	for (int i = 800; i < 800 * (600 - 1); i++)
		if (!g_bufMask[i])
			for (int j = 0; j < 2; j++)
			{
				bufCusion[i] = RGB(
					(GetRValue(bufCusion[i - 800]) + GetRValue(bufCusion[i - 1]) + GetRValue(bufCusion[i])
						+ GetRValue(bufCusion[i + 1]) + GetRValue(bufCusion[i + 800])) / 5,
						(GetGValue(bufCusion[i - 800]) + GetGValue(bufCusion[i - 1]) + GetGValue(bufCusion[i])
							+ GetGValue(bufCusion[i + 1]) + GetGValue(bufCusion[i + 800])) / 5,
							(GetBValue(bufCusion[i - 800]) + GetBValue(bufCusion[i - 1]) + GetBValue(bufCusion[i])
								+ GetBValue(bufCusion[i + 1]) + GetBValue(bufCusion[i + 800])) / 5);
			}

	//针对每个显存单位如果有cusion数据 绘制在屏幕上
	for (int i = 0; i < 800 * 600 - 1; i++)
	{
		if (bufCusion[i])
		{
			bufScreen[i] = bufCusion[i];
		}
	}


}

//常规绘制游戏画面 每次刷新调用一次 将在函数外部flushbatchdraw.
void drawgame(void)
{
	int i, j;//循环变量

	//绘制守卫视野
	GenerateGuardsVision();

	// 绘制玩家   暂时先以实心方格代表玩家
	/*setfillcolor(WHITE);
	solidrectangle(player.x * 40 + 10, player.y * 40 + 10, player.x * 40 + 30, player.y * 40 + 30);*/
	IMAGE playericon;
	loadimage(&playericon, _T("player.jpg"));
	putimage(player.x * 40, player.y * 40, &playericon);


	//绘制守卫 暂时以方格代表守卫 
	
	for (int i = 0; i < guard_num; i++)
	{
		if (guard[i].is_dead)
		{
			IMAGE Gravestone_image;
			loadimage(&Gravestone_image, _T("Gravestone_image.jpg"));
			putimage(guard[i].x * 40, guard[i].y * 40, &Gravestone_image);
			continue;
		}
		
		setfillcolor(BLUE);
		if (guard[i].status == 'i' || guard[i].status == 'w')
		{
			setfillcolor(YELLOW);
		}

		if (guard[i].status == 'c')
		{
			setfillcolor(RED);
		}
		
		if (guard[i].unconsious_time > 0)
		{
			int temp;
			temp = 255 - guard[i].unconsious_time * (255 / UNCONCIOUS_TIME);
			setfillcolor(RGB(temp,temp,85));
		}

		solidrectangle(guard[i].x * 40 + 10, guard[i].y * 40 + 10, guard[i].x * 40 + 30, guard[i].y * 40 + 30);
		/*
		IMAGE Guard_image;
		loadimage(&Guard_image, _T("Guard_image.jpg"));
		putimage(guard[i].x * 40, guard[i].y * 40, &Guard_image);
		*/
	}
	//特殊关卡绘制
	switch (mission_num)
	{
	case 4:
		if (counter == 0 && !mission_goal)
		{
			IMAGE Diamond_image;
			loadimage(&Diamond_image, _T("Diamond_image.jpg"));
			putimage(360, 280, &Diamond_image);
			break;

		}
		
	}



	//遍历地图每个方格 进行绘制
	for (i = 0; i < 15; i++)
	{
		for (j = 0; j < 20; j++)
		{
			//绘制传送设备 暂时以蓝色圆球代表		
			if (map[i][j].portal == TRUE)
			{
				setfillcolor(0xEE687B);
				solidcircle(j * 40 + 20, i * 40 + 20, 10);
			}
			//绘制出口 选取了常见的出口图片
			if (map[i][j].exit == TRUE)
			{
				IMAGE Exit_image;
				loadimage(&Exit_image, _T("Exit_image.jpg"));
				putimage(j*40, i*40, &Exit_image);
			}
			if (map[i][j].cyanide == TRUE)
			{
				setfillcolor(0x578B2E);
				solidcircle(j * 40 + 20, i * 40 + 20, 10);
			}

		}
	}

	//绘制电击是否开启 同时考虑关卡是否达到要求 较前的关卡将不显示
	settextcolor(WHITE);
	settextstyle(30, 15, _T("Courier"));
	if (mission_num >= 3)
	{
		outtextxy(40, 5, _T("电击能力："));

		setfillcolor(WHITE);
		if (player.elec_energy == ELEC_ENERGY_MAX&&Blink)
		{
			setfillcolor(YELLOW);
		}
		
		solidrectangle(200 , 5, 200 + 2 * player.elec_energy, 35);
		
	}

	if (mission_num >= 5)
	{
		settextstyle(30, 15, _T("Courier"));
		outtextxy(400, 5, _T("氰化物剩余："));
		int CyaToUse = player.cya_num;
		moveto(580, 5);
		if (CyaToUse > 9)
		{
			int temp = CyaToUse / 10;
			switch (temp)
			{
			case 1:
				outtext(_T("1"));
				break;
			case 2:
				outtext(_T("2"));
				break;
			case 3:
				outtext(_T("3"));
				break;
			case 4:
				outtext(_T("4"));
				break;
			case 5:
				outtext(_T("5"));
				break;
			case 6:
				outtext(_T("6"));
				break;
			case 7:
				outtext(_T("7"));
				break;
			case 8:
				outtext(_T("8"));
				break;
			case 9:
				outtext(_T("9"));
				break;
			}
		}
		int temp = CyaToUse % 10;
		switch (temp)
		{
		case 0:
			outtext(_T("0"));
			break;
		case 1:
			outtext(_T("1"));
			break;
		case 2:
			outtext(_T("2"));
			break;
		case 3:
			outtext(_T("3"));
			break;
		case 4:
			outtext(_T("4"));
			break;
		case 5:
			outtext(_T("5"));
			break;
		case 6:
			outtext(_T("6"));
			break;
		case 7:
			outtext(_T("7"));
			break;
		case 8:
			outtext(_T("8"));
			break;
		case 9:
			outtext(_T("9"));
			break;
		}

	}




	//绘制传送能力是否开启及剩余次数
	settextcolor(WHITE);
	settextstyle(30, 15, _T("Courier"));

	if (force)
	{
		outtextxy(40, 565, _T("传送能力：ON"));
	}
	else
	{
		outtextxy(40, 565, _T("传送能力：OFF"));
	}
	//定义变量储存用户剩余传送次数
	int PortalToUse = player.por_num;

	RECT r = { 280, 565, 800, 600 };
	drawtext(_T("剩余传送次数: "), &r, DT_LEFT);
	//图形库自带输出变量能力受限 以switch输出剩余传送次数
	moveto(480, 565);
	if (PortalToUse > 9)
	{
		int temp = PortalToUse / 10;
		switch (temp)
		{
		case 1:
			outtext(_T("1"));
			break;
		case 2:
			outtext(_T("2"));
			break;
		case 3:
			outtext(_T("3"));
			break;
		case 4:
			outtext(_T("4"));
			break;
		case 5:
			outtext(_T("5"));
			break;
		case 6:
			outtext(_T("6"));
			break;
		case 7:
			outtext(_T("7"));
			break;
		case 8:
			outtext(_T("8"));
			break;
		case 9:
			outtext(_T("9"));
			break;
		}
	}
	int temp = PortalToUse % 10;
	switch (temp)
	{
	case 0:
		outtext(_T("0"));
		break;
	case 1:
		outtext(_T("1"));
		break;
	case 2:
		outtext(_T("2"));
		break;
	case 3:
		outtext(_T("3"));
		break;
	case 4:
		outtext(_T("4"));
		break;
	case 5:
		outtext(_T("5"));
		break;
	case 6:
		outtext(_T("6"));
		break;
	case 7:
		outtext(_T("7"));
		break;
	case 8:
		outtext(_T("8"));
		break;
	case 9:
		outtext(_T("9"));
		break;
	}


}

//检测玩家是否被看到的函数
bool test_ifseen(void)
{
	bool is_seen = FALSE;//储存是否被看到用于return
	int x_px, y_px;//储存玩家坐标对应的中心像素点
	x_px = player.x * 40 + 20;
	y_px = player.y * 40 + 20;
	//检测用户坐标中心及周围8个像素是否被守卫看到

	for (int i = x_px - 2; i <= x_px + 2; i++)
	{
		for (int j = y_px - 2; j <= y_px + 2; j++)
		{
			//如果用户所在附近像素会被敌人看到
			if (is_seen_by[i][j])
			{
				//获取该像素点的 is_seen_by 以此判别被那位守卫看到
				int bValue = is_seen_by[i][j];
				for (int k = 0; k < guard_num; k++)
				{
					//判断第k位守卫是否看到 如看到bValue%2==1将等于1
					if (bValue % 2 == 1)
					{
						//todo:  将第k位守卫的目的地改为当前玩家坐标 
						guard[k].aim_x = player.x;
						guard[k].aim_y = player.y;
						guard[k].wandertime = 0;
						//当守卫警戒值在追逐阈值2倍以下时 积累警戒值
						if (guard[k].alarm < 2 * GuardAlarmThreshold)
						{
							guard[k].alarm++;
						}
						
						if (guard[k].alarm > GuardAlarmThreshold)
						{
							guard[k].status = 'c';
							settextstyle(50, 0, _T("Courier"));
							outtextxy(250, 150, _T("守卫在追你！"));
						}
						else
						{
							guard[k].status = 'i';
						}
				
						//此处在测试 最终将删除
						/*switch (k)
						{
						case 0:
							settextstyle(50, 0, _T("Courier"));
							outtextxy(250, 150, _T("0号警卫！"));
							break;
						case 1:
							settextstyle(50, 0, _T("Courier"));
							outtextxy(250, 150, _T("1号警卫！"));
							break;
						case 2:
							settextstyle(50, 0, _T("Courier"));
							outtextxy(250, 150, _T("2号警卫！"));
							break;
						}*/
						//以上在测试
					}
					is_seen = TRUE;
					bValue = bValue / 2;
				}
			}
		}
	}
	if (is_seen)
		return TRUE;
	else
		return FALSE;
}

//被发现过的body不会运行该函数
void test_bodyfound(int guard_index)
{
	int x_px, y_px;//储存尸体坐标对应的中心像素点
	x_px = guard[guard_index].x * 40 + 20;
	y_px = guard[guard_index].y * 40 + 20;
	//检测body坐标中心及周围8个像素是否被守卫看到

	for (int i = x_px - 2; i <= x_px + 2; i++)
	{
		for (int j = y_px - 2; j <= y_px + 2; j++)
		{
			//如果body所在附近像素会被敌人看到 所有存活清醒并且不在追逐状态下的守卫目的地改至Body
			if (is_seen_by[i][j])
			{
				for (int k = 0; k < guard_num; k++)
				{
					if (!guard[k].is_dead&&guard[k].unconsious_time == 0 && guard[k].status != 'c')
					{
						guard[k].status = 'i';
						guard[k].aim_x = guard[guard_index].x;
						guard[k].aim_y = guard[guard_index].y;
					}
				}
				guard[guard_index].body_found = TRUE;
			}
		}
	}
	
}


IMAGE* drawgamebuildings(void)
{
	//声明static IMAGE 以确保在函数外可使用该变量
	static IMAGE g_imgMask(800, 600);
	//获取绘图设备的显存指针并储存于全局变量 用于之后的渲染判断
	g_bufMask = GetImageBuffer(&g_imgMask);
	// 设置绘图目标
	SetWorkingImage(&g_imgMask);
	//防止之前关卡叠加
	//cleardevice();
	memset(g_bufMask, 0, 800 * 600 * 4);
	//设置划线（墙）颜色及样式
	setlinecolor(WHITE);
	setlinestyle(PS_SOLID, 3);

	int i, j;     //i,j用于循环控制
	for (i = 0; i < 15; i++)
	{
		for (j = 0; j < 20; j++)
		{
			if (map[i][j].rightwall == TRUE)
			{
				//绘制每个方格右边的墙
				line(40 + 40 * j, 40 * i, 40 + 40 * j, 40 + 40 * i);
			}
			if (map[i][j].downwall == TRUE)
			{
				//绘制每个方格下边的墙
				line(40 * j, 40 + 40 * i, 40 + 40 * j, 40 + 40 * i);
			}
		}
	}

	// 恢复绘图目标为默认窗口
	SetWorkingImage(NULL);
	return &g_imgMask;
}

//定义gameover函数
void gameover(void)
{
	settextcolor(0x999999);
	f.lfWidth = 30;
	settextstyle(&f);
	outtextxy(250, 150, _T("Game over"));
	f.lfWidth = 20;
	settextstyle(&f);
	outtextxy(250, 250, _T("按r重新开始本局"));
	outtextxy(250, 350, _T("按c重选关卡"));
	outtextxy(250, 450, _T("按q退出游戏"));
	FlushBatchDraw();
	BeginBatchDraw();
	char kin;
	while (TRUE)
	{
		Sleep(200);
		if (_kbhit())
		{
			kin = _getch();
			switch (kin)
			{
			case 'r':
				retrygame = TRUE;
				cleardevice();
				return;

			case 'q':
				exit(0);
			case 'c':
				rechoosegame = TRUE;
				cleardevice();
				return;
			}
		}
	}
}

void MissionCompleted(void)
{
	settextcolor(0x999999);
	f.lfWidth = 30;
	settextstyle(&f);
	outtextxy(150, 150, _T("Mission Completed!"));
	f.lfWidth = 20;
	settextstyle(&f);
	outtextxy(250, 250, _T("按n进入下一关"));
	outtextxy(250, 350, _T("按c重选关卡"));
	outtextxy(250, 450, _T("按q退出游戏"));
	FlushBatchDraw();
	BeginBatchDraw();
	char kin;
	key = 0;
	while (TRUE)
	{
		Sleep(200);
		if (_kbhit())
		{
			kin = _getch();
			switch (kin)
			{
			case 'n':
				mission_num++;
				retrygame = TRUE;
				cleardevice();
				return;

			case 'q':
				exit(0);
			case 'c':
				rechoosegame = TRUE;
				cleardevice();
				return;
			}
		}
	}
}

void pause(void)
{
	//设置文字颜色为灰色
	settextcolor(0x999999);
	settextstyle(50, 0, _T("Courier"));
	outtextxy(350, 50, _T("暂停"));
	settextstyle(30, 0, _T("Courier"));
	outtextxy(300, 150, _T("按r重新开始本局"));
	outtextxy(300, 250, _T("按p恢复游戏"));
	outtextxy(300, 350, _T("按c重选关卡"));
	outtextxy(300, 450, _T("按q退出游戏"));

	FlushBatchDraw();
	BeginBatchDraw();
	char kin;
	while (TRUE)
	{
		Sleep(200);
		if (_kbhit())
		{
			kin = _getch();
			switch (kin)
			{
			case 'r':
				retrygame = TRUE;
				cleardevice();
				return;
			case 'p':
				return;
			case 'q':
				exit(0);
			case 'c':
				rechoosegame = TRUE;
				cleardevice();
				return;
			}
		}
	}
}

int choose_mission(void)
{
	//声明字符型变量储存用户输入
	char kin;
	//声明整型变量储存mission_num_chosen用户所选择的关卡序号
	int mission_num_chosen = 0;
	//新建IMAGE对象ChooseMission并载入图片
	IMAGE ChooseMission;
	loadimage(&ChooseMission, _T("choose_mission.jpg"));

	//改变工作区至默认窗口
	SetWorkingImage(NULL);
	//设定填充颜色为黄色
	setfillcolor(0x00a0a0);
	//进入循环 直到用户按'f'后跳出 0.1s刷新一次
	while (TRUE)
	{
		//清空屏幕准备好下一次绘制
		cleardevice();
		//对用户输入进行处理
		if (_kbhit())
		{
			kin = _getch();
			kin = tolower(kin);
			//对用户输入进行判断并改变被选关卡
			switch (kin)
			{
			case 'w':
				if (mission_num_chosen >= 5)
				{
					mission_num_chosen -= 5;
				}
				break;
			case 's':
				if (mission_num_chosen <= 4)
				{
					mission_num_chosen += 5;
				}
				break;
			case 'a':
				if (mission_num_chosen != 0 && mission_num_chosen != 5)
				{
					mission_num_chosen--;
				}
				break;
			case 'd':
				if (mission_num_chosen != 4 && mission_num_chosen != 9)
				{
					mission_num_chosen++;
				}
				break;
			case 'f':
				return mission_num_chosen;
			}
		}

		// 根据所选择的关卡序号填充方格
		fillrectangle(80 + 60 * (mission_num_chosen % 5), 240 + 60 * (mission_num_chosen / 5), 140 + 60 * (mission_num_chosen % 5), 300 + 60 * (mission_num_chosen / 5));

		//将图片展示在窗口 与默认窗口‘或’操作（srcpaint）
		putimage(0, 0, &ChooseMission, SRCPAINT);
		//使用batchdraw和Sleep 避免屏闪
		FlushBatchDraw();
		Sleep(100);
		BeginBatchDraw();

	}
}


//Tip 进入Ingame之前已经开始batchdraw
void ingame(void)
{
	//初始化玩家状态
	player.elec_energy = 0;
	player.por_num = 0;
	player.x = 0;
	player.y = 0;
	for (int i = 0; i < guard_num; i++)
	{
		guard[i].alarm = 0;
		guard[i].body_found = FALSE;
	}
	mission_goal = FALSE;
	counter = 0;

	//初始化地图
	memset(map, 0, WIDETH * HEIGHT * sizeof(cell));
	//生成地图边界
	mapping_border();
	//这里switch语句判断用户选择的关卡 需要补全
	switch (mission_num)
	{
	case 0:
		mapping0();
		break;
	case 1:
		mapping1();
		break;
	case 2:
		mapping2();
		break;
	case 3:
		mapping3();
		break;
	case 4:
		mapping4();
		break;
	case 5:
		mapping5();
		break;
	case 6:
		mapping6();
		break;
	//测试阶段 未完成关卡自动进入第0关
	default:
		mapping0();
		break;
	}


	//使用IMAGE指针变量储存drawgamebuildings函数返回的值 以用于后续的putimage函数
	IMAGE* maskpt = drawgamebuildings();
	//显示buildings
	putimage(0, 0, maskpt, SRCPAINT);
	//绘制游戏界面
	drawgame();

	//输出任务介绍
	intro_mission();

	//刷新界面
	FlushBatchDraw();
	//声明字符型变量储存用户输入
	char input;

	//检测用户输入‘f'以关闭任务介绍
	do
	{
		input = _getch();
	} while (input != 'f');

	BeginBatchDraw();

	//进入游戏循环
	while (key == 0)
	{
		Blink = !Blink;
		//根据关卡在刷新时产生特定效果
		MissionMachanism();

		//移动冷却恢复
		if (MoveCoolDown > 0)
		{
			MoveCoolDown--;
		}
		//电击充能恢复 测试中
		if (player.elec_energy < ELEC_ENERGY_MAX)
		{
			player.elec_energy++;
		}
		//判断是否被看到 警卫改变坐标已经写在test_ifseen函数内
		if (test_ifseen())
		{
			settextstyle(50, 0, _T("Courier"));
			outtextxy(250, 50, _T("被看到了！"));
			FlushBatchDraw();
			BeginBatchDraw();
			//子弹时间 被发现时游戏变慢 测试关闭
			//Sleep(200);
			//todo 被发现音效 test 
			/*mciSendString(_T("open D:\\Users\\MACHENIKE\\source\\repos\\Agent009\\Agent009seen.m4a alias seensound"), NULL, 0, NULL);
			mciSendString(_T("play seensound"), NULL, 0, NULL);
			无效 测试中*/
		}
		//判断尸体被发现
		for (int i = 0; i < guard_num; i++)
		{
			if (guard[i].is_dead&&guard[i].body_found == FALSE)
			{
				test_bodyfound(i);
			}
		}

		//判断是否被守卫抓住
		for (int i = 0; i < guard_num; i++)
		{
			if (guard[i].x == player.x&&guard[i].y == player.y)
			{
				if (!guard[i].is_dead&&guard[i].unconsious_time == 0)
				{
					gameover();
					return;
				}			
			}
		}


		if (_kbhit() && MoveCoolDown == 0)
		{
			input = _getch();

			switch (input)
			{
			case 'w':
				upmove();
				test();
				break;
			case 'a':
				leftmove();
				test();
				break;
			case 's':
				downmove();
				test();
				break;
			case 'd':
				rightmove();
				test();
				break;
			case 'f'://按f强化下一次行动，若已强化，则取消
				if (player.por_num != 0 && force == 0)
					force = 1;
				else if (force == 1)
				{
					force = 0;
				}
				break;
			case 'p'://按p进入暂停
				pause();
				if (retrygame || rechoosegame)
				{
					return;
				}
				break;
			case 'e':
				//电击 
				if (player.elec_energy == ELEC_ENERGY_MAX)
				{
					player.elec_energy = 0;
					for (int i = 0; i < guard_num; i++)
					{
						if (abs(guard[i].x - player.x) <= 1 && abs(guard[i].y - player.y) <= 1)
						{
							guard[i].unconsious_time = UNCONCIOUS_TIME;
							guard[i].wandertime = 0;
						}
					}
				}
				break;
			case 'r':
				//使用氰化物
				if (player.cya_num > 0)
				{
					player.cya_num--;
					for (int i = 0; i < guard_num; i++)
					{
						if (abs(guard[i].x - player.x) <= 1 && abs(guard[i].y - player.y) <= 1)
						{
							guard[i].is_dead = 1;
							map[guard[i].y][guard[i].x].guard = 0;//守卫死亡
						}
					}
				}
				break;

			}
		}

		//警卫行动
		guardaction();
		

		cleardevice();
		drawgame();
		putimage(0, 0, maskpt, SRCPAINT);

		//控制刷新频率
		Sleep(50);

		FlushBatchDraw();
		BeginBatchDraw();
	}
	MissionCompleted();
	FlushBatchDraw();
	BeginBatchDraw();
	return;
}


int main()
{
	//声明字符型变量‘kin’存储用户输入
	char kin;

	//初始化窗口，分辨率 800 * 600.
	initgraph(800, 600);

	g_bufRender = GetImageBuffer(&imgRender);
	
	//新建IMAGE对象startmenu并载入图片
	IMAGE startmenu;
	loadimage(&startmenu, _T("agent009menu.jpg"));
	//将图片展示在窗口
	putimage(0, 0, &startmenu);
	//进行循环直到用户输入'f’


	do
	{
		kin = _getch();
	} while (kin != 'f' && kin != 'F');

	//进入choose_mission函数 获取用户选择的关卡序号 并储存在mission_num变量中 函数末尾开始batchdraw 未flush
	mission_num = choose_mission();

	ingame();
	//使用循环变量控制重新开始游戏 避免递归
	while (retrygame || rechoosegame)
	{
		while (retrygame)
		{
			retrygame = FALSE;
			ingame();
		}
		while (rechoosegame)
		{
			mission_num = choose_mission();
			rechoosegame = FALSE;
			ingame();
		}
	}


	//关闭绘图设备
	closegraph();
	//该部分用于调试
	/*printf("mission number:%d",mission_num);
	_getch();*/
	return 0;
}

