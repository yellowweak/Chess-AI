#include<cstdio>
#include<cstdlib>
#include"anqi.hh"
#include"Protocol.h"
#include"ClientSocket.h"

#ifdef _WINDOWS
#include<windows.h>
#else
#include<ctime>
#endif

#define BASIC 10000
int step=2;
int endgame;
int repeat;
int eval_flag, eeeat, kind;

const int rate[7] = {50, 30, 20, 10, 5, 25, 2};
static const POS ADJ[32][4]={
	{ 1,-1,-1, 4},{ 2,-1, 0, 5},{ 3,-1, 1, 6},{-1,-1, 2, 7},
	{ 5, 0,-1, 8},{ 6, 1, 4, 9},{ 7, 2, 5,10},{-1, 3, 6,11},
	{ 9, 4,-1,12},{10, 5, 8,13},{11, 6, 9,14},{-1, 7,10,15},
	{13, 8,-1,16},{14, 9,12,17},{15,10,13,18},{-1,11,14,19},
	{17,12,-1,20},{18,13,16,21},{19,14,17,22},{-1,15,18,23},
	{21,16,-1,24},{22,17,20,25},{23,18,21,26},{-1,19,22,27},
	{25,20,-1,28},{26,21,24,29},{27,22,25,30},{-1,23,26,31},
	{29,24,-1,-1},{30,25,28,-1},{31,26,29,-1},{-1,27,30,-1}
};

const int DEFAULTTIME = 900 ;
typedef double SCORE;
static const SCORE INF=1000001;
static const SCORE WIN=1000000;
//SCORE SearchMax(const BOARD&,int,int);
//SCORE SearchMin(const BOARD&,int,int);
SCORE SearchNega(const BOARD&,int,int,SCORE,SCORE);
SCORE basic_value(const BOARD &B);
SCORE dynamic_value(const BOARD &B);
SCORE attack_value(const BOARD &B);
SCORE depth_value(int dep);


#ifdef _WINDOWS
DWORD Tick;     // 開始時刻
int   TimeOut;  // 時限
#else
clock_t Tick;     // 開始時刻
clock_t TimeOut;  // 時限
#endif
MOV   BestMove; // 搜出來的最佳著法
int   root;		// search tree裡root的顏色

bool TimesUp() {
#ifdef _WINDOWS
	return GetTickCount()-Tick>=TimeOut;
#else
	return clock() - Tick > TimeOut;
#endif
}

SCORE basic_value(const BOARD &B){
	int t=0;
	//printf("root= %d\n", root);
	for(POS p=0; p<32; p++){
		const CLR c = GetColor(B.fin[p]);
		if(c != -1 && c == root)	t+=rate[B.fin[p]%7]*BASIC;
		//printf("\t%d", c);
		//if(p%4 == 3) printf("\n");
	}
	for(int i=root*7; i<root*7+7; i++)		t += rate[i%7]*BASIC*B.cnt[i];
	
	int t2=0;
	for(POS p=0; p<32; p++){
		const CLR c = GetColor(B.fin[p]);
		if(c != -1 && c == (root^1))	t2+=rate[B.fin[p]%7]*BASIC;
	}
	for(int i=(root^1)*7; i<(root^1)*7+7; i++)		t2 += rate[i%7]*BASIC*B.cnt[i];
	
	for(int i=0; i<repeat; i++)	printf("  ");
	printf("t= %d, t2= %d, return= %d\n", t, t2, t-t2);
	return t-t2;
}

