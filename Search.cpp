#include <windows.h>
#include <iostream>
using namespace std;
#include <stdio.h>
#include <string>

#include "Checkers.h"
#include "EdAccess.h"

#define CHECK_INTERVAL 50000

bool StopRequest;
bool AnalyseMode;
double PerftNodes;
double CheckNodes;
int EdRoot[3];

void Perft(unsigned depth)
{
	Move *old_MP = MP;
	GenerateAllMoves();
	--depth;
	for (Move *m = old_MP; m < MP; ++m)
	{
		MakeMove(m, 0);
		if (depth) Perft(depth);
		else ++PerftNodes;
		UnmakeMove(m);
	}
	MP = old_MP;
}

char **pv_str = 0;
// m  - ������� ���
// cm - ����� �������� ����
// tm - ����� �����
void PrintPV(int depth, int score, unsigned cm, unsigned tm, Move *m)
{
	int time = GetTimeElaps();

	if (pfSearchInfoEx)
	{
		char str[64];

		// ������������� ������� ��� �������� ������� ��������
		if (!pv_str)
		{
			pv_str = new char *[2];
			pv_str[0] = new char[1024];
			pv_str[1] = new char[1];
			pv_str[1][0] = 0;
		}
		
		// ������ �������
		pv_str[0][0] = 0;
		for (unsigned i = 0; PV[0][i].from && i < 6; i++)
		{
			MoveToStr(&PV[0][i], str);
			if (pv_str[0][0]) strcat(pv_str[0], " ");
			strcat(pv_str[0], str);
			if (strlen(pv_str[0]) > 900) break;
		}

		// ������� ���
		char cm_str[32];
		if (time > 1000)
		{
			itoa(cm + 1, cm_str, 10);
			strcat(cm_str, "/");
			itoa(tm, cm_str + strlen(cm_str), 10);
			strcat(cm_str, " ");
			MoveToStr(m, cm_str + strlen(cm_str));
		}
		else cm_str[0] = 0;

		// ��������
		int speed = 0;
		if (time > 1000) speed = int (Nodes / time);
		char speed_str[16];
		itoa(speed, speed_str, 10);

		char score_str[16];
		if (abs(score) > ED_WIN - 1000)
		{
			if (score > 0) score_str[0] = '+';
			else score_str[0] = '-';
			if (abs(score) > WIN - 1000)
			{
				score_str[1] = 'X';
				itoa((WIN - abs(score) + 1) / 2, score_str + 2, 10);
			}
			else
			{
				score_str[1] = 'D';
				itoa((ED_WIN - abs(score) + 1) / 2, score_str + 2, 10);
			}
		}
		else itoa(score, score_str, 10);
		char depth_str[16];
		itoa(depth, depth_str, 10);

		pfSearchInfoEx(score_str, depth_str, speed_str, pv_str, cm_str);
		return;
	}
	else if (pfSearchInfo)
	{
		char str[64];

		// ������ �������
		std::string pv_str;
		for (unsigned i = 0; PV[0][i].from; i++)
		{
			MoveToStr(&PV[0][i], str);
			if (pv_str != "") pv_str += " ";
			pv_str += str;
		}

		// ������� ���
		std::string cm_str;
		if (time > 1000)
		{
			cm_str = itoa(cm + 1, str, 10);
			cm_str += "/";
			cm_str += itoa(tm, str, 10);
			cm_str += " ";
			MoveToStr(m, str);
			cm_str += str;
		}

		// ��������
		int speed = 0;
		if (time > 1000) speed = int (Nodes / time);

		pfSearchInfo(score, depth, speed, (char *)pv_str.c_str(), (char *)cm_str.c_str());
		return;
	}

	// ����� � ���������� ������
	cout.width(2);
	cout << depth << ' ';
	cout.width(3);
	cout << score << ' ';
	cout.width(6);
	cout << time << ' ';
	cout.width(10);
	cout.setf(ios::fixed);
	cout << Nodes << ' ';	
	char move[128];
	for (int i = 0; PV[0][i].from; ++i)
	{
		MoveToStr(&PV[0][i], move);
		if (i) cout << ' ';
		cout << move;
	}
	cout << endl;
	cout.flush();
}

