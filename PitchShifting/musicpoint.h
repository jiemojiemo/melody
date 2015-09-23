#pragma once
#include <vector>
//音符，
struct Note {
	int frequnce;	//频率
	float time;		//时长
	float velocity;	//音量
};

//节奏点。这个点有具体的歌词，以及需要的音高等等
struct MusicPoint {
	Note note;		//音符
	int lyricIndex;	//歌词下标
};





void CreateMusicPoints(std::vector<MusicPoint>& musicPoints);