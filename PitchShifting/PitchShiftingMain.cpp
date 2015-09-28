#include "pitchshifting.h"
#include "pcm2wav.h"
#include "voicedetection.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include "CWavread.h"
#include "music.h"
using namespace Imyours;
using namespace std;


//获取需要音频数据，用于人声片段的检�?
short* GetDataForDetection(int& sampleCount, int& sampleRate);
//�?wav 文件获取数据，只能用于简单情况的wav文件（即真正音频数据�?x2c位置开始）
short* GetDataForDetectionFromWavFile(const string& fileName, int& sampleCount, int& sampleRate);
//通过录音获取数据
short* GetDataForDetectionFromRecord(int& sampleCount, int& sampleRate);
int main()
{
	int sampleCount = 0;
	int sampleRate = 0;
	short* buffer = GetDataForDetection( sampleCount, sampleRate);

	////�������

	//CVoiceDetection detection;
	//auto segments( detection.Detection(buffer,sampleCount,sampleRate) );
	//for (auto& i : segments)
	//	cout << i.start*detection.m_hop << " " << i.end*detection.m_hop << endl;


	float dst_freq = 260;
	float timeScale = 0.0;
	CPitchShifting pitch;
	vector<short> finalOutBuffer;
	vector<pair< unsigned long, unsigned long>> voicePositionInSapmple;

	for (int i = 0; i < voiceCnt; i = i + 2)
		voicePositionInSapmple.push_back(pair<unsigned long, unsigned long>(voicePosition[i]+1500, voicePosition[i + 1]+1500));


	for (int i = 0; i < noteCnt; i++)
	{

		if (lyric[i] == 0)//������Ϊ�գ�������һ�ξ�����

		{
			unsigned long quiteSize = musicTime[i] * 44100 / 24*0.95;

			for (int k = 0; k < quiteSize; k++)
				finalOutBuffer.push_back(0);
		//	cout << finalOutBuffer.size() << "  ";
		}
		else
		{
			Note note;
			note.frequnce = 130 * pow(1.0594, (double)(musicPitch[i] - 60));
			note.time = (musicTime[i]) / 24.0*0.95;
			//if (i < 16)
			//{
			//	note.time *= 0.93;
			//}
			note.velocity = 1;
			note.lyric = lyric[i] - 1;
			cout << "lyric:" << note.lyric << "  time:" << note.time << "  freq:" << note.frequnce;
			//字在整个语音段的位置
			unsigned long  distance = voicePositionInSapmple[note.lyric].first;
			//字的长度，即字的sample的个�?
			int length = (voicePositionInSapmple[note.lyric].second - voicePositionInSapmple[note.lyric].first);
			auto dataResult = pitch.TimeScalingAndPitchShifting(note, buffer+distance, length);

			for (int k = 0; k < pitch.GetFinalSampleCount(); k++)
				finalOutBuffer.push_back(dataResult[k]);
			for (int kk = pitch.GetFinalSampleCount();kk < note.time * 44100;kk++)
				finalOutBuffer.push_back(0);
		//	cout << finalOutBuffer.size() << "  ";



		}

	}

	CPcm2Wav convert(&finalOutBuffer[0], finalOutBuffer.size()*sizeof(short), "demo1out.wav");
	Pcm2WavParameter params;
	params.channels = 1;
	params.formatTag = 1;
	params.sampleBits = 16;
	params.sampleRate = 44100;
	convert.Pcm2Wav(params);
}


//************************************
// Method:    GetDataForDetection
// Parameter@ sampleCount: 
// Parameter@ sampleRate: 
// Returns short*:数据内存的指�?
// Comment:   
// Creator:	  HW
// Modifier:  
//************************************
short* GetDataForDetection(int& sampleCount, int& sampleRate)
{
	//return GetDataForDetectionFromRecord(sampleCount, sampleRate);
	return GetDataForDetectionFromWavFile("demo2.wav", sampleCount, sampleRate);
}

//************************************
// Method:    GetDataForDetectionFromWavFile
// Parameter@ fileName: wav文件�?
// Returns short*:   
// Comment:   暂定只能�?16bit 单声道wav文件
// Creator:	  HW
// Modifier:  
//************************************
short* GetDataForDetectionFromWavFile(const string& fileName, int& sampleCount, int& sampleRate)
{
	FILE* fd = fopen(fileName.c_str(), "rb");
	if (fd == nullptr)
	{
		fprintf(stderr, "Cannot open the file!\n");
		exit(-1);
	}
	fseek(fd, 0, SEEK_END);
	unsigned long fileSize = ftell(fd) - 0x2c;
	fseek(fd, 0x2c, SEEK_SET);
	short* data = (short*)malloc(fileSize);
	fread(data, 1, fileSize, fd);
	fclose(fd);
	sampleRate = 44100;
	sampleCount = fileSize / sizeof(short);
	return data;
}


//************************************
// Method:    GetDataForDetectionFromRecord
// Returns:   float*
// Comment:   录音暂时定为 16bit 单声�?44.1hz，因为使用其他参数的录音，后期处理有一些问�?
// Creator:	  HW
// Modifier:  
//************************************
short* GetDataForDetectionFromRecord(int& sampleCount, int& sampleRate)
{
	////设置录音的参�?暂时定为 16bit 单声�?44.1hz
	//RecordParameters recordParams;
	//recordParams.audioFormat = AUDIO_SIN16;
	//recordParams.framesPerBuffer = 512;
	//recordParams.nChannels = 1;
	//recordParams.nSeconds = 5;
	//recordParams.sampleRate = 44100;

	////开始录�?
	//CRecord<short> recorder(recordParams);
	//recorder.Start();

	////
	//sampleCount = recorder.GetTotalSamples();
	//sampleRate = recordParams.sampleRate;

	////short数据 --> float数据,后期处理需要用float数据进行
	//float* buffer = Bit16toBit32(recorder.GetDataPointer(), recorder.GetTotalSamples());
	//return buffer;
	return nullptr;
}