// ����������� ����� �� ������ �����
int TreeSearch(int depth, unsigned ply)
{
	// ���� ��� ������� ��������
	PV[ply][ply].from = 0;

	if (ply >= MAX_DEPTH) return Eval();

	// ����� �� ������ ��������� �����?
	if (Nodes >= CheckNodes)
	{
		CheckNodes = Nodes + CHECK_INTERVAL;
		if (!AnalyseMode && CheckTime())
		{
			StopRequest = true;
			return 0;
		}
	}

	// ������ � ����������� ����
	if (!EdNocaptures && Pieces <= EdPieces)
	{
		int res = EdProbe();
		if (res != EdAccess::not_found)
		{
			if (res != EdRoot[stm] || !Reversible[ply - 1])
			{
				if (res == EdAccess::win)  return ED_WIN - ply;
				if (res == EdAccess::lose) return -ED_WIN + ply;
				if (res == EdAccess::draw) return 0;
				MessageBox(0, "unknown value from EdAccess", "Error", 0);
			}
		}
	}

	// ��������� �����
	Move *old_MP = MP;
	if (depth > 0) GenerateAllMoves();
	else
	{
		// �� ��������������� �� ����������, ���� ���� ������
		GenerateCaptures();
		if (MP == old_MP) return Eval();
	}

	// ������ � ����������� ����, ������ � ������� ������� �� ��������
	if (EdNocaptures && Pieces <= EdPieces && !old_MP->cap_sq[0])
	{
		int res = EdProbe();
		if (res != EdAccess::not_found)
		{
			if (res != EdRoot[stm] || !Reversible[ply - 1])
			{
				if (res == EdAccess::win)  return ED_WIN - ply;
				if (res == EdAccess::lose) return -ED_WIN + ply;
				if (res == EdAccess::draw) return 0;
				MessageBox(0, "unknown value from EdAccess", "Error", 0);
			}
		}
	}

	// ����������� �����
	int best_score = -WIN + ply;
	for (Move *m = old_MP; m < MP; ++m)
	{
		MakeMove(m, ply);
		int score = -TreeSearch(depth - 1, ply + 1);
		UnmakeMove(m);

		if (StopRequest) break;

		if (score > best_score)
		{
			best_score = score;

			// ��������� ������ �������
			PV[ply][ply] = *m;
			int i;
			for (i = ply + 1; PV[ply + 1][i].from; i++)
				PV[ply][i] = PV[ply + 1][i];
			PV[ply][i].from = 0;
		}
	}

	MP = old_MP;
	return best_score;
}

// ����� � ����� ������ �����
Move RootSearch()
{
	StartTimer();

	StopRequest = false;

	Nodes = 0;
	CheckNodes = CHECK_INTERVAL;

	// ��������� �����
	MP = MoveBuffer;
	GenerateAllMoves();
	if (MP == MoveBuffer) return *MP;
	if (!AnalyseMode && MP == MoveBuffer + 1)
	{
		// ������������ ��� ���������� �����
		MP = MoveBuffer;
		return *MP;
	}

	if (!EdNocaptures || !MoveBuffer[0].cap_sq[0]) EdRoot[stm] = EdProbe();
	else EdRoot[stm] = EdAccess::not_found;
	if (EdRoot[stm] == EdAccess::win) EdRoot[stm ^ CHANGE_COLOR] = EdAccess::lose;
	else if (EdRoot[stm] == EdAccess::lose) EdRoot[stm ^ CHANGE_COLOR] = EdAccess::win;
	else EdRoot[stm ^ CHANGE_COLOR] = EdRoot[stm];

	int last_score = 0;

	// ����������� ����������
	for (int depth = 1; depth < MAX_DEPTH; depth++)
	{
		int best_score = -WIN;

		// ��� ������� ���� �������� �����
		for (Move *m = MoveBuffer; m < MP; m++)
		{
			PrintPV(depth, last_score, m - MoveBuffer, MP - MoveBuffer, m);

			MakeMove(m, 0);
			int score = -TreeSearch(depth - 1, 1);
			UnmakeMove(m);

			if (StopRequest) break;

			if (score > best_score)
			{
				// ����� ������ ���
				best_score = score;

				// ��������� ������ �������
				PV[0][0] = *m;
				unsigned i;
				for (i = 1; PV[1][i].from; i++)
					PV[0][i] = PV[1][i];
				PV[0][i].from = 0;

				// ���������� ��� � ������ ������ �����
				Move best = *m;
				for (Move *pm = m; pm > MoveBuffer; pm--) *pm = *(pm - 1);
				*MoveBuffer = best;
			}
			PrintPV(depth, last_score = best_score, m - MoveBuffer, MP - MoveBuffer, m);
		}

		if (StopRequest) break;
	}

	MP = MoveBuffer;
	return PV[0][0];
}