SCORE dynamic_value(const BOARD &B){
	int t=0;
	// 算第 i 顆棋子基本價值
	int bv[14];
	for(int i=0; i<14; i++){
		int tt=0;
		
		// i = 砲
		if(i%7 == 5){
			//我方棋子數x4
			for(int j=((i/7)*7)%14; j<((i/7)*7)%14+7; j++)		tt += B.cnt[j] * 4;
			//對方棋子數x1
			for(int j=((i/7)*7+7)%14; j<((i/7)*7+7)%14+7; j++)	tt += B.cnt[j] * 1;
			
			//已翻
			for(POS p=0; p<32; p++){
				const CLR c = GetColor(B.fin[p]);
				if(c != -1)		tt += (c == i/7)?	4:1;
			}
			
			bv[i] = tt;
		}
		
		// i = 帥
		else if(i%7 == 0){
			tt += 1;
			for(int j=((i/7)*7+7)%14; j<((i/7)*7+7)%14+7; j++){
				if(B.cnt[j] > 0){
					// j 無法反擊 帥 (j LEVEL為 1~5)
					if(1 <= GetLevel(FIN(j)) && GetLevel(FIN(j)) <= 5)	tt += B.cnt[j] * 4;
					// j 可以反擊 帥 (j LEVEL為 0)
					else if(GetLevel(FIN(j)) == 0)					tt += B.cnt[j] * 1;
				}
			}
			
			//已翻
			for(POS p=0; p<32; p++){
				const CLR c = GetColor(B.fin[p]);
				if(c != -1 && c != i/7 && 1 <= GetLevel(B.fin[p]) && GetLevel(B.fin[p]) <= 5)		tt += 4;
				else if(c != -1 && c != i/7 && GetLevel(B.fin[p]) == 0)							tt += 1;
			}
			
			bv[i] = tt;
		}
		
		// i = 卒
		else if(i%7 == 6){
			tt += 1;
			for(int j=((i/7)*7+7)%14; j<((i/7)*7+7)%14+7; j++){
				if(B.cnt[j] > 0){
					// j 無法反擊 卒 (j LEVEL為 0)
					if(GetLevel(FIN(j)) == 0)			tt += B.cnt[j] * 4;
					// j 可以反擊 卒 (j LEVEL為 6)
					else if(GetLevel(FIN(j)) == 6)		tt += B.cnt[j] * 1;
				}
			}
			
			//已翻
			for(POS p=0; p<32; p++){
				const CLR c = GetColor(B.fin[p]);
				if(c != -1 && c != i/7 && GetLevel(B.fin[p]) == 0)		tt += 4;
				else if(c != -1 && c != i/7 && GetLevel(B.fin[p]) == 6)	tt += 1;
			}
			bv[i] = tt;
		}
		
		// i = 其他
		else{
			tt += 1;
			for(int j=((i/7)*7+7)%14; j<((i/7)*7+7)%14+7; j++){
				if(B.cnt[j] > 0){
					// j 無法反擊 i (j LEVEL較 i 高)
					if(GetLevel(FIN(i)) < GetLevel(FIN(j)))		tt += B.cnt[j] * 4;
					// j 可以反擊 i (j 與 i LEVEL相同)
					else if(GetLevel(FIN(i)) == GetLevel(FIN(j)))	tt += B.cnt[j] * 1;
				}
			}
			
			//已翻
			for(POS p=0; p<32; p++){
				const CLR c = GetColor(B.fin[p]);
				if(c != -1 && c != i/7 && GetLevel(FIN(i)) < GetLevel(B.fin[p]))			tt += 4;
				else if(c != -1 && c != i/7 && GetLevel(FIN(i)) == GetLevel(B.fin[p]))		tt += 1;
			}
			
			bv[i] = tt;
		}
	}
	
	//for(int i=0; i<14; i++)	printf("bv[%d]= %d\n", i, bv[i]);
	
	int remain[14];
	for(int i=0; i<14; i++)	remain[i] = 0;
	for(POS p=0; p<32; p++){
		const CLR c = GetColor(B.fin[p]);
		if(c != -1)	remain[B.fin[p]]++;
	}
	for(int i=0; i<14; i++)	remain[i] += B.cnt[i];
	/*
	printf("remain= ");
	for(int i=0; i<14; i++)	printf("%d", remain[i]);
	printf("\n");
	*/
	//每顆己方棋子 i 子力為「能吃到的所有棋子(明+暗) j 基本價值總和」
	int dv = 0;
	for(int i=(root)*7; i<(root)*7+7; i++){
		//printf("dv= %d\n", dv);
		// i = 帥
		if(i%7 == 0){
			for(int j=(root^1); j<(root^1)+7; j++){
				if(0 <= j%7 && j%7 <= 5)	dv += remain[i]*remain[j]*bv[j]*rate[i%7];
			}
		}
		// i = 卒
		else if(i%7 == 6){
			for(int j=(root^1); j<(root^1)+7; j++){
				if(0 == j%7 || j%7 == 6)	dv += remain[i]*remain[j]*bv[j]*rate[i%7];
			}
		}
		// i = 砲
		else if(i%7 == 5){
			for(int j=(root^1); j<(root^1)+7; j++){
				dv += remain[i]*remain[j]*bv[j]*rate[i%7];
			}
		}
		// i = 其他
		else{
			for(int j=(root^1); j<(root^1)+7; j++){
				if(GetLevel(FIN(i)) <= GetLevel(FIN(j)))	dv += remain[i]*remain[j]*bv[j]*rate[i%7];
			}
		}
	}
	
	//每顆敵方棋子 i 子力為「能吃到的所有棋子(明+暗) j 基本價值總和」
	int dv2 = 0;
	for(int i=(root^1)*7; i<(root^1)*7+7; i++){
		//printf("dv2= %d, i= %d\n", dv2, i);
		// i = 帥
		if(i%7 == 0){
			for(int j=(root); j<(root)+7; j++){
				if(0 <= j%7 && j%7 <= 5)	dv2 += remain[i]*remain[j]*bv[j]*rate[i%7];
			}
		}
		// i = 卒
		else if(i%7 == 6){
			for(int j=(root); j<(root)+7; j++){
				if(0 == j%7 || j%7 == 6)	dv2 += remain[i]*remain[j]*bv[j]*rate[i%7];
			}
		}
		// i = 砲
		else if(i%7 == 5){
			for(int j=(root); j<(root)+7; j++){
				dv2 += remain[i]*remain[j]*bv[j]*rate[i%7];
			}
		}
		// i = 其他
		else{
			for(int j=(root); j<(root)+7; j++){
				if(GetLevel(FIN(i)) <= GetLevel(FIN(j)))	dv2 += remain[i]*remain[j]*bv[j]*rate[i%7];
			}
		}
	}
	
	//for(int i=0;i<repeat; i++)	printf("  ");
	//printf("dv= %d, dv2= %d, return= %d", dv, dv2, dv-dv2);
	return dv-dv2;
}

