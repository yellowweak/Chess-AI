#include<cstdio>
#include<cstdlib>
#include<windows.h>
#include<strings.h>
#include<time.h>
#include"anqi.hh"
#include"Protocol.h"
#include"ClientSocket.h"
#define BASIC 4000
int step=2;
int endgame;

const int rate[7] = {50, 30, 20, 10, 5, 25, 2};

const int DEFAULTTIME = 18;
typedef  int SCORE;
static const SCORE INF=1000001;
static const SCORE WIN=1000000;
SCORE SearchNega(const BOARD&,int,int,int,int);
SCORE basic_value(const BOARD &B);
SCORE dynamic_value(const BOARD &B);
SCORE threaten(const BOARD &B);


DWORD Tick;     // 開始時刻
int   TimeOut;  // 時限
MOV   BestMove; // 搜出來的最佳著法
int   root;		// search tree裡root的顏色

bool TimesUp() {
	return GetTickCount()-Tick>=TimeOut;
}

SCORE basic_value(const BOARD &B){
	int t=0;
	for(POS p=0; p<32; p++){
		const CLR c = GetColor(B.fin[p]);
		if(c != -1 && c == root)	t+=BASIC;
	}
	for(int i=root*7; i<root*7+7; i++)		t += BASIC*B.cnt[i];
	
	int t2=0;
	for(POS p=0; p<32; p++){
		const CLR c = GetColor(B.fin[p]);
		if(c != -1 && c == (root^1))	t2+=BASIC;
	}
	for(int i=(root^1)*7; i<(root^1)*7+7; i++)		t2 += BASIC*B.cnt[i];
	
	//printf("t= %d, t2= %d\n", t, t2);
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
	
	//printf("dv= %d, dv2= %d\n", dv, dv2);
	return dv-dv2;
}

SCORE threaten(const BOARD &B){

}

// 審局函數
SCORE Eval(const BOARD &B) {
	return basic_value(B) + dynamic_value(B);
}

// dep=現在在第幾層
// cut=還要再走幾層
SCORE SearchNega(const BOARD &B,int dep,int cut, int alpha, int beta) {
	if(B.ChkLose())return (dep%2 == 1)?		-WIN:+WIN;

	MOVLST lst;
	if(cut==0||TimesUp()||B.MoveGen(lst)==0){
		return (dep%2)?	(-1)*Eval(B):Eval(B);
	}

	SCORE ret=alpha;
	for(int i=0;i<lst.num;i++) {
		//printf("(%d:%d,%d)\n", i , (lst.mov[i]).st, (lst.mov[i]).ed);
		BOARD N(B);
		N.Move(lst.mov[i]);
		const SCORE tmp= -SearchNega(N,dep+1,cut-1, -beta, -ret);
		//if(dep == 0)	printf("score= %d\n", tmp);
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
	Tick=GetTickCount();            
	TimeOut = (DEFAULTTIME-3)*1000; 
	POS p; int c=0;
	
	// 新遊戲？隨機翻子
	if(B.who==-1){p=rand()%32;printf("%d\n",p);return MOV(p,p);}
	
	/*
	//判斷殘局
	endgame = 0;
	for(POS p=0; p<32; p++)		if(B.fin[p] == 14)	endgame++;
	*/
	
	// 若搜出來的結果會比現在好就用搜出來的走法
	clock_t start = clock();
	int ss = SearchNega(B,0,10,-INF, INF);
	clock_t end = clock();
	printf("SearchNega spend= %d (%d, %d)\n", end-start, start, end);
	int ee = Eval(B);
	printf("SearchNega= %d, Eval= %d\n", ss, ee);
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


int main(int argc, char* argv[]) {

	srand(Tick=GetTickCount());

	BOARD B;
	if (argc!=3) {
	    TimeOut=(B.LoadGame("board.txt")-3)*1000;
	    if(!B.ChkLose())Output(Play(B));
	    return 0;
	}
	Protocol *protocol; 
	if (argc == 2)	protocol = new Protocol(argv[0], atoi(argv[1]));
	else			protocol = new Protocol(argv[1], atoi(argv[2])); 
	int iPieceCount[14], iCurrentPosition[32], turn, color, type;

	DarkChessPoint src, dst; 
	Record moveRecord; 
	protocol->init_board(iPieceCount, iCurrentPosition, moveRecord);     
	protocol->get_turn(turn,color);           
	
	TimeOut = (DEFAULTTIME-3)*1000;

	B.Init(iCurrentPosition, iPieceCount, (color==2)?(-1):color);
	
	MOV m;
	if(turn) // 我先
	{
	    m = Play(B);
	    src.x = m.st%4; src.y = m.st/4;
	    dst.x = m.ed%4; dst.y = m.ed/4;
	    protocol->send(src, dst);         
	    protocol->recv(src, dst, type);
	    if( color == 2 && type == 1 )
	    {
		color = (dst.x/8);
	    }
		B.who = color;
		root = B.who;
	    B.DoMove(m, type2fin(dst.x));
	    protocol->recv(src, dst,type);
	    m.st = src.x + src.y*4;
	    m.ed = (type==0)?(dst.x + dst.y*4):m.st;
	    B.DoMove(m, type2fin(dst.x));
	}
	else // 對方先
	{ 
	    protocol->recv(src, dst, type); 
	    if( color == 2 && type == 1 ) 
	    {
		color = !(dst.x/8);
	    }
		B.who = !color;
		root = (B.who^1);
	    m.st = src.x + src.y*4;
	    m.ed = (type==0)?(dst.x + dst.y*4):m.st;
	    B.DoMove(m, type2fin(dst.x));
	}
	while(1)
	{  
		step++;
	    m = Play(B);
	    src.x = m.st%4; src.y = m.st/4;
	    dst.x = m.ed%4; dst.y = m.ed/4;
		//printf("src= (%d, %d), dst= (%d, %d)\n", src.x, src.y, dst.x, dst.y);
	    protocol->send(src, dst);    
	    protocol->recv(src, dst,type);  
	    B.DoMove(m, type2fin(dst.x));

	    protocol->recv(src, dst,type);
	    m.st = src.x + src.y*4;
	    m.ed = (type==0)?(dst.x + dst.y*4):m.st;
	    B.DoMove(m, type2fin(dst.x));
	}  

	return 0;
}