//如果沒有這項，就無法把對方困死吃子
SCORE attack_value(const BOARD &B){
	int av = 0;
	for(POS p=0; p<32; p++){
		const CLR c = GetColor(B.fin[p]);
		if(c != (root^1))	continue;
		for(int i=0; i<4; i++){
			if(ADJ[p][i]==-1)	continue;
			//扣掉敵方、未翻、空格
			if(GetColor(B.fin[ADJ[p][i]]) != root)	continue;
			//砲相鄰時沒有攻擊力
			if(B.fin[ADJ[p][i]]%7 == 5)	continue;
			
			//檢查敵子是否會被被子力較大的我方所吃
			//敵子：將帥
			if(B.fin[p]==(root^1)*7 && B.fin[ADJ[p][i]] == root*7+6){
				av += BASIC*rate[B.fin[p]%7];
				break;
			}
			//敵子：其他
			if(B.fin[p]%7 != 0 && B.fin[p]%7>B.fin[ADJ[p][i]]%7){
				av += BASIC*rate[B.fin[p]%7];
				break;
			}
			
			//沒檢查敵子被我方砲吃
		}
	}
	for(int i=0; i<repeat; i++)	printf("  ");
	printf(", av= %d, ", av);
	return av;
}

SCORE depth_value(int dep){	
	for(int i=0; i<repeat; i++)	printf("  ");
	printf("depv= %d", dep*500);
	return dep*500;
}

// 審局函數
SCORE Eval(const BOARD &B, int dep) {
	int yy = basic_value(B) /*+ dynamic_value(B)*/ + depth_value(dep) + eeeat*1000000;
	if(eval_flag==0)	yy+=attack_value(B);
	for(int i=0; i<repeat;i++)	printf("  ");
	printf("Eval(B)= %d\n", yy);
	return yy;
}

// dep=現在在第幾層
// cut=還要再走幾層
SCORE SearchNega(const BOARD &B,int dep,int cut, SCORE alpha, SCORE beta) {
	if(B.ChkLose())return (dep%2 == 1)?		-WIN:+WIN;
	MOVLST lst;
	int ggg = B.MoveGen(lst);
	repeat = dep;
	if(cut==0||TimesUp()||ggg==0){
		//for(int i=0;i<dep;i++)	printf("  ");
		//printf("ggg= %d, cut= %d, timeup= %s\n", lst.num, cut, (TimesUp())? "TRUE":"FALSE");
		int ppp = (dep ==0)? 0:cut;
		return (dep%2)?	(-1)*Eval(B, ppp):Eval(B, ppp);
	}
	SCORE ret=-INF;
	for(int i=0;i<lst.num;i++) {
		for(int j=0;j<dep;j++)	printf("  ");
		printf("(%d:%d,%d)\n", i , (lst.mov[i]).st, (lst.mov[i]).ed);
		if(GetColor(B.fin[(lst.mov[i]).ed]) == root^1){
			eeeat=1;
			kind = B.fin[(lst.mov[i]).ed]%7;
		}
		BOARD N(B);
		N.Move(lst.mov[i]);
		SCORE tmp= -SearchNega(N,dep+1,cut-1, -beta, -ret);
		eeeat=0;
		if(dep == 0){
			for(int k =0; k <dep; k++)	printf("  ");
			printf("score= %.0lf\n", tmp);
		}
		if(tmp>ret){
			ret=tmp;
			if(dep==0)	BestMove=lst.mov[i];
		}
		else if(tmp == ret){
			if(rand()%2){
				ret=tmp;
				if(dep == 0)	BestMove=lst.mov[i];
			}
		}
		if(ret >= beta)	return ret;
	}
	return ret;
}

MOV Play(const BOARD &B) {
	Tick=clock();           
	TimeOut = (DEFAULTTIME-3)*1000; 
	POS p; int c=0;
	eval_flag = 0;
	
	// 新遊戲？隨機翻子
	if(B.who==-1){p=rand()%32;printf("%d\n",p);return MOV(p,p);}

	/*
	//判斷殘局
	endgame = 0;
	for(POS p=0; p<32; p++)		if(B.fin[p] == 14)	endgame++;
	*/
	
	// 若搜出來的結果會比現在好就用搜出來的走法
	clock_t start = clock();
	
	int ss = SearchNega(B,0,6,-INF, INF);
	clock_t end = clock();
	printf("SearchNega spend= %d (%d, %d)\n", end-start, start, end);
	eval_flag = 1;
	int ee = Eval(B, 0);
	printf("SearchNega= %d, Eval= %d, root= %d\n", ss, ee, root);
	if(ss > ee){
		printf("BestMove= (%d, %d)\n", BestMove.st, BestMove.ed);
		return BestMove;
	}

	// 否則隨便翻一個地方 但小心可能已經沒地方翻了
	for(p=0;p<32;p++)if(B.fin[p]==FIN_X)c++;
	if(c==0)return BestMove;
	c=rand()%c;
	for(p=0;p<32;p++)if(B.fin[p]==FIN_X&&--c<0)break;
	printf("WTFFFFFFFFFFFFFFF\n");
	return MOV(p,p);
}

FIN type2fin(int type) {
    switch(type) {
	case  1: return FIN_K;
	case  2: return FIN_G;
	case  3: return FIN_M;
	case  4: return FIN_R;
	case  5: return FIN_N;
	case  6: return FIN_C;
	case  7: return FIN_P;
	case  9: return FIN_k;
	case 10: return FIN_g;
	case 11: return FIN_m;
	case 12: return FIN_r;
	case 13: return FIN_n;
	case 14: return FIN_c;
	case 15: return FIN_p;
	default: return FIN_E;
    }
}
FIN chess2fin(char chess) {
    switch (chess) {
	case 'K': return FIN_K;
	case 'G': return FIN_G;
	case 'M': return FIN_M;
	case 'R': return FIN_R;
	case 'N': return FIN_N;
	case 'C': return FIN_C;
	case 'P': return FIN_P;
	case 'k': return FIN_k;
	case 'g': return FIN_g;
	case 'm': return FIN_m;
	case 'r': return FIN_r;
	case 'n': return FIN_n;
	case 'c': return FIN_c;
	case 'p': return FIN_p;
	default: return FIN_E;
    }
}

int main(int argc, char* argv[]) {
	
#ifdef _WINDOWS
	srand(Tick=GetTickCount());
#else
	srand(Tick=time(NULL));
#endif

	BOARD B;
	if (argc!=2) {
	    TimeOut=(B.LoadGame("board.txt")-3)*1000;
	    if(!B.ChkLose())Output(Play(B));
	    return 0;
	}
	
	
	Protocol *protocol;
	protocol = new Protocol();
	protocol->init_protocol(argv[0],atoi(argv[1]));
	int iPieceCount[14];
	char iCurrentPosition[32];
	int type, remain_time;
	bool turn;
	PROTO_CLR color;

	char src[3], dst[3], mov[6];
	History moveRecord;
	protocol->init_board(iPieceCount, iCurrentPosition, moveRecord, remain_time);
	protocol->get_turn(turn,color);

	TimeOut = (DEFAULTTIME-3)*1000;

	B.Init(iCurrentPosition, iPieceCount, (color==2)?(-1):(int)color);

	FILE *fd;
	MOV m;
	if(turn) // 我先
	{
	    m = Play(B);
	    sprintf(src, "%c%c",(m.st%4)+'a', m.st/4+'1');
	    sprintf(dst, "%c%c",(m.ed%4)+'a', m.ed/4+'1');
	    protocol->send(src, dst);
	    protocol->recv(mov, remain_time);
	    if( color == 2)
		color = protocol->get_color(mov);
	    B.who = color;
		printf("init: color= %d", B.who);
	    B.DoMove(m, chess2fin(mov[3]));
	    protocol->recv(mov, remain_time);
	    m.st = mov[0] - 'a' + (mov[1] - '1')*4;
	    m.ed = (mov[2]=='(')?m.st:(mov[3] - 'a' + (mov[4] - '1')*4);
	    B.DoMove(m, chess2fin(mov[3]));
	}
	else // 對方先
	{
	    protocol->recv(mov, remain_time);
	    if( color == 2)
	    {
		color = protocol->get_color(mov);
		B.who = color;
	    }
	    else {
		B.who = color;
		B.who^=1;
	    }
		
	    m.st = mov[0] - 'a' + (mov[1] - '1')*4;
	    m.ed = (mov[2]=='(')?m.st:(mov[3] - 'a' + (mov[4] - '1')*4);
	    B.DoMove(m, chess2fin(mov[3]));
	}
	root = B.who;
	//B.Display();
	while(1)
	{
		fd = freopen("PPP.txt", "a", stdout);
	    if(!fd)	printf("Open File Error.\n");
		m = Play(B);
		fclose(fd);
	    sprintf(src, "%c%c",(m.st%4)+'a', m.st/4+'1');
	    sprintf(dst, "%c%c",(m.ed%4)+'a', m.ed/4+'1');
	    protocol->send(src, dst);
	    protocol->recv(mov, remain_time);
	    m.st = mov[0] - 'a' + (mov[1] - '1')*4;
	    m.ed = (mov[2]=='(')?m.st:(mov[3] - 'a' + (mov[4] - '1')*4);
	    B.DoMove(m, chess2fin(mov[3]));
	    //B.Display();

	    protocol->recv(mov, remain_time);
	    m.st = mov[0] - 'a' + (mov[1] - '1')*4;
	    m.ed = (mov[2]=='(')?m.st:(mov[3] - 'a' + (mov[4] - '1')*4);
	    B.DoMove(m, chess2fin(mov[3]));
	    //B.Display();
		
	}

	fclose(fd);
	return 0;
}
